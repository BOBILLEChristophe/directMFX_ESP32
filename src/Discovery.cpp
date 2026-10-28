/*

   Discovery.cpp

*/

#include "Discovery.h"
#include "Centrale.h"
#include "Feedback.h"
#include "Loco.h"
#include "Message.h"
#include "MfxFrame.h"

static const uint32_t PERIOD_MS = 2000;        // search for new decoders
static const uint32_t SAME_DECODER_MS = 30000; // pause if a decoder answers again
static const uint8_t TRIES = 2;                // a missed answer gives a wrong bit

void Discovery::setup()
{
    if (!Feedback::ready())
    {
        Serial.println("Discovery: no feedback, automatic discovery disabled");
        return;
    }
    xTaskCreatePinnedToCore(task, "Discovery", 4 * 1024, NULL, 4, NULL, 0);
}

/*
 Search: do decoders not yet registered have a UID starting with the first
 `count` bits of `uid`? A "no" is asked twice: one missed answer would give
 a wrong bit (the final check would then fail, and the search restart).
*/
bool Discovery::probe(uint8_t count, uint32_t uid)
{
    MfxFrame f;
    f.broadcast();
    f.search(count, uid);
    f.finish();
    for (uint8_t i = 0; i < TRIES; i++)
        if (Feedback::ask(f))
            return true;
    return false;
}

bool Discovery::findUid(uint32_t *found)
{
    if (!probe(0, 0))
        return false;

    // Bit by bit, from the most significant one: keep a 0 when a decoder
    // answers, a 1 otherwise.
    uint32_t uid = 0xFFFFFFFFUL;
    for (uint8_t count = 1; count <= 32; count++)
    {
        const uint32_t test = uid & ~(1UL << (32 - count));
        if (probe(count, test))
            uid = test;
    }

    if (!probe(32, uid))
        return false;
    *found = uid;
    return true;
}

bool Discovery::readName(uint16_t sid, char *name, size_t size)
{
    if (size == 0)
        return false;
    name[0] = 0;
    size_t len = 0;
    for (uint8_t index = 0x01; index < 0x11 && len + 1 < size; index += 4)
    {
        uint8_t b[4];
        if (!Feedback::readCV(sid, 0x003, index, 4, b))
            return len > 0;
        for (uint8_t i = 0; i < 4; i++)
        {
            if (b[i] == 0 || len + 1 >= size)
            {
                name[len] = 0;
                return true;
            }
            name[len++] = (b[i] >= 0x20 && b[i] < 0x7F) ? (char)b[i] : '?';
        }
        name[len] = 0;
    }
    return true;
}

/*
 Test of the reading of CVs: the name of each locomotive declared in
 main.cpp. Only the decoders registered with this command station answer.
*/
void Discovery::readDeclaredNames()
{
    for (uint8_t a = 1; a < 128; a++)
    {
        Loco *l = Loco::findLoco(a);
        if (l == nullptr)
            continue;
        char name[17];
        if (readName(a, name, sizeof(name)))
            Serial.printf("Discovery: address %u (%s): name read in the decoder: \"%s\"\n", a,
                          l->name().c_str(), name);
        else
            Serial.printf("Discovery: address %u (%s): no answer to the reading of the name\n", a,
                          l->name().c_str());
    }
}

static void sendBind(Loco *l)
{
    MfxFrame f;
    f.broadcast();
    f.bind(l->addr(), l->uid32());
    f.finish();
    uint8_t buff[BUFFER_SIZE];
    if (f.toBuffer(buff, sizeof(buff)))
        xQueueSendToFront(mfxQueue, buff, pdMS_TO_TICKS(10));
}

void Discovery::registerDecoder(uint32_t uid)
{
    Loco *l = Loco::findByUid(uid);
    if (l != nullptr)
    {
        // Declared in main.cpp but not registered (registered on another
        // command station, or a new counter): give it its address again
        Serial.printf("Discovery: decoder %08lX is %s (address %u), address sent again\n",
                      (unsigned long)uid, l->name().c_str(), l->addr());
        sendBind(l);
        return;
    }

    uint8_t address = 0;
    for (uint8_t a = 1; a < 128; a++)
        if (Loco::findLoco(a) == nullptr)
        {
            address = a;
            break;
        }
    if (address == 0)
    {
        Serial.println("Discovery: no free address");
        return;
    }

    char name[16];
    snprintf(name, sizeof(name), "mfx %08lX", (unsigned long)uid);
    l = Loco::createLoco(address, name, uid >> 24, uid >> 16, uid >> 8, uid);
    if (!Message::addLoco(l))
    {
        Serial.println("Discovery: too many locomotives, see MAX_LOCOS in main.cpp");
        return;
    }
    sendBind(l);

    // The name stored in the decoder, once it has its address
    vTaskDelay(pdMS_TO_TICKS(300));
    char decoderName[17] = "name";
    const bool named = readName(address, decoderName, sizeof(decoderName));

    Serial.printf("\nDiscovery: new decoder, UID %02X %02X %02X %02X -> address %u\n",
                  (uint8_t)(uid >> 24), (uint8_t)(uid >> 16), (uint8_t)(uid >> 8), (uint8_t)uid, address);
    if (named)
        Serial.printf("  Name read in the decoder: \"%s\"\n", decoderName);
    else
        Serial.println("  The name could not be read in the decoder");
    Serial.println("  Declare it in Rocrail (mfx, this address). To keep it after a restart, add to main.cpp:");
    Serial.printf("  loco[n] = Loco::createLoco(%u, \"%s\", 0x%02X, 0x%02X, 0x%02X, 0x%02X);\n\n",
                  address, named ? decoderName : "name", (uint8_t)(uid >> 24), (uint8_t)(uid >> 16),
                  (uint8_t)(uid >> 8), (uint8_t)uid);
}

void Discovery::task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(3000)); // let the decoders start
    uint32_t last = 0;
    TickType_t lastTime = 0;
    bool namesRead = false;

    for (;;)
    {
        if (Centrale::power() && !namesRead)
        {
            vTaskDelay(pdMS_TO_TICKS(2000)); // the decoders start
            readDeclaredNames();
            namesRead = true;
        }
        if (Centrale::power())
        {
            uint32_t uid = 0;
            if (findUid(&uid))
            {
                const TickType_t now = xTaskGetTickCount();
                if (uid == last && now - lastTime < pdMS_TO_TICKS(SAME_DECODER_MS))
                {
                    // It still answers after its Bind: do not search all the time
                    Serial.printf("Discovery: decoder %08lX still answers the search, pause\n",
                                  (unsigned long)uid);
                    vTaskDelay(pdMS_TO_TICKS(SAME_DECODER_MS));
                }
                else
                    registerDecoder(uid);
                last = uid;
                lastTime = xTaskGetTickCount();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}
