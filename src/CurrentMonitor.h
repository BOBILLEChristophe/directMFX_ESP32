
/**********************************************************************

CurrentMonitor.h

**********************************************************************/

#ifndef CURRENTMONITOR_H
#define CURRENTMONITOR_H

#include "Arduino.h"
#include "Centrale.h"

#define CURRENT_SAMPLE_SMOOTHING 0.01f

/*
 La mesure se lit en millivolts (analogReadMilliVolts), puis se convertit en mA.
 La tension max acceptable sur la broche de l'ESP32 est de 3,3 V.

 CURRENT_MV_PER_A : tension fournie par le capteur pour 1 A.
 1000 mV par A correspond au commentaire d'origine (1000 mV -> 1 A).
 A VERIFIER sur le schéma de votre carte (La Box) avant de se fier au seuil.
*/
#define CURRENT_MV_PER_A 1000

// Seuil de coupure, en mA. La limite est ici fixée à 1 A.
#define MAX_CURRENT_MA 1000

#define CURRENT_SAMPLE_TIME 1 // ms

class CurrentMonitor
{
public:
    gpio_num_t m_pin;
    float m_current; // mA, lissé
    // Constructeur
    CurrentMonitor(gpio_num_t pin)
        : m_pin(pin), m_current(0)
    {
    }
};

#endif
