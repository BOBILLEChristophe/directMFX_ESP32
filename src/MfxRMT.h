
/*

   MfxRMT.h


*/

#pragma once

#include <Arduino.h>
#include <vector>
#include "driver/rmt.h"
#include "Centrale.h"
#include "Loco.h"
#include "Message.h"

extern QueueHandle_t mfxQueue;
#define MAX_DATA_LEN 100
#define BUFFER_SIZE 100

class RMTChannel
{
private:
    // static std::vector<rmt_item32_t> item;
    static bool level;
    static bool startFrame;
    static bool isReady;
    static rmt_item32_t *item;
    static byte itemCount;
    static volatile bool receivedMsg;
    static byte buff[BUFFER_SIZE]; // Declared as static for shared access

public:
    RMTChannel() = delete;
    static void setup(gpio_num_t, gpio_num_t);
    static void receiverTask(void *parameter);
    static void sendToCentrale(const byte*);
    static void setMFXbit1(rmt_item32_t *);
    static void setMFXbit0(rmt_item32_t *);
    static void setMFXsync(rmt_item32_t *);
};
