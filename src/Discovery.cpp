/*

   Discovery.cpp

*/

#include "Discovery.h"
#include "Centrale.h"
#include "Feedback.h"
#include "Loco.h"
#include "Message.h"
#include "MfxFrame.h"
#include "MfxConfig.h"
#include "Cs2Config.h"

static const uint32_t PERIOD_MS = 2000;        // search for new decoders
static const uint32_t SAME_DECODER_MS = 30000; // pause if a decoder answers again
static const uint8_t TRIES = 2;                // a missed answer gives a wrong bit
static const uint32_t POWERED_MS = 3000;       // track on before reading the configurations
static const uint32_t RETRY_MS = 20000;        // a decoder that did not answer is tried again

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
    // Count 0: every decoder not yet registered answers, whatever the UID.
    // The UID is all ones, as a Marklin MS2 sends it.
    if (!probe(0, 0xFFFFFFFFUL))
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

/*
 The noise of a running motor spoils the data feedback (a third of the
 reads failed on a 150 Y at speed): the configuration is read only while
 the locomotive stands still. If it starts during the reading, the reading
 stops and is done again later.
*/
static volatile bool sMoving = false;

static bool readFromTrack(void *ctx, uint16_t cv, uint8_t index, uint8_t bytes, uint8_t *out)
{
    const uint16_t sid = (uint16_t)(uintptr_t)ctx;
    if (sMoving)
        return false;
    Loco *l = Loco::findLoco(sid);
    if (!Centrale::power() || (l != nullptr && l->speed() != 0))
    {
        sMoving = true; // running, or the track was switched off: later
        return false;
    }
    return Feedback::readCV(sid, cv, index, bytes, out);
}

// Name of the locomotive, printable (UTF-8 kept, control bytes replaced)
static void printable(const char *in, char *out, size_t size)
{
    size_t i = 0;
    for (; in[i] && i + 1 < size; i++)
        out[i] = ((uint8_t)in[i] < 0x20 || in[i] == 0x7F) ? '?' : in[i];
    out[i] = 0;
}

bool Discovery::readConfig(uint16_t sid, char *nameOut, size_t size)
{
    static MfxConfig c; // static: keeps the task stack small (one task only)
    const uint32_t t0 = millis();
    sMoving = false;
    const MfxConfig::Status st = c.read(readFromTrack, (void *)(uintptr_t)sid);
    if (sMoving)
    {
        Serial.printf("Discovery: address %u is running (or the track is off), configuration read later\n", sid);
        return false;
    }
    if (st == MfxConfig::NO_ANSWER)
        return false;

    char name[17];
    printable(c.name, name, sizeof(name));

    // Kept with the locomotive, for the loco list sent to Rocrail
    MfxInfo info;
    memset(&info, 0, sizeof(info));
    info.valid = true;
    info.av = c.acceleration;
    info.bv = c.braking;
    info.vmin = c.vmin;
    info.vmax = c.vmax;
    info.tachoMax = c.tachoKmh;
    info.functions = c.functionCount;
    for (uint8_t f = 0; f < c.functionCount && f < 16; f++)
    {
        const MfxConfig::Function &fn = c.functions[f];
        info.fnType[f] = fn.present ? Cs2Config::cs2FunctionType(f, fn.momentary, fn.group, fn.s1, fn.s2) : 0;
    }
    if (Loco *l = Loco::findLoco(sid))
    {
        l->mfxInfo(info);
        Message::announceBind(l->uid32(), sid); // a test: does Rocrail create the locomotive?
    }
    if (nameOut && size)
    {
        strncpy(nameOut, name, size - 1);
        nameOut[size - 1] = 0;
    }

    Serial.printf("Decoder at address %u: \"%s\"", sid, name);
    if (c.haveManufacturer)
        Serial.printf(", manufacturer %02lX%s, product %08lX", (unsigned long)c.manufacturer,
                      c.manufacturer == 0x83 ? " (Marklin/Trix)" : (c.manufacturer == 0x97 ? " (ESU)" : ""),
                      (unsigned long)c.product);
    Serial.println();
    if (st == MfxConfig::NO_BLOCK_TABLE)
        Serial.println("  no block table at CV 004: structure not read");

    Serial.print("  blocks:");
    for (uint8_t i = 0; i < c.blockCount; i++)
        Serial.printf(" %02X@%03X", c.blocks[i].type, c.blocks[i].cv);
    Serial.println();

    for (uint8_t f = 0; f < c.functionCount; f++)
    {
        const MfxConfig::Function &fn = c.functions[f];
        if (!fn.present)
            continue;
        Serial.printf("  F%-2u %-20s symbol %02X %02X %02X%s\n", f, MfxConfig::groupName(fn.group),
                      fn.group, fn.s1, fn.s2, fn.momentary ? ", momentary" : "");
    }
    if (c.haveDriving)
        Serial.printf("  acceleration %.2f s, braking %.2f s, Vmin %u, Vmax %u, tachometer %u km/h\n",
                      c.acceleration * 0.25f, c.braking * 0.25f, c.vmin, c.vmax, c.tachoKmh);
    Serial.printf("  block 01 (%u CAs), types:", c.block01Size);
    for (uint8_t k = 0; k < c.block01Size && k < sizeof(c.block01Types); k++)
        if (c.block01Types[k] == 0xFF)
            Serial.print(" --");
        else
            Serial.printf(" %02X", c.block01Types[k]);
    Serial.println();
    if (const MfxConfig::Block *f = c.block(0x04))
        Serial.printf("  block 04: %u groups of %u CAs, %u functions read\n", f->groups, f->perGroup,
                      c.functionCount);
    Serial.printf("  %u reads, %u failed, %.1f s", c.reads, c.failures, (millis() - t0) / 1000.0f);
    for (uint8_t i = 0; i < c.failures && i < MfxConfig::MAX_FAILED; i++)
        Serial.printf("%s %03X/%02X", i ? "," : " (failed:", c.failedCv[i], c.failedIndex[i]);
    Serial.println(c.failures ? ")" : "");
    return true;
}

/*
 Test of the reading of CVs: the configuration of each locomotive declared
 in main.cpp. Only the decoders registered with this command station answer.
 A decoder that does not answer is tried again later (READ_TRIES in all):
 it may be off the track, or still starting.
 Returns true when nothing is left to try.
*/
static const uint8_t READ_TRIES = 3;
static uint8_t sReadTries[128]; // tries per address, READ_TRIES + 1 once read

bool Discovery::readDeclaredConfigs()
{
    bool left = false;
    for (uint8_t a = 1; a < 128; a++)
    {
        Loco *l = Loco::findLoco(a);
        if (l == nullptr || sReadTries[a] >= READ_TRIES)
            continue;
        if (!Centrale::power())
            return false; // the track was switched off: later
        if (l->speed() != 0)
        {
            left = true; // running: later, without counting a try
            continue;
        }
        sReadTries[a]++;
        if (readConfig(a))
            sReadTries[a] = READ_TRIES; // done
        else if (sMoving)
        {
            sReadTries[a]--; // started during the reading: not a try
            left = true;
        }
        else
        {
            // Printed once, after the last try: absent locomotives would
            // otherwise give a line every RETRY_MS
            if (sReadTries[a] < READ_TRIES)
                left = true;
            else
                Serial.printf("Discovery: address %u (%s): no answer after %u tries, configuration not read\n",
                              a, l->name().c_str(), READ_TRIES);
        }
    }
    return !left;
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
    const bool named = readConfig(address, decoderName, sizeof(decoderName)) && decoderName[0];
    if (named)
    {
        l->name(decoderName);          // the name stored in the decoder, for Rocrail
        sReadTries[address] = READ_TRIES; // already read: not read again
    }

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

/*
 Commands typed in the serial monitor (one letter, then Enter):
   R : re-registration of every decoder. The re-registration counter
       (Krauss 3.2.6) is increased and kept in NVS, then the command station
       restarts: the decoders see a new counter when the track comes back
       and register again. Declared locomotives get their address back; the
       others get a free address.
   V : existence query (ping, Krauss 3.2.5) of every declared locomotive,
       with the counts of the 1-bit feedback: does its decoder answer, and
       how strongly?
   ? : this help.
*/
static void verifyAll()
{
    for (uint8_t a = 1; a < 128; a++)
    {
        Loco *l = Loco::findLoco(a);
        if (l == nullptr)
            continue;
        MfxFrame f;
        f.address(a);
        f.verify(l->uid32());
        f.finish();
        Feedback::Result r;
        const bool yes = Feedback::ask(f, &r);
        Serial.printf("Verify address %u (%s, UID %08lX): %s, %u regular intervals in the window, %u in the reference\n",
                      a, l->name().c_str(), (unsigned long)l->uid32(), yes ? "ANSWER" : "no answer",
                      r.windowRegular, r.referenceRegular);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void console()
{
    while (Serial.available() > 0)
    {
        const int c = Serial.read();
        if (c == 'R')
        {
            const uint16_t next = Centrale::counter() + 1;
            Centrale::setCounter(next);
            Serial.printf("\nRe-registration of every decoder: counter %u, restart...\n", next);
            Serial.flush();
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP.restart();
        }
        else if (c == 'V')
        {
            if (Centrale::power())
                verifyAll();
            else
                Serial.println("Verify: the track is off");
        }
        else if (c == '?')
            Serial.printf("\nCommands: R = re-registration of every decoder (counter %u -> %u, restart), "
                          "V = verify every declared locomotive, ? = help\n",
                          Centrale::counter(), Centrale::counter() + 1);
    }
}

void Discovery::task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(3000)); // let the decoders start
    uint32_t last = 0;
    TickType_t lastTime = 0;
    bool configsDone = false;
    TickType_t lastConfigs = 0;

    for (;;)
    {
        console();

        // The configurations are read once the track has been on for
        // POWERED_MS without a break (the decoders must have started),
        // then again every RETRY_MS for the decoders that did not answer.
        const TickType_t now = xTaskGetTickCount();
        if (!configsDone && Centrale::poweredForMs() >= POWERED_MS &&
            (lastConfigs == 0 || now - lastConfigs >= pdMS_TO_TICKS(RETRY_MS)))
        {
            configsDone = readDeclaredConfigs();
            lastConfigs = xTaskGetTickCount();
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
