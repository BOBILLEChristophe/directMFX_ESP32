

#ifndef MFX_H
#define MFX_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "Loco.h"

#define BUFFER_SIZE 100
extern QueueHandle_t mfxQueue;

class MFX
{
private:
    static gpio_num_t IN1_pin;
    static gpio_num_t IN2_pin;
    static gpio_num_t EN_pin;
    static bool m_polarity;
    static bool m_power;

    static byte buff[BUFFER_SIZE];     // MFX Buffer (type BYTE to store Length)
    static bool bitVal;       // MFX First bit always 1
    static byte bitIdx;       // Interrupt buffer index B[I]);
    static byte leng;        // MFX Frame  Length
    static byte step;          // MFX Interrupt
    static volatile byte Pa[18 + 4]; // MM2 Packet for Turn
    static volatile byte nSync; // 3 caract de synchro MFX au min
    static volatile byte stufN; // Stuffing bit
    static volatile byte Ti;    // MM2_Bit
    static volatile bool receivedMsg;
    static volatile bool TuCmd;        // Turn Cmd
    static volatile byte stateMachine; // MFX State Machine
    // static volatile byte StatMach;

    static void handleState3();  // Synchronisation
    static void handleState4();  // Pause avant le premier paquet
    static void handleState5();  // Envoi du premier paquet
    static void handleState7();  // Envoi du deuxième paquet
    static void handleState10(); // Gestion de la synchronisation et des commandes
    static void handleState20(); // Transmission du flux de données
    static void handleState30(); // Envoi des "0" après le stuffing

    // static QueueHandle_t mfxQueue; // La queue statique partagée entre les instances

public:
    MFX() = delete;
    static hw_timer_t *timer;
    static void IRAM_ATTR timerHandler(void);
    static void setup(gpio_num_t, gpio_num_t, gpio_num_t);
    static void toggleSignal();
    static void setSM(byte x);
    static void setPower(bool);
    static bool power();

    static void receiverTask(void *parameter);
};

#endif
