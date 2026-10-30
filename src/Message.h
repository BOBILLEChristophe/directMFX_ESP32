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
#define CAN_MFX_BIND 0x02           // assignment of a rail address (UID, SID)
// Period of the command station frame (111 101, UID and counter), in ms.
// A Marklin MS2 with its Gleisbox sends it every 46 ms (recorded on the
// track); it was 500 ms before.
#define STATION_PERIOD_MS 50
#define CAN_PING 0x18               // who is on the bus? (answered as a CS2)
#define CAN_CONFIG_DATA 0x20        // request of a configuration file ("loks"...)
#define CAN_CONFIG_DATA_STREAM 0x21 // the file

// Defined in main.cpp: sends bytes (whole 13-byte frames) to Rocrail.
// Returns false if Rocrail is not connected. Safe from several tasks.
bool sendToRocrail(const uint8_t *buf, size_t len);

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
    static volatile byte locoSize; // locomotives in loco[]
    static byte locoCapacity;      // size of loco[]

public:
    // locoArray: room for capacity locomotives, the first count are filled
    static void setup(Loco *locoArray[], byte capacity, byte count);
    // Adds a locomotive to the refresh (drive frames and Bind).
    // Returns false if the array is full.
    static bool addLoco(Loco *l);
    // Prints each command received from Rocrail (speed, direction, functions)
    static void setTrace(bool on) { traceCommands = on; }
    static void decodeMsg(byte *);
    static void parse();
    static void periodic(void *);
    static void driveFrame(Loco *l, byte *out, size_t outSize);
    static void sendNow(Loco *l);
    static void centrale(void *);
    static void setSID(void *);

    // Tells Rocrail that the decoder uid has the rail address sid, as an
    // MS2 does on the CAN bus when it registers a locomotive (command 0x02,
    // UID and SID), followed by the answer of the track box.
    static void announceBind(uint32_t uid, uint16_t sid);

private:
    static void configRequest();
    static void pingAnswer();
    static void sendLocoList();
    static void traceUnknown();
};
