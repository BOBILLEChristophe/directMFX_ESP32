/*

  Loco.h


*/

#ifndef __LOCO_H__
#define __LOCO_H__

#include <Arduino.h>

class Loco
{
private:
    std::array<byte, 4> m_uid;
    uint8_t m_addr;
    uint8_t m_dir;
    uint8_t m_speed;
    bool    m_funct[32];
    String  m_name;

public:
    byte    UID(byte);
    void    addr(uint8_t);
    uint8_t addr();
    void    dir(uint8_t);
    uint8_t dir();
    void    toggleDir();
    void    speed(uint8_t);
    uint8_t speed();
    void    setFunct(uint32_t, bool);
    bool    getFunct(uint32_t);
    void    toggleFunct(uint32_t);
    void    name(String);
    String  name();

   static byte zahler;
   static byte locoActive;

  Loco(String, byte, byte, byte, byte); // Constructor
};

#endif
