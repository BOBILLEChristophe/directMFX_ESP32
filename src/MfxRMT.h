/*

   MfxRMT.h

   Track signal generator: one task owns the RMT channel and keeps it busy.
   Frames come from mfxQueue; when there is none, synchronisation patterns
   fill the line, so the signal never stops.

*/

#pragma once

#include <Arduino.h>
#include "driver/rmt.h"
#include "MfxSignal.h"

extern QueueHandle_t mfxQueue;
#define BUFFER_SIZE 100 // size of one frame in mfxQueue (see MfxFrame::toBuffer)

class RMTChannel
{
public:
    RMTChannel() = delete;
    static void setup(gpio_num_t IN1, gpio_num_t IN2);

private:
    static void signalTask(void *parameter);
    static void send(const MfxSignal &signal);

    // Two buffers: one is being sent while the next chunk is built in the other
    static rmt_item32_t sItems[2][MfxSignal::MAX_ITEMS];
    static uint8_t sCurrent;
};
