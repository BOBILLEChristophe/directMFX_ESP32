
/**********************************************************************

CurrentMonitor.h

**********************************************************************/

#ifndef CURRENTMONITOR_H
#define CURRENTMONITOR_H

#include "Arduino.h"
#include "Centrale.h"

#define CURRENT_SAMPLE_SMOOTHING 0.01
#define CURRENT_SAMPLE_MAX 1000 // 1 V env 1 A
#define CURRENT_SAMPLE_TIME 1

class CurrentMonitor
{
public:
    gpio_num_t m_pin;
    float m_current;
    // Constructeur
    CurrentMonitor(gpio_num_t pin)
        : m_pin(pin), m_current(0)
    {
    }
};

#endif