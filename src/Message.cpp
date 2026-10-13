/*

  Message.cpp


*/

#include "Message.h"
#include "Centrale.h"
#include "MfxFrame.h"

Loco **Message::loco = nullptr;
byte Message::locoSize = 0;

bool Message::trace = false;
bool Message::traceCommands = false;

uint32_t Message::id = 0;
uint8_t Message::length = 0;
uint8_t Message::prio = 0;
uint8_t Message::cmd = 0;
uint8_t Message::response = 0;
uint16_t Message::hash = 0;
uint8_t Message::data[8] = {0};
byte Message::buff[BUFF_MSG_SIZE] = {0};

SemaphoreHandle_t taskSemaphore;

void Message::setup(Loco *locoArray[], byte locoCount)
{
    loco = locoArray;
    locoSize = locoCount;
    trace = false;

    taskSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(taskSemaphore); // Libère le sémaphore pour la première tâche

    xTaskCreatePinnedToCore(periodic, "Periodic", 2 * 1024, (void *)loco, 6, NULL, 0);
    xTaskCreatePinnedToCore(centrale, "Centrale", 2 * 1024, (void *)loco, 5, NULL, 0);
    xTaskCreatePinnedToCore(setSID, "SetSID", 2 * 1024, (void *)loco, 5, NULL, 0);
}

/*
 Drive + functions F0..F15 of a locomotive (Krauss 3.1), in the format of
 mfxQueue. Used by the periodic refresh and by sendNow().
*/
void Message::driveFrame(Loco *l, byte *out, size_t outSize)
{
    uint16_t functions = 0;
    for (byte n = 0; n < 16; n++)
        if (l->funct(n))
            functions |= (uint16_t)(1u << n);
    MfxFrame frame;
    frame.address(l->addr());
    frame.drive(l->dir() != 0, l->speed());
    frame.functions16(functions);
    frame.finish();
    frame.toBuffer(out, outSize);
}

/*
 A command from Rocrail is sent at once, at the head of the queue, instead
 of waiting for the next refresh (up to 50 ms). The refresh repeats it.
*/
void Message::sendNow(Loco *l)
{
    byte frame[BUFF_MSG_SIZE];
    driveFrame(l, frame, sizeof(frame));
    xQueueSendToFront(mfxQueue, frame, 0);
}

void Message::periodic(void *p)
{
    Loco **loco;
    loco = (Loco **)p;
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();

    // byte buff[BUFF_MSG_SIZE] = {0};

    for (;;)
    {
        for (byte i = 0; i < locoSize; i++)
        {
            if (xSemaphoreTake(taskSemaphore, portMAX_DELAY) == pdTRUE)
            {
                if (loco[i] != nullptr)
                {
                    if (trace)
                        Serial.printf("Periodic- %s\n", loco[i]->name().c_str());

                    driveFrame(loco[i], buff, sizeof(buff));

                    xQueueSend(mfxQueue, &buff, 0);
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            xSemaphoreGive(taskSemaphore);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}

void Message::centrale(void *p)
{
    /*
 3.2.6 Commande 111 101 : centrale
 111 101 UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU ZZZZZZZZZZZZZZZZ
 La centrale envoie cette commande à intervalles réguliers avec l'adresse de diffusion (broadcast) 0 indiquant ainsi son UID (U) et le compteur de nouvelles inscriptions (Z).
 */

    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(taskSemaphore, portMAX_DELAY) == pdTRUE)
        {
            if (trace)
                Serial.println("Centrale");

            // Broadcast: command station UID and re-registration counter.
            // The counter only changes on explicit request: a decoder that
            // sees it change registers again.
            MfxFrame frame;
            frame.broadcast();
            frame.centrale(Centrale::gUid(), Centrale::counter());
            frame.finish();
            frame.toBuffer(buff, sizeof(buff));
            xQueueSend(mfxQueue, &buff, 0);
            xSemaphoreGive(taskSemaphore);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

void Message::setSID(void *p)
{
    Loco **loco;
    loco = (Loco **)p;
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        if (xSemaphoreTake(taskSemaphore, portMAX_DELAY) == pdTRUE)
        {
            for (byte i = 0; i < locoSize; i++)
            {
                if (loco[i] != nullptr)
                {

                    if (trace)
                        Serial.printf("setSID- %s\n", loco[i]->name().c_str());

                    /*
                    Cette commande est envoyée à l'adresse de diffusion 0, puisque le décodeur ne connaît pas encore le SID.
                    Le décodeur passe ainsi en mode de fonctionnement avec l'UID correspondant et peut ensuite être adressé sous ce SID.
                    */
                    const uint32_t uid = (uint32_t)loco[i]->UID(0) << 24 | (uint32_t)loco[i]->UID(1) << 16 |
                                         (uint32_t)loco[i]->UID(2) << 8 | loco[i]->UID(3);
                    MfxFrame frame;
                    frame.broadcast();
                    frame.bind(loco[i]->addr(), uid);
                    frame.finish();
                    frame.toBuffer(buff, sizeof(buff));
                    xQueueSend(mfxQueue, &buff, 0);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
            xSemaphoreGive(taskSemaphore);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

void Message::decodeMsg(byte *rxBuffer)
{
    id = (uint32_t)rxBuffer[0] << 24 | (uint32_t)rxBuffer[1] << 16 | (uint32_t)rxBuffer[2] << 8 | rxBuffer[3];

    length = rxBuffer[4];
    if (length > 8)
        length = 8;
    prio = (id & 0x1E000000) >> 25;
    cmd = (id & 0x1FE0000) >> 17;
    response = (id & 0x10000) >> 16;
    hash = id & 0xFFFF;

    // On efface data[] : sinon une trame courte (une requête) garde les
    // octets de la trame précédente, qui sont alors pris pour un ordre.
    memset(data, 0, sizeof(data));
    for (byte i = 0; i < length; i++)
        data[i] = rxBuffer[i + 5];
}

/*
 Les trames CS2 se distinguent par leur longueur :
   vitesse   : 4 octets = requête, 6 octets = ordre
   direction : 4 octets = requête, 5 octets = ordre
   fonction  : 5 octets = requête, 6 octets = ordre
 Les requêtes sont ignorées pour l'instant (les réponses à Rocrail viendront
 à l'étape 3) : surtout, elles ne modifient plus l'état d'une loco.
*/
void Message::parse()
{
    if (response) // écho ou réponse d'un autre appareil : rien à exécuter
        return;

    if (cmd == CAN_SYSTEM_CONTROL)
    {
        if (length < 5)
            return;
        if (traceCommands)
            Serial.printf("cmd systeme, sous-commande %u\n", data[4]);
        switch (data[4]) // sous-commande
        {
        case 0x00: // Arrêt du système
            Centrale::setPower(false);
            break;
        case 0x01: // Démarrage du système (seul moyen de sortir d'une surcharge)
            Centrale::setPower(true);
            break;
        case 0x02: // System Halt : à implémenter (étape 3)
            break;
        case 0x03: // Arrêt d'urgence de la locomotive : à implémenter (étape 3)
            break;
        }
        return;
    }

    if (cmd < CAN_LOCO_SPEED || cmd > CAN_LOCO_FUNCT || length < 4)
        return;

    const uint8_t address = data[3];
    Loco *currentLoco = Loco::findLoco(address);
    if (currentLoco == nullptr)
    {
        Serial.printf("Loco inconnue : adresse %u, commande ignorée\n", address);
        return;
    }

    switch (cmd)
    {
    case CAN_LOCO_SPEED:
        if (length >= 6)
        {
            uint16_t tempSpeed = (data[4] << 8) | data[5];
            if (tempSpeed > 1000)
                tempSpeed = 1000;
            tempSpeed = map(tempSpeed, 0, 1000, 0, 127);
            if (tempSpeed == 1) // 1 = arrêt d'urgence en MFX
                tempSpeed = 0;
            currentLoco->speed(tempSpeed);
        }
        break;

    case CAN_LOCO_DIREC:
        if (length >= 5)
        {
            // CS2 : 0 = garder, 1 = avant, 2 = arrière, 3 = inverser
            const uint8_t before = currentLoco->dir();
            switch (data[4])
            {
            case 1:
                currentLoco->dir(0);
                break;
            case 2:
                currentLoco->dir(1);
                break;
            case 3:
                currentLoco->toggleDir();
                break;
            }
            // Comme une CS2 : un changement de direction arrête la loco.
            if (currentLoco->dir() != before)
                currentLoco->speed(0);
        }
        break;

    case CAN_LOCO_FUNCT:
        if (length >= 6)
            currentLoco->funct(data[4], data[5] != 0);
        break;
    }

    // An order (not a request) changed the locomotive: send it now
    const bool order = (cmd == CAN_LOCO_SPEED && length >= 6) ||
                       (cmd == CAN_LOCO_DIREC && length >= 5) ||
                       (cmd == CAN_LOCO_FUNCT && length >= 6);
    if (order)
        sendNow(currentLoco);

    if (traceCommands)
    {
        Serial.printf("cmd %u, loco %s (adresse %u) : vitesse %u, direction %u\n",
                      cmd, currentLoco->name().c_str(), currentLoco->addr(),
                      currentLoco->speed(), currentLoco->dir());
    }
}
