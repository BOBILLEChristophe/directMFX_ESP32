/*

  Loco.cpp


*/

#include "Loco.h"

// Constructeur par défaut.
// Il n'incrémente plus le compteur de réinscription MFX : unordered_map::operator[]
// appelle ce constructeur, et chaque appel changeait le compteur diffusé aux
// décodeurs, ce qui forçait la réinscription de toutes les locos.
Loco::Loco() : m_addr(0), m_dir(0), m_speed(0), m_name("")
{
  memset(m_uid, 0, sizeof(m_uid));
  memset(m_funct, 0, sizeof(m_funct));
}

Loco::Loco(uint8_t address, String name, uint8_t uid0, uint8_t uid1, uint8_t uid2, uint8_t uid3)
    : m_addr(address),
      m_uid{uid0, uid1, uid2, uid3},
      m_dir(0),
      m_speed(0),
      m_name(name)
{
  memset(m_funct, 0, sizeof(m_funct));
}

byte Loco::UID(byte i) { return i < 4 ? m_uid[i] : 0; }
void Loco::addr(uint8_t address) { m_addr = address; }
uint8_t Loco::addr() const { return m_addr; }
void Loco::dir(uint8_t dir) { m_dir = dir ? 1 : 0; }
uint8_t Loco::dir() const { return m_dir; }
void Loco::toggleDir() { m_dir ^= 1; }
void Loco::speed(uint8_t speed) { m_speed = speed; }
uint8_t Loco::speed() const { return m_speed; }
void Loco::funct(uint32_t x, bool y)
{
  if (x < 32)
    m_funct[x] = y;
}
uint8_t Loco::funct(uint32_t x) const { return x < 32 ? m_funct[x] : 0; }
void Loco::toggleFunct(uint32_t x)
{
  if (x < 32)
    m_funct[x] ^= 1;
}
String Loco::name() const { return m_name; }

void Loco::addLoco(const Loco &loco)
{
  locoMap[loco.addr()] = loco;
}

// Utilisation d'un unordered_map pour stocker les locomotives en fonction de leur adresse
std::unordered_map<uint8_t, Loco> Loco::locoMap;

// Recherche d'une locomotive par son adresse.
// Une loco inconnue n'est plus créée : elle n'aurait jamais été rafraîchie
// (absente du tableau parcouru par Message::periodic) et sa création modifiait
// le compteur de réinscription.
Loco *Loco::findLoco(uint8_t address)
{
  auto it = locoMap.find(address);
  return it == locoMap.end() ? nullptr : &it->second;
}

Loco *Loco::createLoco(uint8_t address, String name, uint8_t uid0, uint8_t uid1, uint8_t uid2, uint8_t uid3)
{
  Loco loco(address, name, uid0, uid1, uid2, uid3);
  auto it = locoMap.find(address);
  if (it != locoMap.end())
    it->second = loco;
  else
    it = locoMap.emplace(address, loco).first;
  return &it->second;
}
