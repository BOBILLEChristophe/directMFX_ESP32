/*

  MFXWaveform.cpp


*/

#include "MFXWaveform.h"

bool MFXWaveform::m_polarity = false;
byte MFXWaveform::step = 0;
volatile byte MFXWaveform::nSync = 0;
volatile byte MFXWaveform::stuffCount = 0;
byte MFXWaveform::buff[BUFFER_SIZE] = {0};
volatile byte MFXWaveform::stateMachine = 0;
volatile bool MFXWaveform::receivedMsg = false;
bool MFXWaveform::bitVal = 0;
byte MFXWaveform::bitIdx = 0;
byte MFXWaveform::leng = 0;

void MFXWaveform::setStateMachine(byte state)
{
    stateMachine = state;
}

// Tâche de réception de données
void MFXWaveform::receiverTask(void *parameter)
{
    Serial.printf("receiverTask running on core %d\n", xPortGetCoreID());
    byte receivedBuff[BUFFER_SIZE]; // Message reçu
    while (true)
    {
        // Lecture du message dans la queue
        if (xQueueReceive(mfxQueue, (void *)receivedBuff, (TickType_t)10) == pdPASS)
        {
            // if (xSemaphoreTake(buffSemaphore, portMAX_DELAY) == pdTRUE)
            // {
            memcpy(buff, receivedBuff, BUFFER_SIZE);
            receivedMsg = true;
            //*********** exclusivement pour tests **************************** */
            // if (receivedBuff[0] != 71)
            // {
            //     Serial.println("Message reçu :");
            //     for (int i = 0; i < receivedBuff[0]; i++)
            //     {
            //         Serial.printf("%d ", buff[i]); // Affiche chaque octet en hexadécimal
            //     }
            //     Serial.println();
            // }

            //*************************************************************** */
            //     xSemaphoreGive(buffSemaphore);
            // }
        }
    }
}

void MFXWaveform::setup()
{
    // ptrToClass = this;
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, timerHandler, true);
    timerAlarmWrite(timer, 50, true);
    timerAlarmEnable(timer);

    m_polarity = HIGH;
    stateMachine = 1;

    mfxQueue = xQueueCreate(10, BUFFER_SIZE);
    if (mfxQueue == NULL)
    {
        Serial.println("Erreur : impossible de créer la queue");
        while (1)
            ; // Boucle infinie si échec
    }
    // Running on core 1 because WiFi is running on core 0
    xTaskCreatePinnedToCore(MFXWaveform::receiverTask, "Receiver Task", 2 * 1024, NULL, 1, NULL, 1);
}

hw_timer_t *MFXWaveform::timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR MFXWaveform::timerHandler()
{
    portENTER_CRITICAL_ISR(&timerMux);
    {
        step++; // Compteur de timing

        switch (stateMachine)
        {
        case 10:
            handleState10();
            break;
        case 20:
            handleState20();
            break;
        }
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}


void MFXWaveform::handleState10()
{
    if (step == 1 || step == 3 || step == 4 || step == 6 || step == 8 || step == 9)
        toggleSignal(); // Stefan Krauss § 2.2.1 Synchronization

    else if (step == 10)
    {
        if (receivedMsg && nSync > 3)
        {
            receivedMsg = false;
            nSync = 0;
            stuffCount = 0;
            bitVal = 1;
            bitIdx = 1;
            leng = buff[0];
            stateMachine = 20; // Transition vers l'envoi des données
        }
        else
        {
            step = 0;
            nSync++;
        }
    }
}

void MFXWaveform::handleState20()
{
    if (bitVal)
        toggleSignal();
    else
    {
        if (buff[bitIdx]) // == 1
        {
            toggleSignal();
            stuffCount++;
            if (stuffCount == 8) // Stefan Krauss § 2.2.2 Stuffing
            {
                stuffCount = 0;
                bitVal = 0;
                bitVal = !bitVal;
            }
        }
        else
            stuffCount = 0;

        bitIdx++;
        leng--;
        if (leng == 0)
        {
            stateMachine = 10; // Retour à l'état 10
            step = 0;
        }
    }
    bitVal = !bitVal;
}
