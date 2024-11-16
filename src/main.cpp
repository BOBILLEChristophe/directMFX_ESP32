// 8 oct 2024 from https://gelit.ch/Train/DirectMFX.ino
// erreur avec ESP32 V3 - OK avec ESP32 V2.0.17
// 9 oct : supprimer S88
// 10 oct : Failed uploading: uploading error: exit status 2 - je n'arrive pas Ã  alimenter L293D depuis ma carte NodeMCU ESP32 30 pin

// 31 mars 2022 : Demo code for 2 MFX locs, 1 Turn (Adr=5) & S88 (16 inputs)
// MFX est une marque dÃƒÂ©posÃƒÂ©e par MÃƒÂ¤rklin
// Ce code minimaliste a besoin de la Gleisbox pour lire l'UID prÃƒÂ©sent dans chaque locomotive
// voir procÃƒÂ©dure dans Ã‚Â§4.1 de http://gelit.ch/Train/Raildue_F.pdf

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "Loco.h"
#include "Mfx.h"

gpio_num_t IN1_pin = GPIO_NUM_13; // IN1 H-Bridge
gpio_num_t IN2_pin = GPIO_NUM_14; // IN2 H-Bridge
gpio_num_t EN_pin = GPIO_NUM_12;  // ENABLE H-Bridge

const gpio_num_t Red_Led = GPIO_NUM_2; // Power Status

const byte nbLocos = 10; // Taille du tableau des locomotives

std::array<byte, BUFFER_SIZE> buff; // MFX Buffer (type BYTE to store Length)
int Ti;                             // MM2_Bit
uint16_t crc;                       // Krauss p13
byte len;                           // MFX Frame  Length
byte Pa[18 + 4];                    // MM2 Packet for Turn
byte loopState;                     // State in loop
bool trace;                         // Debug
unsigned long TZ, TP, T_S88, Time;

// Pointeur vers la queue
QueueHandle_t mfxQueue;

Loco *loco[nbLocos];

void addr(byte Loc);
void addr0();
void setSID(byte Loc);
void centrale();
void periodic(byte Loc);
void CRC();
void bCRC(bool b);
void Turn(int dev, bool val);
void Tri(int v, int b);
void S88();

void setup()
{
  trace = false; // debug
  Serial.begin(115200);
  Serial.println("DirectMFX_ESP32");

  MFX::setup(IN1_pin, IN2_pin, EN_pin);

  // Création de la queue (10 messages de 'BUFFER_SIZE'(100) octets)
  mfxQueue = xQueueCreate(10, BUFFER_SIZE);
  if (mfxQueue == NULL)
  {
    Serial.println("Erreur : impossible de creer la queue");
    while (1)
      ;
  }
  Serial.println("mfxQueue ok");
  delay(10000);

  loco[0] = new Loco("5519 CFL", 0x73, 0xF6, 0x88, 0x34);
  Loco::locoActive = loco[0]->addr();
  Serial.printf("%s is active\n", loco[0]->name());

  loopState = 1;
  Serial.println("PowerOFF - 'm' to PowerON");
}

void loop()
{
  byte cmd;

  switch (loopState)
  {

  case 0: // stay here after PowerOFF
    break;

  case 1:
    if (Serial.available() > 0)
    {
      cmd = Serial.read(); // start here from setup
      if (cmd == 'm')
      {
        MFX::setPower(true);
        MFX::setSM(10);
        delay(200);
        loopState = 2;
      }
    } // send Sync & PowerON
    break;

  case 2:
    if (MFX::power())
    {
      delay(500);
      digitalWrite(Red_Led, 1);
      Serial.println("PowerON ('a' to PowerOFF)");
      loopState = 3;
    }
    break;

  case 3:
    // Za = 0;
    centrale();
    delay(500);
    centrale();
    delay(500);
    centrale();
    delay(500);
    loopState = 4; //  Ã‚Â§4.2
    break;

  case 4:
    for (byte i = 0; i < nbLocos; i++)
    {
      if (loco[i] != nullptr)
      {
        setSID(i);
        delay(200);
        // Za++;
        centrale();
        delay(200);
        Serial.println("setSID Robel");
      }
    }
    TP = millis() + 50;
    TZ = millis() + 500;
    T_S88 = millis() + 1000; // set Timing
    loopState = 5;
    break;

  case 5:

    if (millis() > T_S88)
    {
      T_S88 = millis() + 1000;
      S88();
    }
    if (millis() > TZ)
    {
      TZ = millis() + 500;
      centrale();
    }
    if (millis() > TP)
    {
      TP = millis() + 5;
      if (loco[0] != nullptr) // Il existe au moins une loc
      {
        periodic(0);
      }
    } // Find Next Decoder

    if (Serial.available() > 0)
    {
      cmd = Serial.read();
      if (cmd > 64 && cmd < 91)
      {
        Serial.println("CapsLock !!!");
      }
      if (cmd >= '0' && cmd <= '7')
        loco[0]->speed(cmd - 48);

      // Serial.printf("commande = %d\n", cmd);

      switch (cmd)
      {
        //   case '8': LoA=Rob; Serial.println("Robel"); break;
        //   case '9': LoA=BLS; Serial.println("BLS"); break;
      case 'l':
        loco[0]->setFunct(0, 1);
        break;
      case 'b':
        loco[0]->toggleFunct(2);
        break;
      case 's':
        loco[0]->toggleFunct(3);
        break;
      case 'd':
        loco[0]->toggleDir();
        Serial.printf("Toggle Direction %s\n", loco[0]->dir() ? "arriere" : "avant");
        break;
        //   case 't': TurnVal=!TurnVal; Turn(TurnAdr,TurnVal); delay(250); Serial.print("TurnVal="); Serial.println(TurnVal); break;
        //   case 's': Serial.print("Power="); Serial.print(power); Serial.print("  S="); Serial.print(S); Serial.print("  SM="); Serial.println(SM);
        //             Serial.print("Len="); Serial.print(buff[0]); Serial.print("   CRC="); Serial.println(crc,HEX);
        //             for (a=1; a<=buff[0]; a++) {Serial.print(" "); Serial.print(buff[a]);} Serial.println();
        //             Serial.print("MM2= "); for (a=0; a<=18; a++) {Serial.print(Pa[a]); Serial.print(" ");}  Serial.println();break;
      case 'a':
        MFX::setPower(false);
        loopState = 1;
        Serial.println("Power0FF ('m' to PowerOFF)");
        break;
        //   case 'h': Serial.println("Speed:0-7  Robel:8  BLS:9  l:Light  d:Direction  t:Turn  s:Statistics"); break;
      }
    }
    static byte compt = 0;
    break;
  }
}

void addr0()
{
  buff[1] = 1;
  buff[2] = 0;
  buff[3] = 0;
  buff[4] = 0;
  buff[5] = 0;
  buff[6] = 0;
  buff[7] = 0;
  buff[8] = 0;
  buff[9] = 0;
  len = 2 + 7;
} // always 7 bit adr (buff[0] used for LENGTH)

void addr(byte Loc)
{
  /*
  2.2.4 Structure des trames de données

  10 AAAAAAA : adresse de 7 bits
  110 AAAAAAAAA : adresse de 9 bits
  1110 AAAAAAAAAAA : adresse de 11 bits
  1111 AAAAAAAAAAAAAA : adresse de 14 bits
  */

  buff[1] = 1;
  buff[2] = 0;
  len = 2;

  for (byte a = 0, b = 6; a < 7; a++, b--) // 7 bits address
    buff[++len] = Loc & (1 << b);
}

void setSID(byte idx)
{ // fixed SID from UID
  if (trace)
  {
    Serial.print("setSID-");
    Serial.println(loco[idx]->name());
  }
  len = 0;
  addr0();
  // §3.2.4  111 011 AAAAAAAAAAAAAA UID
  //******************** fonction 111 011 ****************************** */
  buff[10] = 1;
  buff[11] = 1;
  buff[12] = 1;
  buff[13] = 0;
  buff[14] = 1;
  buff[15] = 1;
  len += 6;
  //******************* adresse loco 7 premiers bits ******************* */
  // Adresse sur 14 bits
  buff[16] = 0;
  buff[17] = 0;
  buff[18] = 0;
  buff[19] = 0;
  buff[20] = 0;
  buff[21] = 0;
  buff[22] = 0;
  len += 7;
  //******************* adresse loco 7 derniers bits ******************* */

  for (byte a = 0, b = 6; a < 7; a++, b--, len++)
    buff[a + 23] = loco[idx]->addr() & (1 << b) >> b;

  //******************* UID loco *************************************** */

  for (byte j = 0; j < 4; j++)
  {
    for (byte a = 0, b = 7; a < 8; a++, b--)
      buff[++len] = loco[idx]->UID(j) & (1 << b) >> b;
  }

  //******************************************************************** */
  buff[0] = len; // Length in FIRST BYTE
  CRC();
  xQueueSend(mfxQueue, &buff, 0);
}

/*
3.2.6 Commande 111 101 : centrale
111 101 UUUUUUUUUUUUUUUUUUUUUUU ZZZZZZZZZZ
La centrale envoie cette commande à intervalles réguliers avec l'adresse de diffusion (broadcast) 0 indiquant ainsi son UID (U) et le compteur de nouvelles inscriptions (Z).
*/
void centrale()
{ // p23
  // byte a, b;
  if (trace)
  {
    // Serial.println("Zentrale");
  }
  len = 0;
  addr0();
  // Commande 111 101
  buff[10] = 1;
  buff[11] = 1;
  buff[12] = 1;
  buff[13] = 1;
  buff[14] = 0;
  buff[15] = 1;
  len += 6;
  // Centrale UID (32 bit)
  buff[16] = 0;
  buff[17] = 1;
  buff[18] = 0;
  buff[19] = 0;
  buff[20] = 0;
  buff[21] = 1;
  buff[22] = 1;
  buff[23] = 1;
  len += 8;
  buff[24] = 0;
  buff[25] = 1;
  buff[26] = 1;
  buff[27] = 0;
  buff[28] = 1;
  buff[29] = 0;
  buff[30] = 1;
  buff[31] = 1;
  len += 8;
  buff[32] = 1;
  buff[33] = 0;
  buff[34] = 1;
  buff[35] = 1;
  buff[36] = 0;
  buff[37] = 1;
  buff[38] = 1;
  buff[39] = 1;
  len += 8;
  buff[40] = 1;
  buff[41] = 1;
  buff[42] = 0;
  buff[43] = 1;
  buff[44] = 1;
  buff[45] = 1;
  buff[46] = 0;
  buff[47] = 0;
  len += 8;
  // Compteur (16 bits)
  buff[48] = 1;
  buff[49] = 0;
  buff[50] = 0;
  buff[51] = 0;
  buff[52] = 0;
  buff[53] = 0;
  buff[54] = 0;
  buff[55] = 0;
  len += 8;

  for (byte a = 0, b = 7; a < 8; a++, b--, len++)
    buff[a + 56] = ((Loco::zahler + 1) & (1 << b)) >> b;

  buff[0] = len; // Length in FIRST BYTE
  CRC();
  xQueueSend(mfxQueue, &buff, 0);
} // End centrale

void periodic(byte idx)
{
  if (trace)
  {
    Serial.print("Periodic-");
    Serial.println(loco[idx]->name());
  }

  len = 0;
  // adresse loco (7 bits)
  addr(loco[idx]->addr());
  // commande 001: Conduite 001 R SSSSSSS
  buff[10] = 0;
  buff[11] = 0;
  buff[12] = 1;
  len += 3;
  //  direction
  buff[13] = loco[idx]->dir() ? 1 : 0; //
  len += 1;
  // vitesse
  for (byte a = 0, b = 2; a < 3; a++, b--)
    buff[++len] = (loco[idx]->speed() & (1 << b)) >> b;
  // fonctions
  buff[17] = 0;
  buff[18] = 0;
  buff[19] = 0;
  buff[20] = 0;
  len += 4; // only MSB
  buff[21] = 0;
  buff[22] = 1;
  buff[23] = 1;
  buff[24] = 1;
  len += 4; // 3.1.5 F15-F0

  // fonctions F15-F0 (MSB)
  for (byte a = 0, b = 15; a < 16; a++, b--)
    buff[++len] = loco[idx]->getFunct(b);

  buff[0] = len; // Length in FIRST BYTE
  CRC();
  xQueueSend(mfxQueue, &buff, 0);
} // End periodic

void CRC()
{ // avoid to compute in interrupt ! --> easier for bit stuffing !
  byte a, b;
  crc = 0x007F;
  for (a = 1; a < buff[0] + 1; a++)
  {
    bCRC(buff[a]);
  } // CRC
  for (a = 0; a < 8; a++)
  {
    bCRC(0);
  } // Krauss p13 "diese bit mÃƒÂ¼ssen zuerst mit 0 belegt ..."}
  b = 8;
  for (a = 0; a < 8; a++)
  {
    buff[len + 1 + a] = bitRead(crc, b - 1);
    b--;
  }
  buff[0] += 8; // Length
}

void bCRC(bool b)
{ // Krauss p13
  crc = (crc << 1) + b;
  if ((crc & 0x0100) > 0)
  {
    crc = (crc & 0x00FF) ^ 0x07;
  }
}

void Turn(int dev, bool val)
{ // Device value
  int p[10];
  int a;
  int q[10];

  if (dev > 0 && dev < 320)
  {
    p[0] = 1;
    p[1] = 2;
    p[2] = 1 * 4;
    p[3] = 3 * 4;
    p[4] = 3 * 3 * 4;
    p[5] = 3 * 3 * 3 * 4;
    p[6] = 3 * 3 * 3 * 3 * 4;
    dev += 3;
    for (a = 6; a >= 2; a--)
    {
      q[a] = 0;
      if (dev >= p[a])
      {
        q[a]++;
        dev = dev - p[a];
      }
      if (dev >= p[a])
      {
        q[a]++;
        dev = dev - p[a];
      }
      switch (a)
      {
      case 2:
        Tri(q[2], 0);
        break; // MM2 Adr become HIGH adr
      case 3:
        Tri(q[3], 2);
        break;
      case 4:
        Tri(q[4], 4);
        break;
      case 5:
        Tri(q[5], 6);
        break;
      case 6:
        Tri(q[6], 8);
        break;
      }
    }
    for (a = 1; a >= 0; a--)
    {
      q[a] = 0;
      if (dev >= p[a])
      {
        q[a]++;
        dev = dev - p[a];
      }
      switch (a)
      {
      case 0:
        Tri(q[0], 12);
        break; // Factor 4 implemented with bit 12 - 15
      case 1:
        Tri(q[1], 14);
        break;
      }
    }
    Tri(val, 10); // value
    Tri(1, 16);   // always 1
  }
}

void Tri(int v, int b)
{ // value, Bit
  switch (v)
  {
  case 0:
    Pa[b] = 0;
    Pa[b + 1] = 0;
    break; // MC 145026 encoding
  case 1:
    Pa[b] = 1;
    Pa[b + 1] = 1;
    break;
  case 2:
    Pa[b] = 1;
    Pa[b + 1] = 0;
    break;
  }
}

void S88() {}
