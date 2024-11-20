/*

  Main.cpp

  from https://gelit.ch/Train/DirectMFX.ino

  MFX est une marque déposée par MARKLIN

*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#define PROJECT "DirectMFX_ESP32"
#define VERSION "0.5"
#define AUTHOR "Christophe BOBILLE : christophe.bobille@gmail.com"

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <unordered_map>
#include <vector>
#include "Centrale.h"
#include "Loco.h"
#include "Message.h"
#include "MFXWaveform.h"

const uint32_t idCentrale = 0x476bb7dc;

//----------------------------------------------------------------------------------------
//  Buffers  : Rocrail always send 13 bytes
//----------------------------------------------------------------------------------------

static const uint8_t BUFFER_S = 13;
uint8_t rxBuffer[BUFFER_S]; // RX buffer
uint8_t txBuffer[BUFFER_S]; // TX buffer

//----------------------------------------------------------------------------------------
//  Marklin hash
//----------------------------------------------------------------------------------------

uint16_t thisHash = 0x18FF; // hash
uint16_t rrHash;            // for Rocrail hash

// Pin mapping LaBox
gpio_num_t IN1_pin = GPIO_NUM_27; // IN1 H-Bridge
gpio_num_t IN2_pin = GPIO_NUM_33; // IN2 H-Bridge
gpio_num_t EN_pin = GPIO_NUM_32;  // ENABLE H-Bridge

const byte nbLocos = 10; // Taille du tableau des locomotives

// Pointeur vers la queue
QueueHandle_t mfxQueue;

Loco *loco[nbLocos];

//----------------------------------------------------------------------------------------
//  Select a communication mode
//----------------------------------------------------------------------------------------

// #define ETHERNET
#define WIFI

IPAddress ip(192, 168, 1, 210);
const uint port = 2560;

//----------------------------------------------------------------------------------------
//  Ethernet
//----------------------------------------------------------------------------------------
#if defined(ETHERNET)
#include <Ethernet.h>
#include <SPI.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
EthernetServer server(port);
EthernetClient client;

//----------------------------------------------------------------------------------------
//  WIFI
//----------------------------------------------------------------------------------------
#elif defined(WIFI)
#include <WiFi.h>

const char *ssid = "Livebox-BC90";
const char *password = "V9b7qzKFxdQfbMT4Pa";
IPAddress gateway(192, 168, 1, 1);  // passerelle par défaut
IPAddress subnet(255, 255, 255, 0); // masque de sous réseau
WiFiServer server(port);
WiFiClient client;

#endif

//----------------------------------------------------------------------------------------
//  Queues
//----------------------------------------------------------------------------------------

QueueHandle_t rxQueue;

//----------------------------------------------------------------------------------------
//  Tasks
//----------------------------------------------------------------------------------------

void rxTask(void *pvParameters);

void setup()
{
  // trace = false; // debug
  Serial.begin(115200);

  Serial.printf("\nProject   :    %s", PROJECT);
  Serial.printf("\nVersion   :    %s", VERSION);
  Serial.printf("\nAuteur    :    %s", AUTHOR);
  Serial.printf("\nFichier   :    %s", __FILE__);
  Serial.printf("\nCompiled  :    %s", __DATE__);
  Serial.printf(" - %s\n\n", __TIME__);

  MFXWaveform::setup();
  Centrale::setup(idCentrale, IN1_pin, IN2_pin, EN_pin);

  // Création de la queue (10 messages de 'BUFFER_SIZE'(100) octets)
  mfxQueue = xQueueCreate(10, BUFFER_SIZE);
  if (mfxQueue == NULL)
  {
    Serial.println("Erreur : impossible de creer la queue");
    while (1)
      ;
  }
  Serial.println("mfxQueue ok");

#if !defined(ETHERNET) && !defined(WIFI)
  Serial.print("Select a communication mode.");
  while (1)
  {
  }
#endif

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
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Waiting for WiFi connection : \n\n");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());
  Serial.printf("Port = %d\n", port);
  server.begin();

#endif

  Serial.printf("\n\nWaiting for connection from Rocrail.\n");

  while (!client) // listen for incoming clients
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

    //****************** instrances de Loco ***************************************************************

    // Create locomotives instances
    loco[0] = Loco::createLoco(1, "5519 CFL", 0x73, 0xF6, 0x88, 0x34);
    loco[1] = Loco::createLoco(2, "test", 0x73, 0xF6, 0x88, 0x35);

    Message::setup(loco, nbLocos);

    // Create queues
    rxQueue = xQueueCreate(50, BUFFER_SIZE * sizeof(byte));
    // txQueue = xQueueCreate(50, BUFFER_SIZE * sizeof(byte));
    // debugQueue = xQueueCreate(50, BUFFER_SIZE * sizeof(byte));  // Create debug queue

    // Create tasks
    xTaskCreatePinnedToCore(rxTask, "RxTask", 4 * 1024, NULL, 5, NULL, 0); // priority 5
    // xTaskCreatePinnedToCore(txTask, "TxTask", 4 * 1024, NULL, 3, NULL, 0);                  // priority 3
    // xTaskCreatePinnedToCore(debugFrameTask, "debugFrameTask", 2 * 1024, NULL, 1, NULL, 0);  // debug task with priority 1 on core 1

    void rxTask(void *pvParameters);
    // void txTask(void *pvParameters);
    // void debugFrameTask(void *pvParameters);  // Debug task
  }
} // End setup

void loop() {}

//----------------------------------------------------------------------------------------
//   TCPReceiveTask
//----------------------------------------------------------------------------------------

void rxTask(void *pvParameters)
{
  Message message;
  while (true)
  {
    if (client.connected() && client.available())
    {
      if (client.readBytes(rxBuffer, BUFFER_S) == BUFFER_S)
      {
        message.decodeMsg(rxBuffer);
        message.parse();
        // xQueueSend(debugQueue, &message, 10);  // send to debug queue
        // xQueueSend(rxQueue, rxBuffer, 10);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Avoid busy-waiting
  }
}
