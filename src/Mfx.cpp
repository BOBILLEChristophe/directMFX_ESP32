

#include "MFX.h"

// Déclaration du sémaphore
// SemaphoreHandle_t buffSemaphore;

gpio_num_t MFX::IN1_pin;
gpio_num_t MFX::IN2_pin;
gpio_num_t MFX::EN_pin;
bool MFX::m_polarity = false;
byte MFX::step = 0;
volatile byte MFX::nSync = 0;
volatile byte MFX::stufN = 0;
volatile byte MFX::Ti = 0;
volatile bool MFX::TuCmd = false;
volatile byte MFX::Pa[18 + 4] = {0};
byte MFX::buff[BUFFER_SIZE] = {0};
bool MFX::m_power = false;
volatile byte MFX::stateMachine = 0;
volatile bool MFX::receivedMsg = false;
bool MFX::bitVal = 0;
byte MFX::bitIdx = 0;
byte MFX::leng = 0;

// Tâche de réception de données
void MFX::receiverTask(void *parameter)
{
    // byte *buff = (byte *)parameter; // Cast du paramètre en pointeur vers un tableau de bytes
    byte receivedBuff[BUFFER_SIZE]; // Message reçu
    while (true)
    {
        // Lecture du message dans la queue
        if (xQueueReceive(mfxQueue, (void *)receivedBuff, (TickType_t)10) == pdPASS)
        {
            // Attendre que le sémaphore soit disponible
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

            // Libérer le sémaphore après la copie
            //     xSemaphoreGive(buffSemaphore);
            // }
        }
    }
}

void MFX::setup(gpio_num_t IN1, gpio_num_t IN2, gpio_num_t EN)
{
    IN1_pin = IN1;
    IN2_pin = IN2;
    EN_pin = EN;

    pinMode(EN_pin, OUTPUT);
    // gpio_set_direction(EN_pin, GPIO_MODE_OUTPUT);
    digitalWrite(EN_pin, LOW);
    pinMode(IN1_pin, OUTPUT);
    // gpio_set_direction(IN1_pin, GPIO_MODE_OUTPUT);
    pinMode(IN2_pin, OUTPUT);
    // gpio_set_direction(IN2_pin, GPIO_MODE_OUTPUT);
    digitalWrite(IN1_pin, LOW);
    digitalWrite(IN2_pin, HIGH);

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
    xTaskCreatePinnedToCore(MFX::receiverTask, "Receiver Task", 2 * 1024, NULL, 1, NULL, 0);

    // Création du sémaphore
    // buffSemaphore = xSemaphoreCreateBinary();
    // xSemaphoreGive(buffSemaphore); // Initialise le sémaphore comme disponible
}

void MFX::setSM(byte x)
{
    stateMachine = x;
}

void MFX::setPower(bool power)
{
    Serial.println(power);
    if (power)
    {
        digitalWrite(EN_pin, HIGH); // Active l'alimentation
        m_power = true;
    }
    else
    {
        digitalWrite(EN_pin, LOW); // Active l'alimentation
        m_power = false;
    }
}

bool MFX::power()
{
    return m_power;
}

void MFX::toggleSignal()
{
    digitalWrite(IN1_pin, !m_polarity);
    digitalWrite(IN2_pin, m_polarity);
    m_polarity = !m_polarity;
} // Change Level

hw_timer_t *MFX::timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR MFX::timerHandler()
{
    portENTER_CRITICAL_ISR(&timerMux);
    {
        // Protéger l'accès à buff avec le sémaphore
        // if (xSemaphoreTakeFromISR(buffSemaphore, NULL) == pdTRUE)
        // {
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
        //}
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

void MFX::handleState3()
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

void MFX::handleState4()
{
    if (step == 125)
    {
        Ti = 0;           // Réinitialise l'index du paquet
        stateMachine = 5; // Passe à l'état d'envoi du premier paquet
    }
}

void MFX::handleState5()
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

void MFX::handleState7()
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

void MFX::handleState10()
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

void MFX::handleState20()
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

void MFX::handleState30()
{
    if (bitVal)         // Si le bit F est à 1
        toggleSignal(); // Envoie un "1"
    else
        stateMachine = 20; // Retourne à l'état 20 pour continuer le flux de données
    bitVal = !bitVal;      // Inverse le bit F (alternance entre 1 et 0)
}
