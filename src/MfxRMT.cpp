
/*

   MfxRMT.cpp

   The track signal must never stop: a decoder expects a transition at least
   every 100 µs (except inside a synchronisation pattern).

   - signalTask() builds the next chunk while the current one is being sent:
     rmt_write_items(..., false) waits for the end of the previous chunk, then
     starts the new one at once.
   - A chunk is either a frame between two synchronisations, or a single
     synchronisation when there is nothing to send.
   - Between two chunks the RMT is briefly idle (a few tens of µs). Its idle
     level is set to the level of the end of the chunk being sent, so the
     line keeps its level: no spurious transition, only the last half-bit
     of a synchronisation is a little longer.

*/

#include "MfxRMT.h"
#include "esp_rom_gpio.h" // esp_rom_gpio_pad_select_gpio, esp_rom_gpio_connect_out_signal

#define RMT_CLOCK_DIVIDER 80          // 80 MHz / 80 = 1 µs per tick
static const uint16_t MFX_HALF_US = 50; // half of a bit

rmt_item32_t RMTChannel::sItems[2][MfxSignal::MAX_ITEMS];
uint8_t RMTChannel::sCurrent = 0;

void RMTChannel::setup(gpio_num_t IN1_pin, gpio_num_t IN2_pin)
{
    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_TX;
    config.channel = RMT_CHANNEL_0;
    config.clk_div = RMT_CLOCK_DIVIDER;
    config.gpio_num = (gpio_num_t)IN1_pin;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    // 3 blocks of 64 items: a whole chunk (MfxSignal::MAX_ITEMS = 154) fits in
    // the RMT memory, no refill by interrupt during the transmission.
    config.mem_block_num = 3;

    // Signal inversé : IN2 reçoit la sortie du RMT, inversée par la matrice GPIO.
    // esp_rom_gpio_* existe avec le core Arduino 2.x (ESP-IDF 4.4) comme 3.x (ESP-IDF 5),
    // contrairement à GPIO_PIN_MUX_REG et gpio_matrix_out, retirés du core 3.x.
    gpio_num_t gpioNum = (gpio_num_t)(IN2_pin);
    esp_rom_gpio_pad_select_gpio(gpioNum);
    gpio_set_direction(gpioNum, GPIO_MODE_OUTPUT);
    esp_rom_gpio_connect_out_signal(gpioNum, RMT_SIG_OUT0_IDX + RMT_CHANNEL_0, true, false);
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, ESP_INTR_FLAG_SHARED);

    // The queue is normally created in setup(); create it here only if it is missing
    if (mfxQueue == NULL)
        mfxQueue = xQueueCreate(10, BUFFER_SIZE);
    if (mfxQueue == NULL)
    {
        Serial.println("Error: Unable to create queue");
        while (1)
            ;
    }
    xTaskCreatePinnedToCore(signalTask, "Signal Task", 4 * 1024, NULL, 5, NULL, 1);
}

/*
 Copies the chunk into the free buffer and starts it as soon as the previous
 chunk has been sent.
*/
void RMTChannel::send(const MfxSignal &signal)
{
    rmt_item32_t *items = sItems[sCurrent];
    const uint16_t n = signal.count();
    for (uint16_t i = 0; i < n; i++)
    {
        const MfxItem &it = signal.item(i);
        items[i].level0 = it.level0;
        items[i].duration0 = MFX_HALF_US;
        items[i].level1 = it.level1;
        items[i].duration1 = MFX_HALF_US;
    }

    // Waits for the end of the previous chunk, then starts this one
    rmt_write_items(RMT_CHANNEL_0, items, n, false);

    // While this chunk is being sent: if the next one is a little late, the
    // line keeps the level this chunk ends with.
    rmt_set_idle_level(RMT_CHANNEL_0, true, signal.level() ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW);

    sCurrent ^= 1; // the other buffer is free again only after this chunk ends
}

void RMTChannel::signalTask(void *parameter)
{
    Serial.printf("Signal task running on core %d\n", xPortGetCoreID());

    static MfxSignal signal; // static: too large for the task stack
    uint8_t frame[BUFFER_SIZE];

    for (;;)
    {
        signal.clear(); // keeps the level reached by the previous chunk
        const bool startLevel = signal.level();

        if (xQueueReceive(mfxQueue, frame, 0) == pdPASS)
        {
            // Krauss 2.2.1: a frame between two synchronisations
            signal.sync();
            signal.frame(frame);
            signal.sync();
        }
        else
        {
            // Nothing to send: keep the line alive
            signal.sync();
        }

        if (signal.overflow())
        {
            Serial.println("MfxRMT: frame too long, not sent");
            signal.clear();
            signal.setLevel(startLevel); // the line is still at this level
            signal.sync();
        }

        send(signal);
    }
}
