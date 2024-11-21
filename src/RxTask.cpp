#include "RxTask.h"
#include <Arduino.h>

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
