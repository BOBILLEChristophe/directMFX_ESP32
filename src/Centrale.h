/*

   Centrale.h


*/

#pragma once

// #ifndef _CENTRALE_H_
// #define _CENTRALE_H_

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
//#include "Message.h"

class Centrale
{
private:
    static uint32_t m_uid;
    static gpio_num_t IN1_pin;
    static gpio_num_t IN2_pin;
    static gpio_num_t EN_pin;

    static bool m_power;

public:
    Centrale() = delete;
    static uint32_t gUid();
    static void setup(uint32_t, gpio_num_t, gpio_num_t, gpio_num_t);
    static void setPower(bool);
    static bool power();
    static void togglePin(bool);
};

