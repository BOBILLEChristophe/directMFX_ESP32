
/*

   MfxRMT.cpp


*/

#include "MfxRMT.h"

#define RMT_CLOCK_DIVIDER 80
const uint16_t MFX_PERIODE = 50; // 50µs
rmt_item32_t *RMTChannel::item = (rmt_item32_t *)malloc(MAX_DATA_LEN * sizeof(rmt_item32_t));
byte RMTChannel::itemCount = 0;
volatile bool RMTChannel::receivedMsg = false;
byte RMTChannel::buff[BUFFER_SIZE];
// bool RMTChannel::isReady = true;
bool RMTChannel::level = LOW;

void RMTChannel::setMFXbit1(rmt_item32_t *item)
{
    level = !level;
    item->level0 = level;
    item->duration0 = MFX_PERIODE;
    level = !level;
    item->level1 = level;
    item->duration1 = MFX_PERIODE;
}

void RMTChannel::setMFXbit0(rmt_item32_t *item)
{
    level = !level;
    item->level0 = level;
    item->duration0 = MFX_PERIODE;
    item->level1 = level;
    item->duration1 = MFX_PERIODE;
}

void RMTChannel::setMFXsync(rmt_item32_t *item)
{
    // Affecte les valeurs au premier élément
    level = !level; // 1
    item[itemCount].level0 = level;
    item[itemCount].duration0 = MFX_PERIODE;
    item[itemCount].level1 = level;
    item[itemCount].duration1 = MFX_PERIODE;

    itemCount++;

    // Affecte les valeurs au deuxième élément
    level = !level; // 0
    item[itemCount].level0 = level;
    item[itemCount].duration0 = MFX_PERIODE;
    level = !level; // 1
    item[itemCount].level1 = level;
    item[itemCount].duration1 = MFX_PERIODE;

    itemCount++;

    // Affecte les valeurs au troisième élément
    item[itemCount].level0 = level;
    item[itemCount].duration0 = MFX_PERIODE;
    level = !level; // 0
    item[itemCount].level1 = level;
    item[itemCount].duration1 = MFX_PERIODE;

    itemCount++;

    // Affecte les valeurs au quatrième élément
    item[itemCount].level0 = level;
    item[itemCount].duration0 = MFX_PERIODE;
    level = !level; // 1
    item[itemCount].level1 = level;
    item[itemCount].duration1 = MFX_PERIODE;

    itemCount++;

    // Affecte les valeurs au cinquième élément
    level = !level; // 0
    item[itemCount].level0 = level;
    item[itemCount].duration0 = MFX_PERIODE;
    item[itemCount].level1 = level;
    item[itemCount].duration1 = MFX_PERIODE;

    itemCount++;
}

void RMTChannel::setup(gpio_num_t IN1_pin, gpio_num_t IN2_pin)
{
    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_TX;
    config.channel = RMT_CHANNEL_0;
    config.clk_div = RMT_CLOCK_DIVIDER;
    config.gpio_num = (gpio_num_t)IN1_pin;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.mem_block_num = 2;

    // Signal inversé
    gpio_num_t gpioNum = (gpio_num_t)(IN2_pin);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpioNum], PIN_FUNC_GPIO);
    gpio_set_direction(gpioNum, GPIO_MODE_OUTPUT);
    gpio_matrix_out(gpioNum, RMT_SIG_OUT0_IDX + RMT_CHANNEL_0, true, 0);
    rmt_config(&config);
    rmt_driver_install(config.channel, ESP_INTR_FLAG_LOWMED, ESP_INTR_FLAG_SHARED);

    mfxQueue = xQueueCreate(10, BUFFER_SIZE);
    if (mfxQueue == NULL)
    {
        Serial.println("Error: Unable to create queue");
        while (1)
            ;
    }
    xTaskCreatePinnedToCore(receiverTask, "Receiver Task", 4 * 1024, NULL, 5, NULL, 1);
}

// Tâche de réception de données
void RMTChannel::receiverTask(void *parameter)
{
    Serial.printf("Receiver Task running on core %d\n", xPortGetCoreID());
    byte receivedBuff[BUFFER_SIZE];
    while (true)
    {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        receivedMsg = false;
        // isReady = false;
        if (xQueueReceive(mfxQueue, (void *)receivedBuff, (TickType_t)0) == pdPASS)
        {
            receivedMsg = true;
            memcpy(buff, receivedBuff, BUFFER_SIZE);
        }
        sendToCentrale(buff);
    }
}

void RMTChannel::sendToCentrale(const byte *buff)
{
    byte stuffCount = 0;
    byte endCount = buff[0];
    itemCount = 0;
    // for (byte i = 0; i < 3; i++)
    setMFXsync(item); // Trame de synchro - Krauss 2.2.1 Codage des bits et synchronisation

    if (receivedMsg)
    {
        byte count = 0;
        do
        {
            count++;
            if (buff[count])
            {
                setMFXbit1(&item[itemCount++]);
                stuffCount++;
            }
            else
            {
                setMFXbit0(&item[itemCount++]);
                stuffCount = 0;
            }

            if (stuffCount == 8)
            {
                setMFXbit0(&item[itemCount++]);
                stuffCount = 0;
                endCount++;
            }
        } while (count < endCount);
    }
    if (itemCount)
    {
        // for (byte i = 0; i < itemCount; i++)
        // {
        //     if (item[i].level0 == 0 && item[i].level1 == 0)
        //         Serial.print("0");
        //     if (item[i].level0 == 1 && item[i].level1 == 1)
        //         Serial.print("0");
        //     if (item[i].level0 == 1 && item[i].level1 == 0)
        //         Serial.print("1");
        //     if (item[i].level0 == 0 && item[i].level1 == 1)
        //         Serial.print("1");
        // }
        // Serial.println();
        rmt_write_items(RMT_CHANNEL_0, item, itemCount, false);
        /* défini sur false, la fonction retourne immédiatement après avoir initié la transmission,
        permettant au programme de continuer son exécution sans attendre la fin de l'envoi.*/
    }
}
