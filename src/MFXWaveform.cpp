/*

  MFXWaveform.cpp


*/

#include "MFXWaveform.h"

bool MFXWaveform::m_polarity = false;
byte MFXWaveform::step = 0;
volatile byte MFXWaveform::nSync = 0;
volatile byte MFXWaveform::stufN = 0;
volatile byte MFXWaveform::Ti = 0;
volatile bool MFXWaveform::TuCmd = false;
volatile byte MFXWaveform::Pa[18 + 4] = {0};
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

    // Création de la tâche de réception
    xTaskCreatePinnedToCore(MFXWaveform::receiverTask, "Receiver Task", 2 * 1024, NULL, 1, NULL, 0);
}


void MFXWaveform::toggleSignal()
{
    Centrale::togglePin(m_polarity);
    m_polarity = !m_polarity;
} // Change Level

hw_timer_t *MFXWaveform::timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR MFXWaveform::timerHandler()
{
    portENTER_CRITICAL_ISR(&timerMux);
    {
        step++; // Compteur de timing

        switch (stateMachine)
        {
        case 1:
            // État d'attente initial, pas d'action
            // Devra probablement être viré
            break;
        case 3:
            handleState3();
            break;
        case 4:
            handleState4();
            break;
        case 5:
            handleState5();
            break;
        case 7:
            handleState7();
            break;
        case 10:
            handleState10();
            break;
        case 20:
            handleState20();
            break;
        case 30:
            handleState30();
            break;
        }
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

void MFXWaveform::handleState3()
{
    if (step == 1 || step == 3 || step == 4 || step == 6 || step == 8 || step == 9)
    {
        toggleSignal();
    }
    else if (step == 10)
    {
        nSync++;
        if (nSync > 1)
        {
            step = 0;
            stateMachine = 4; // Transition vers l'état suivant
        }
        else
        {
            step = 0; // Réinitialisation
        }
    }
}

void MFXWaveform::handleState4()
{
    if (step == 125)
    {
        Ti = 0;           // Réinitialise l'index du paquet
        stateMachine = 5; // Passe à l'état d'envoi du premier paquet
    }
}

void MFXWaveform::handleState5()
{
    if (Pa[Ti])
    {
        toggleSignal();
        delayMicroseconds(85);
        toggleSignal();
    }
    else
    {
        toggleSignal();
        delayMicroseconds(9);
        toggleSignal();
    }

    Ti++;
    if (Ti == 18)
    {
        step = 0;
        stateMachine = 7; // Transition vers l'envoi du deuxième paquet
    }
}

void MFXWaveform::handleState7()
{
    if (Pa[Ti])
    {
        toggleSignal();
        delayMicroseconds(85);
        toggleSignal();
    }
    else
    {
        toggleSignal();
        delayMicroseconds(9);
        toggleSignal();
    }

    Ti++;
    if (Ti == 18)
    {
        step = 0;
        stateMachine = 8; // Transition vers l'état suivant
    }
}

void MFXWaveform::handleState10()
{
    if (step == 1)
    {
        toggleSignal();
    }
    else if (step == 3 || step == 4 || step == 6 || step == 8 || step == 9)
    {
        toggleSignal();
    }
    else if (step == 10)
    {
        if (receivedMsg && nSync > 3)
        {
            receivedMsg = false;
            nSync = 0;
            stufN = 0;
            bitVal = 1;
            bitIdx = 1;
            leng = buff[0];
            stateMachine = 20; // Transition vers l'envoi des données
        }
        else if (TuCmd)
        {
            step = 0;
            nSync = 0;
            stateMachine = 3;
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
    {
        toggleSignal();
    }
    else
    {
        if (buff[bitIdx])
        {
            toggleSignal();
            stufN++;
            if (stufN == 8)
            {
                stufN = 0;
                bitVal = 0;
                stateMachine = 30; // Transition vers l'état suivant
            }
        }
        else
        {
            stufN = 0;
        }
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

void MFXWaveform::handleState30()
{
    if (bitVal)         // Si le bit F est à 1
        toggleSignal(); // Envoie un "1"
    else
        stateMachine = 20; // Retourne à l'état 20 pour continuer le flux de données
    bitVal = !bitVal;      // Inverse le bit F (alternance entre 1 et 0)
}
