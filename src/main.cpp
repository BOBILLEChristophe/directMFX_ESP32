/*
  Main.cpp

  Source: https://gelit.ch/Train/DirectMFX.ino

  MFX is a trademark of MARKLIN.
*/


#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#define PROJECT "DirectMFX_ESP32"
#define VERSION "0.7.2"
#define AUTHOR "Christophe BOBILLE : christophe.bobille@gmail.com"

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <unordered_map>
#include <vector>
#include "Centrale.h"
#include "CurrentMonitor.h"
#include "Loco.h"
#include "Message.h"
#include "MFXWaveform.h"
#include "RxTask.h"



// Central system ID used for identification
const uint32_t idCentrale = 0x476bb7dc;

//----------------------------------------------------------------------------------------
//  Buffers for communication: Rocrail always sends 13 bytes
//----------------------------------------------------------------------------------------

//static const uint8_t BUFFER_S = 13; // Size of communication buffers
const uint8_t BUFFER_S = 13;        // Size of communication buffers
uint8_t rxBuffer[BUFFER_S];         // RX buffer for receiving data
uint8_t txBuffer[BUFFER_S];         // TX buffer for transmitting data

//----------------------------------------------------------------------------------------
//  Märklin-specific hash
//----------------------------------------------------------------------------------------

//uint16_t thisHash = 0x18FF; // hash
uint16_t rrHash;            // Rocrail hash received from Rocrail system

// Pins for controlling H-Bridge
gpio_num_t IN1_pin = GPIO_NUM_27;  // Pin for IN1 of H-Bridge
gpio_num_t IN2_pin = GPIO_NUM_33;  // Pin for IN2 of H-Bridge
gpio_num_t EN_pin = GPIO_NUM_32;   // Pin for Enable signal of H-Bridge
gpio_num_t CURRENT_MONITOR_PIN_MAIN = GPIO_NUM_36;   // Pin for Enable signal of H-Bridge


// Pointer to the MFX message queue
QueueHandle_t mfxQueue;

// Locomotives
const byte nbLocos = 10;  // Maximum number of locomotives
Loco *loco[nbLocos];  // Array of locomotive pointers

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
//#define ETHERNET
#define WIFI

//----------------------------------------------------------------------------------------

// Define IP address and port for the communication server
IPAddress ip(192, 168, 1, 210);
const uint port = 2560;

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

// WiFi credentials
const char *ssid = "**********";
const char *password = "**********";
IPAddress gateway(192, 168, 1, 1);  // Default gateway
IPAddress subnet(255, 255, 255, 0); // Subnet mask
WiFiServer server(port);
WiFiClient client;

#endif

//----------------------------------------------------------------------------------------
//  Queues
//----------------------------------------------------------------------------------------

QueueHandle_t rxQueue;  // Queue for receiving data

//----------------------------------------------------------------------------------------
//  Tasks declaration
//----------------------------------------------------------------------------------------

void rxTask(void *pvParameters);  // Task for handling received data
void currentMonitorTask(void *pvParameters);  // Task for handling current check

void setup()
{
  Serial.begin(115200);

  Serial.printf("\nProject   :    %s", PROJECT);
  Serial.printf("\nVersion   :    %s", VERSION);
  Serial.printf("\nAuteur    :    %s", AUTHOR);
  Serial.printf("\nFichier   :    %s", __FILE__);
  Serial.printf("\nCompiled  :    %s", __DATE__);
  Serial.printf(" - %s\n\n", __TIME__);
  
  // Initialize MFXWaveform and Centrale modules
  MFXWaveform::setup();
  Centrale::setup(idCentrale, IN1_pin, IN2_pin, EN_pin);

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

// Ethernet-specific initialization
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
  // WiFi-specific initialization
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

  // Wait for a client connection
  while (!client)
    client = server.available();

  // extract the Rocrail hash
  if (client.connected())
  {                 // loop while the client's connected
    int16_t rb = 0; //!\ Do not change type int16_t See https://www.arduino.cc/reference/en/language/functions/communication/stream/streamreadbytes/
    while (rb != BUFFER_S)
    {
      if (client.available()) // if there's bytes to read from the client,
        rb = client.readBytes(rxBuffer, BUFFER_S);
    }
    rrHash = ((rxBuffer[2] << 8) | rxBuffer[3]);

    Serial.printf("New Client Rocrail : 0x");
    Serial.println(rrHash, HEX);

    //Initialize locomotive instances
    loco[0] = Loco::createLoco(1, "5519 CFL", 0x73, 0xF6, 0x88, 0x34);
    loco[1] = Loco::createLoco(2, "test", 0x73, 0xF6, 0x88, 0x35);

    // Set up the Message system
    Message::setup(loco, nbLocos);

    // Create a queue for received data
    rxQueue = xQueueCreate(50, BUFFER_SIZE * sizeof(byte));
    // txQueue = xQueueCreate(50, BUFFER_SIZE * sizeof(byte));
    // debugQueue = xQueueCreate(50, BUFFER_SIZE * sizeof(byte));  // Create debug queue

    // Create tasks
    xTaskCreatePinnedToCore(rxTask, "RxTask", 4 * 1024, NULL, 5, NULL, 0); // priority 5
    // xTaskCreatePinnedToCore(txTask, "TxTask", 4 * 1024, NULL, 3, NULL, 0);                  // priority 3
    // xTaskCreatePinnedToCore(debugFrameTask, "debugFrameTask", 2 * 1024, NULL, 1, NULL, 0);  // debug task with priority 1 on core 1
    xTaskCreatePinnedToCore(currentMonitorTask, "CurrentMonitorTask", 1 * 1024, &mainMonitor, 9, NULL, 0); // priority 5

    void rxTask(void *pvParameters);
    // void txTask(void *pvParameters);
    // void debugFrameTask(void *pvParameters);  // Debug task
  }
} // End setup

void loop() {}