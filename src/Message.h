/*

   Message.h


*/

#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
// #include "Centrale.h"
#include "Loco.h"
#include "MFXWaveform.h"

//----------------------------------------------------------------------------------------
//  Commandes
//----------------------------------------------------------------------------------------
const byte BUFF_MSG_SIZE = 100;

#define CAN_SYSTEM_CONTROL 0x00
#define CAN_LOCO_SPEED 0x04
#define CAN_LOCO_DIREC 0x05
#define CAN_LOCO_FUNCT 0x06

class Centrale;
class MFXWaveform;

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
    static bool trace; // Debug
    static byte buff[BUFF_MSG_SIZE];
    static byte len;
    static uint16_t crc;
    static long TZ, TP, T_S88, Time;
    static Loco **loco;
    static byte locoSize;

    static byte Pa[18 + 4]; // MM2 Packet for Turn

public:
    static void setup(Loco *locoArray[], byte locoCount);
    static void decodeMsg(byte *);
    static void parse();
    static void addr0();

    static void addr(byte);
    // static void setSID(byte);
    // static void centrale();
    // static void periodic(byte);
    static void CRC();
    static void bCRC(bool);
    static void Turn(int, bool);
    static void Tri(int, int);
    static void S88();

    static void periodic(void *);
    static void centrale(void *);
    static void setSID(void *);
};
