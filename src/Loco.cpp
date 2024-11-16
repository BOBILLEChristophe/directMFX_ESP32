/*

  Loco.cpp


*/

#include "Loco.h"

byte Loco::zahler = 0;
byte Loco::locoActive = 0;

Loco::Loco(String name, byte uid0, byte uid1, byte uid2, byte uid3)
    : m_uid{uid0, uid1, uid2, uid3},
      m_dir(0),
      m_speed(0),
      m_name(name),
      m_funct{0}
{
  ++zahler;
  m_addr = zahler;
};

byte Loco::UID(byte i) { return m_uid[i]; }
void Loco::addr(uint8_t address) { m_addr = address; }
uint8_t Loco::addr() { return m_addr; }
void Loco::dir(uint8_t dir) { m_dir = dir; }
uint8_t Loco::dir() { return m_dir; }
void Loco::toggleDir() { m_dir = !m_dir; }
void Loco::speed(uint8_t speed) { m_speed = speed; }
uint8_t Loco::speed() { return m_speed; }
void Loco::setFunct(uint32_t x, bool y)
{
  if (x < 32)
    m_funct[x] = y;
}
bool Loco::getFunct(uint32_t x) { return m_funct[x]; }
void Loco::toggleFunct(uint32_t x) { m_funct[x] = !m_funct[x]; }
void Loco::name(String name) { m_name = name; }
String Loco::name() { return m_name; }
