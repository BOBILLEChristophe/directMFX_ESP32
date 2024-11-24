
/**********************************************************************

CurrentMonitor.h

**********************************************************************/

#ifndef CURRENTMONITOR_H
#define CURRENTMONITOR_H

#include "Arduino.h"
#include "Centrale.h"

#define CURRENT_SAMPLE_SMOOTHING 0.01

/*
La tension max accéptable sur la broche de l'ESP32 est de 3,3V
Cela correspond à un courant de 3,3A
Valeur max=1000(par A) × 3,3A = 3300 mA
Le code pour le courant max est donc :
#define CURRENT_SAMPLE_MAX 3300 // Correspond à 3,3 A pour un driver qui supporte 4A

Pour un courant supérieure à 3,3 A
vous devrez utiliser un pont diviseur de tension pour limiter l'entrée à 3,3 V.
*/

// La limite est ici fixée à 1A
#define CURRENT_SAMPLE_MAX 1000 // 1000 mV -> 1 A
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