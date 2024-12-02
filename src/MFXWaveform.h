

#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "Centrale.h"
#include "Loco.h"
#include "Message.h"

#define BUFFER_SIZE 100
extern QueueHandle_t mfxQueue;

class MFXWaveform
{
private:
    static bool m_polarity;
    static byte buff[BUFFER_SIZE];   // MFX Buffer (type BYTE to store Length)
    static bool bitVal;              // MFX First bit always 1
    static byte bitIdx;              // Interrupt buffer index B[I]);
    static byte leng;                // MFX Frame  Length
    static byte step;                // MFX Interrupt
    static volatile byte nSync;      // 3 caract de synchro MFX au min
    static volatile byte stuffCount; // Stuffing counter
    static volatile bool receivedMsg;
    static volatile byte stateMachine; // MFX State Machine

    //static void handleState3();  // Synchronisation
    //static void handleState4();  // Pause avant le premier paquet
    static void handleState10(); // Gestion de la synchronisation et des commandes
    static void handleState20(); // Transmission du flux de données
    //static void handleState30(); // Envoi des "0" après le stuffing

public:
    MFXWaveform() = delete;
    static hw_timer_t *timer;
    static void IRAM_ATTR timerHandler(void);
    static void setup();
    static void receiverTask(void *parameter);
    static void setStateMachine(byte state);
    static inline void toggleSignal()
    {
        Centrale::togglePin(m_polarity);
        m_polarity = !m_polarity;
    }
};
