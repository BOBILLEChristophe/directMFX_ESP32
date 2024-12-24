/*

  Centrale.cpp


*/

#include "Centrale.h"

uint32_t Centrale::m_uid = 0;
gpio_num_t Centrale::IN1_pin = GPIO_NUM_0;
gpio_num_t Centrale::IN2_pin = GPIO_NUM_0;
gpio_num_t Centrale::EN_pin = GPIO_NUM_0;
bool Centrale::m_power = false;

uint32_t Centrale::Centrale::gUid()
{
    return m_uid;
}

void Centrale::setup(uint32_t uid, gpio_num_t IN1, gpio_num_t IN2, gpio_num_t EN)
{
    m_uid = uid;
    IN1_pin = IN1;
    IN2_pin = IN2;
    EN_pin = EN;

    gpio_set_direction(EN_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(EN_pin, LOW);
}

void Centrale::setPower(bool power)
{
    m_power = power;
    if (m_power)
        gpio_set_level(EN_pin, HIGH); // Active l'alimentation
    else
        gpio_set_level(EN_pin, LOW); // Désactive l'alimentation
    Serial.printf("power %s\n", m_power ? "on" : "off");
}

bool Centrale::power()
{
    return m_power;
}

void Centrale::togglePin(bool polarity)
{
    gpio_set_level(IN1_pin, !polarity);
    gpio_set_level(IN2_pin, polarity);
}
