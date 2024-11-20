#pragma once

#include <Arduino.h>
#include <unordered_map>
#include <vector>
#include <string>

class Loco
{
private:
  uint16_t m_addr;
  uint8_t m_uid[4];
  uint8_t m_dir;
  uint8_t m_speed;
  uint8_t m_funct[32];
  String m_name;

public:
 
  Loco(); // Constructeur par défaut
  Loco(uint8_t, String, uint8_t, uint8_t, uint8_t, uint8_t);

  uint8_t UID(uint8_t);
  void addr(uint8_t);
  uint8_t addr() const;
  void dir(uint8_t);
  uint8_t dir() const;
  void toggleDir();
  void speed(uint8_t);
  uint8_t speed() const;
  void funct(uint32_t, bool);
  uint8_t funct(uint32_t) const;
  void toggleFunct(uint32_t);
  void name(String);
  String name() const;

  static void addLoco(const Loco& loco);

  static std::unordered_map<uint8_t, Loco> locoMap;
  static Loco *findLoco(uint8_t);
  static uint8_t zahler;
  static uint8_t locoActive;

  static Loco* createLoco(uint8_t address, String name, uint8_t uid0, uint8_t uid1, uint8_t uid2, uint8_t uid3) {
        locoMap[address] = Loco(address, name, uid0, uid1, uid2, uid3);
        return &locoMap[address];
    }

};
