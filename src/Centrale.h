/*

   Centrale.h


*/

#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

class Centrale
{
private:
    static uint32_t m_uid;
    static uint16_t m_counter;
    static gpio_num_t EN_pin;
    static volatile bool m_power;
    static volatile bool m_overload;
    static volatile uint32_t m_powerOnMs;
    static portMUX_TYPE m_mux;

public:
    /*
     Compteur de réinscription MFX diffusé avec l'UID de la centrale (Krauss 3.2.6).
     Un décodeur qui voit changer ce compteur oublie son adresse et se réinscrit :
     il ne doit donc changer que sur demande explicite.
     5 est la valeur que diffusaient les versions <= 0.9.2 avec 4 locos déclarées :
     la garder par défaut évite une réinscription de toutes les locos à la mise à jour.
    */
    static const uint16_t DEFAULT_MFX_COUNTER = 5;

    Centrale() = delete;
    static uint32_t gUid();
    static uint16_t counter();
    static void setCounter(uint16_t); // mémorisé en NVS
    static void setup(uint32_t, gpio_num_t EN);
    static void setPower(bool);
    static bool power();
    // Durée depuis la dernière mise sous tension de la voie (ms), 0 si coupée
    static uint32_t poweredForMs();
    static void setOverload(); // coupe la voie et mémorise la surcharge
    static bool overload();
};
