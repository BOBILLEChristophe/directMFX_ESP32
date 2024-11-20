/*

  Loco.cpp


*/

#include "Loco.h"

uint8_t Loco::zahler = 0;
uint8_t Loco::locoActive = 0;

// Constructeur par défaut
Loco::Loco() : m_addr(++zahler), m_name(""), m_speed(0), m_dir(0)
{
  memset(m_uid, 0, sizeof(m_uid));
  memset(m_funct, 0, sizeof(m_funct));
  // ++zahler;
  // m_addr = zahler;
}

Loco::Loco(uint8_t address, String name, uint8_t uid0, uint8_t uid1, uint8_t uid2, uint8_t uid3)
    : m_addr(address),
      m_uid{uid0, uid1, uid2, uid3},
      m_dir(0),
      m_speed(0),
      m_name(name)
{
  memset(m_funct, 0, sizeof(m_funct));
  // ++zahler;
  // m_addr = zahler;
}

byte Loco::UID(byte i) { return m_uid[i]; }
void Loco::addr(uint8_t address) { m_addr = address; }
uint8_t Loco::addr() const { return m_addr; }
void Loco::dir(uint8_t dir) { m_dir = dir; }
uint8_t Loco::dir() const { return m_dir; }
void Loco::toggleDir() { m_dir ^= 1; }
void Loco::speed(uint8_t speed) { m_speed = speed; }
uint8_t Loco::speed() const { return m_speed; }
void Loco::funct(uint32_t x, bool y)
{
  if (x < 32)
    m_funct[x] = y;
}
uint8_t Loco::funct(uint32_t x) const { return m_funct[x]; }
void Loco::toggleFunct(uint32_t x) { m_funct[x] ^= 1; }
String Loco::name() const { return m_name; }

void Loco::addLoco(const Loco &loco)
{
  locoMap[loco.addr()] = loco;
}

// Utilisation d'un unordered_map pour stocker les locomotives en fonction de leur adresse
std::unordered_map<uint8_t, Loco> Loco::locoMap;

// Recherche d'une locomotive par son adresse
Loco *Loco::findLoco(uint8_t address)
{
  auto it = locoMap.find(address);
  if (it != locoMap.end())
  {
    return &it->second;
  }
  // Crée une nouvelle loco si elle n'existe pas
  locoMap[address] = Loco(address, "Default", 0, 0, 0, 0);
  return &locoMap[address];
}
