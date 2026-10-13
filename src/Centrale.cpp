/*

  Centrale.cpp


*/

#include "Centrale.h"
#include <Preferences.h>

uint32_t Centrale::m_uid = 0;
uint16_t Centrale::m_counter = Centrale::DEFAULT_MFX_COUNTER;
gpio_num_t Centrale::EN_pin = GPIO_NUM_0;
volatile bool Centrale::m_power = false;
volatile bool Centrale::m_overload = false;
portMUX_TYPE Centrale::m_mux = portMUX_INITIALIZER_UNLOCKED;

static const char *NVS_NAMESPACE = "centrale";
static const char *NVS_COUNTER_KEY = "mfxCounter";

uint32_t Centrale::gUid()
{
    return m_uid;
}

uint16_t Centrale::counter()
{
    return m_counter;
}

void Centrale::setCounter(uint16_t value)
{
    m_counter = value;
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false))
    {
        prefs.putUShort(NVS_COUNTER_KEY, value);
        prefs.end();
    }
}

void Centrale::setup(uint32_t uid, gpio_num_t EN)
{
    m_uid = uid;
    EN_pin = EN;

    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, true))
    {
        m_counter = prefs.getUShort(NVS_COUNTER_KEY, DEFAULT_MFX_COUNTER);
        prefs.end();
    }

    gpio_set_direction(EN_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(EN_pin, LOW);
}

/*
 Appelée par Rocrail (Stop / Go). Un Go est le seul moyen de sortir d'une surcharge.
*/
void Centrale::setPower(bool power)
{
    portENTER_CRITICAL(&m_mux);
    m_power = power;
    if (power)
        m_overload = false;
    gpio_set_level(EN_pin, power ? HIGH : LOW);
    portEXIT_CRITICAL(&m_mux);
    Serial.printf("power %s\n", power ? "on" : "off");
}

bool Centrale::power()
{
    return m_power;
}

/*
 Appelée par la surveillance du courant. La voie reste coupée jusqu'au prochain Go.
*/
void Centrale::setOverload()
{
    portENTER_CRITICAL(&m_mux);
    gpio_set_level(EN_pin, LOW);
    m_power = false;
    m_overload = true;
    portEXIT_CRITICAL(&m_mux);
}

bool Centrale::overload()
{
    return m_overload;
}
