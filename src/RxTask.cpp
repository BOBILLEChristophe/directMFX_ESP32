/*

  RxTask.cpp


*/

#include "RxTask.h"

void rxTask(void *pvParameters)
{
    while (true)
    {
        // Reads every complete frame available, then waits a little
        while (readRocrailFrame(rxBuffer, BUFFER_S))
        {
            Message::decodeMsg(rxBuffer);
            Message::parse();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Avoid busy-waiting
    }
}
