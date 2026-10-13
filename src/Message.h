/*

   Message.h


*/

#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "Loco.h"
#include "MfxRMT.h"

//----------------------------------------------------------------------------------------
//  Commandes
//----------------------------------------------------------------------------------------
const byte BUFF_MSG_SIZE = 100;

#define CAN_SYSTEM_CONTROL 0x00
#define CAN_LOCO_SPEED 0x04
#define CAN_LOCO_DIREC 0x05
#define CAN_LOCO_FUNCT 0x06

class Message
{
private:
    static uint32_t id;
    static uint8_t prio;
    static uint8_t cmd;
    static uint8_t response;
    static uint16_t hash;
    static uint8_t length;
    static uint8_t data[8];
    static bool trace;         // Debug: internal tasks (very verbose)
    static bool traceCommands; // Commands received from Rocrail
    static byte buff[BUFF_MSG_SIZE];
    static Loco **loco;
    static byte locoSize;

public:
    static void setup(Loco *locoArray[], byte locoCount);
    // Prints each command received from Rocrail (speed, direction, functions)
    static void setTrace(bool on) { traceCommands = on; }
    static void decodeMsg(byte *);
    static void parse();
    static void periodic(void *);
    static void centrale(void *);
    static void setSID(void *);
};
