/*
  MFX Track Station (formerly directMFX_ESP32)
  https://github.com/BOBILLEChristophe/MFX-Track-Station

  main.cpp

  Source: https://gelit.ch/Train/DirectMFX.ino
          https://www.skrauss.de/modellbahn/Trackprotocol.pdf
          https://gelit.ch/Train/MFX.pdf

  mfx is a registered trademark of Gebr. Marklin & Cie GmbH.
*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#define PROJECT "MFX Track Station"
#define VERSION "0.10.0"
#define AUTHOR "Christophe BOBILLE : christophe.bobille@gmail.com"

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "Centrale.h"
#include "CurrentMonitor.h"
#include "Discovery.h"
#include "Feedback.h"
#include "Loco.h"
#include "Message.h"
#include "RxTask.h"
#include "MfxRMT.h"

// Central system ID used for identification
const uint32_t idCentrale = 0x476bb7dc;

//----------------------------------------------------------------------------------------
//  Buffers for communication: Rocrail always sends 13 bytes
//----------------------------------------------------------------------------------------

const uint8_t BUFFER_S = 13; // Size of communication buffers
uint8_t rxBuffer[BUFFER_S];  // RX buffer for receiving data

// Pins for controlling H-Bridge
gpio_num_t IN1_pin = GPIO_NUM_27;                  // Pin for IN1 of H-Bridge
gpio_num_t IN2_pin = GPIO_NUM_33;                  // Pin for IN2 of H-Bridge
gpio_num_t EN_pin = GPIO_NUM_32;                   // Pin for Enable signal of H-Bridge
gpio_num_t CURRENT_MONITOR_PIN_MAIN = GPIO_NUM_36; // Pin for current mesure
gpio_num_t FEEDBACK_PIN = GPIO_NUM_26;             // Output of the feedback comparator (MCP6541)

// Pointer to the MFX message queue
QueueHandle_t mfxQueue = NULL;

// Locomotives: the declared ones, plus room for the decoders found on the
// track by the automatic discovery (Discovery.h)
const byte MAX_LOCOS = 16; // Maximum number of locomotives
const byte nbLocos = 5;    // Locomotives declared below
Loco *loco[MAX_LOCOS] = {}; // Array of locomotive pointers

CurrentMonitor mainMonitor(CURRENT_MONITOR_PIN_MAIN); // create monitor for current on Main Track

//----------------------------------------------------------------------------------------
//  Select a communication mode
//----------------------------------------------------------------------------------------

// Caution:
// This note refers to a potential change in the following file:
// /Users/xxxxx/.platformio/packages/framework-arduinoespressif32/cores/esp32/Server.h:31:18
//
// Details:
// - Original declaration:
//     virtual void begin() = 0; // For Ethernet
// - Updated declaration:
//     virtual void begin(uint16_t port = 0) = 0; // For WiFi
//
// Ensure you are aware of this change and update your implementation accordingly.

// Uncomment one of the following modes:
// #define ETHERNET
#define WIFI

//----------------------------------------------------------------------------------------

// Define IP address and port for the communication server
// 15731 is the TCP port of the Marklin CS2, used by default by Rocrail
// ("CS2" command station). Versions <= 0.9.2 used 2560.
IPAddress ip(192, 168, 1, 210);
const uint port = 15731;

//----------------------------------------------------------------------------------------
//  Ethernet configuration (if used)
//----------------------------------------------------------------------------------------
#if defined(ETHERNET)
#include <Ethernet.h>
#include <SPI.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
EthernetServer server(port);
EthernetClient client;

//----------------------------------------------------------------------------------------
//  WiFi configuration (if used)
//----------------------------------------------------------------------------------------
#elif defined(WIFI)
#include <WiFi.h>

// WiFi credentials : copy src/secrets.h.example to src/secrets.h and fill it in.
// secrets.h is ignored by Git, so your password never ends up on GitHub.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy src/secrets.h.example to src/secrets.h and enter your WiFi SSID and password"
#endif
IPAddress gateway(192, 168, 1, 1);  // Default gateway
IPAddress subnet(255, 255, 255, 0); // Subnet mask
WiFiServer server(port);
WiFiClient client;

#endif

//----------------------------------------------------------------------------------------
//  Rocrail connection (used by rxTask)
//----------------------------------------------------------------------------------------

/*
 Reads one 13-byte CS2 frame from Rocrail into buf.
 Accepts a new client whenever there is none, so Rocrail can disconnect and
 reconnect without restarting the command station.
 Returns true when a complete frame was read.
*/
bool readRocrailFrame(uint8_t *buf, size_t len)
{
  if (!client || !client.connected())
  {
    if (client)
      client.stop();
    client = server.available();
    if (!client)
      return false;
    Serial.println("Rocrail connected.");
  }

  if (client.available() < (int)len)
    return false;

  return client.readBytes(buf, len) == len;
}

//----------------------------------------------------------------------------------------
//  Tasks declaration
//----------------------------------------------------------------------------------------

void rxTask(void *pvParameters);             // Task for handling received data
void currentMonitorTask(void *pvParameters); // Task for handling current check
void currentDisplayTask(void *pvParameters);

void setup()
{
  Serial.begin(115200);

  Serial.printf("\nProject   :    %s", PROJECT);
  Serial.printf("\nVersion   :    %s", VERSION);
  Serial.printf("\nAuteur    :    %s", AUTHOR);
  Serial.printf("\nFichier   :    %s", __FILE__);
  Serial.printf("\nCompiled  :    %s", __DATE__);
  Serial.printf(" - %s\n\n", __TIME__);

  // Create MFX message queue (10 * 'BUFFER_SIZE'(100) octets)
  mfxQueue = xQueueCreate(10, BUFFER_SIZE);
  if (mfxQueue == NULL)
  {
    Serial.println("Error: Unable to create the MFX queue.");
    while (1)
      ;
  }
  Serial.println("MFX queue initialized.");

// Ensure at least one communication mode is defined
#if !defined(ETHERNET) && !defined(WIFI)
  Serial.print("Select a communication mode.");
  while (1)
  {
  }
#endif

  //--------------------------------------------------------------------------------------
  //  Command station: starts at boot, without waiting for Rocrail
  //--------------------------------------------------------------------------------------

  // Initialize locomotive instances
  loco[0] = Loco::createLoco(1, "5519 CFL", 0x73, 0xF6, 0x88, 0x34);
  loco[1] = Loco::createLoco(2, "241 A 004", 0x73, 0xE9, 0x2A, 0x0F);
  loco[2] = Loco::createLoco(3, "150 X 5", 0xFF, 0xE9, 0xB1, 0x94);
  loco[3] = Loco::createLoco(4, "040 DE 9", 0xF9, 0xE6, 0x10, 0xE0);
  loco[4] = Loco::createLoco(8, "150 Y 17 3R", 0x73, 0xFA, 0x74, 0x01);

  // Centrale first: Message::centrale() broadcasts its UID and counter
  Centrale::setup(idCentrale, EN_pin);
  //Centrale::setCounter(Centrale::counter() + 1); // PROVISOIRE : force la réinscription
  Serial.printf("MFX re-registration counter : %u\n", Centrale::counter());

  // Set up the Message system
  Message::setup(loco, MAX_LOCOS, nbLocos);
  // Print each command received from Rocrail (speed, direction, functions).
  // Set to false once everything works: printing slows down the reception.
  Message::setTrace(true);
  // Feedback sensor, before the signal task that uses it
  Feedback::setup(FEEDBACK_PIN);
  // Print the edge counts of each feedback request (set to false once it works)
  Feedback::setTrace(true);

  // Initialize RMTChannel
  RMTChannel::setup(IN1_pin, IN2_pin);

  // Automatic discovery of the decoders not yet registered
  Discovery::setup();

  xTaskCreatePinnedToCore(currentMonitorTask, "CurrentMonitorTask", 4 * 1024, &mainMonitor, 9, NULL, 0);
  xTaskCreatePinnedToCore(currentDisplayTask, "CurrentDisplayTask", 4 * 1024, &mainMonitor, 1, NULL, 0);

  //--------------------------------------------------------------------------------------
  //  Network
  //--------------------------------------------------------------------------------------

#if defined(ETHERNET)
  Serial.println("Waiting for Ethernet connection : ");
  // Ethernet initialization
  Ethernet.init(5); // MKR ETH Shield (change depending on your hardware)
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("IP address = ");
  Serial.println(Ethernet.localIP());
  Serial.printf("Port = %d\n", port);

#elif defined(WIFI)
  // WiFi initialization
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Waiting for WiFi connection : \n\n");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());
  Serial.printf("Port = %d\n", port);
  server.begin();

#endif

  Serial.printf("\n\nWaiting for connection from Rocrail.\n");

  // Rocrail frames are read by rxTask, which also accepts (re)connections
  xTaskCreatePinnedToCore(rxTask, "RxTask", 4 * 1024, NULL, 5, NULL, 0);
} // End setup

void loop()
{
  vTaskDelete(NULL); // Everything runs in FreeRTOS tasks
}
