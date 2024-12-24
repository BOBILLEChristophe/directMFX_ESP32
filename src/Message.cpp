/*

  Message.cpp


*/

#include "Message.h"

Loco **Message::loco = nullptr;
byte Message::locoSize = 0;

bool Message::trace = false;
byte Message::len = 0;
long Message::TZ, TP, T_S88, Time = 0;

uint32_t Message::id = 0;
uint8_t Message::length = 0;
uint8_t Message::prio = 0;
uint8_t Message::cmd = 0;
uint8_t Message::response = 0;
uint16_t Message::hash = 0;
uint8_t Message::data[8] = {0};
byte Message::buff[BUFF_MSG_SIZE] = {0};
uint16_t Message::crc = 0;
uint8_t data[8] = {0};

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
                        Serial.printf("Periodic- %s\n", loco[i]->name());

                    len = 0;
                    // adresse loco (7 bits)
                    addr(loco[i]->addr());
                    // commande 001: Conduite 001 R SSSSSSS
                    buff[++len] = 0;
                    buff[++len] = 0;
                    buff[++len] = 1;
                    //  direction
                    buff[++len] = loco[i]->dir() ? 1 : 0; //
                    // vitesse
                    for (byte a = 0, b = 6; a < 7; a++, b--)
                        buff[++len] = (loco[i]->speed() & (1 << b)) >> b;
                    // commande 0111:  Fonctions F15-F0 (MSB)
                    buff[++len] = 0;
                    buff[++len] = 1;
                    buff[++len] = 1;
                    buff[++len] = 1;

                    for (byte a = 0, b = 15; a < 16; a++, b--)
                        buff[++len] = loco[i]->funct(b);

                    buff[0] = len; // Length in FIRST BYTE
                    CRC();

                    // Serial.println("Message periodic");
                    // for (auto el : buff)
                    //     Serial.print(el);
                    // Serial.println();

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
 111 101 UUUUUUUUUUUUUUUUUUUUUUU ZZZZZZZZZZ
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

            len = 0;
            addr0();

            // Commande 0x3D (111 101)
            for (byte a = 0, b = 5; a < 6; a++, b--)
                buff[++len] = (0x3D & (1 << b)) >> b;

            // Centrale UID (32 bit)
            for (byte a = 0, b = 31; a < 32; a++, b--)
                buff[++len] = (Centrale::gUid() & (1 << b)) >> b;

            // Compteur (16 bits)
            buff[++len] = 1;
            buff[++len] = 0;
            buff[++len] = 0;
            buff[++len] = 0;
            buff[++len] = 0;
            buff[++len] = 0;
            buff[++len] = 0;
            buff[++len] = 0;

            for (byte a = 0, b = 7; a < 8; a++, b--)
                buff[++len] = ((Loco::zahler + 1) & (1 << b)) >> b;

            buff[0] = len; // Length in FIRST BYTE
            CRC();
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
                        Serial.printf("setSID- %s\n", loco[i]->name());

                    len = 0;
                    /*
                    Cette commande est envoyée à l'adresse de diffusion 0, puisque le décodeur ne connaît pas encore le SID.
                    Le décodeur passe ainsi en mode de fonctionnement avec l'UID correspondant et peut ensuite être adressé sous ce SID.
                    */
                    addr0();

                    //   111 011 AAAAAAAAAAAAAA UID
                    //******************** fonction 0x3B (111 011) ****************************** */

                    for (byte a = 0, b = 5; a < 6; a++, b--)
                        buff[++len] = (0x3B & (1 << b)) >> b;

                    // Adresse sur 14 bits
                    //******************* adresse loco 7 premiers bits a zero ******************* */

                    for (byte a = 0; a < 7; a++)
                        buff[++len] = 0;

                    //******************* adresse loco 7 derniers bits ******************* */

                    for (byte a = 0, b = 6; a < 7; a++, b--)
                        buff[++len] = (loco[i]->addr() & (1 << b)) >> b;

                    //******************* UID loco *************************************** */

                    for (byte j = 0; j < 4; j++)
                    {
                        for (byte a = 0, b = 7; a < 8; a++, b--)
                            buff[++len] = (loco[i]->UID(j) & (1 << b)) >> b;
                    }

                    //******************************************************************** */
                    buff[0] = len; // Length in FIRST BYTE
                    CRC();
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

    /*
    Debug du message reçu par TCP
    */
    // for (byte i = 0; i < 13; i++) {
    //   Serial.print(rxBuffer[i], HEX);
    //   Serial.print(" - ");
    // }
    // Serial.println();

    id = rxBuffer[0] << 24 | rxBuffer[1] << 16 | rxBuffer[2] << 8 | rxBuffer[3];

    length = rxBuffer[4];
    prio = (id & 0x1E000000) >> 25;
    cmd = (id & 0x1FE0000) >> 17;
    response = (id & 0x10000) >> 16;
    hash = id & 0xFFFF;

    /*
       Debug du message reçu par TCP
       */
    // Serial.printf("Commande : 0x");
    // Serial.println(cmd, HEX);
    // Serial.printf("Data length : %d\n", length);

    for (byte i = 0; i < length; i++)
    {
        data[i] = rxBuffer[i + 5];
        /*
           Debug du message reçu par TCP
           */
        // Serial.printf("data[%d] : ", i);
        // Serial.print("Ox");
        // Serial.print(data[i], HEX);
        // if (i < length - 1)
        //     Serial.print(" - ");
    }
    // Serial.println("");
}

void Message::parse()
{
    uint8_t address = 0;
    Loco *currentLoco = nullptr;
    if (cmd == 0X00) // Commandes système
    {
        byte ssCmd = data[4];
        switch (ssCmd)
        {
        case 0x00: // Arrêt du système
            Centrale::setPower(false);
            break;
        case 0x01: // Démarrage du système
            Centrale::setPower(true);
            //MFXWaveform::setStateMachine(10);
            break;
        case 0x02: // System Halt

            break;
        case 0x03: // Arrêt d'urgence de la locomotive

            break;
        }
    }
    else if (cmd >= CAN_LOCO_SPEED && cmd <= CAN_LOCO_FUNCT)
    {
        static std::vector<Loco> locos;
        address = data[3];
        currentLoco = Loco::findLoco(address);
        Serial.print("cmd : ");
        Serial.println(cmd);

        uint16_t tempSpeed = 0;
        switch (cmd)
        {
        case CAN_LOCO_SPEED:
            tempSpeed = (data[4] << 8) | data[5];
            if (tempSpeed > 1000)
                tempSpeed = 1000;
            tempSpeed = map(tempSpeed, 0, 1000, 0, 127);
            if (tempSpeed == 1)
                tempSpeed = 0;
            currentLoco->speed(tempSpeed);
            break;
        case CAN_LOCO_DIREC: // 1 = avant 2 = arriere
            currentLoco->dir(data[4] - 1);
            break;
        case CAN_LOCO_FUNCT:
            currentLoco->funct(data[4], data[5]);
            break;
        }

        Serial.printf("Loco name      : %s\n", currentLoco->name());
        Serial.printf("loco address   : %d\n", currentLoco->addr());
        Serial.printf("loco speed     : %d\n", currentLoco->speed());
        Serial.printf("loco direction : %d\n", currentLoco->dir());
        // Serial.printf("loco light     : %d\n", currentLoco->funct(0));
    }
}

void Message::addr0() // Broadcast
{
    buff[1] = 1;
    buff[2] = 0;
    len = 2;

    for (byte a = 0; a < 7; a++) // 7 bits address
        buff[++len] = 0;

} // always 7 bit adr (buff[0] used for LENGTH)

void Message::addr(byte address)
{
    /*
    Krauss 2.2.4 Structure des trames de données

    10 AAAAAAA : adresse de 7 bits
    110 AAAAAAAAA : adresse de 9 bits
    1110 AAAAAAAAAAA : adresse de 11 bits
    1111 AAAAAAAAAAAAAA : adresse de 14 bits
    */

    buff[1] = 1;
    buff[2] = 0;
    len = 2;

    for (byte a = 0, b = 6; a < 7; a++, b--) // 7 bits address
        buff[++len] = (address & (1 << b)) >> b;
}

void Message::CRC()
{ // avoid to compute in interrupt ! --> easier for bit stuffing !
    byte a, b;
    crc = 0x007F;
    for (a = 1; a < buff[0] + 1; a++)
        bCRC(buff[a]); // CRC
    for (a = 0; a < 8; a++)
        bCRC(0); // Krauss p13 "diese bit mÃƒÂ¼ssen zuerst mit 0 belegt ..."}
    b = 8;
    for (a = 0; a < 8; a++, b--)
        buff[len + 1 + a] = bitRead(crc, b - 1);
    buff[0] += 8; // Length
}

void Message::bCRC(bool b)
{ // Krauss p13
    crc = (crc << 1) + b;
    if ((crc & 0x0100) > 0)
        crc = (crc & 0x00FF) ^ 0x07;
}
