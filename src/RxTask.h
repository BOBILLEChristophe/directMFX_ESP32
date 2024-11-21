/*

   RxTask.h


*/

#ifndef RX_TASK_H
#define RX_TASK_H

#include "Message.h"
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// External variables needed for the task
extern WiFiClient client;
extern uint8_t rxBuffer[];
extern const uint8_t BUFFER_S;

void rxTask(void *pvParameters);

#endif // RX_TASK_H
