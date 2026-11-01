/*

   LocoStore.cpp

*/

#include "LocoStore.h"
#include "Loco.h"
#include "Message.h"
#include <Preferences.h>

static const char *NVS_NAMESPACE = "locos";
static const char *NVS_KEY = "list";
static const uint8_t MAX_STORED = 32;

// One record per locomotive. The layout is part of the stored data: a
// change needs a new key (or the old memory is read as garbage).
struct __attribute__((packed)) Record
{
    uint8_t addr;
    uint8_t uid[4];
    char name[17];
};

static uint8_t readRecords(Record *out, uint8_t max)
{
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) // read-write: works on the first boot too
        return 0;
    // isKey() first: getBytesLength() logs an error while nothing is stored
    const size_t len = prefs.isKey(NVS_KEY) ? prefs.getBytesLength(NVS_KEY) : 0;
    uint8_t n = 0;
    if (len > 0 && len % sizeof(Record) == 0 && len / sizeof(Record) <= max)
    {
        prefs.getBytes(NVS_KEY, out, len);
        n = (uint8_t)(len / sizeof(Record));
    }
    prefs.end();
    return n;
}

static uint32_t uidOf(const Record &r)
{
    return (uint32_t)r.uid[0] << 24 | (uint32_t)r.uid[1] << 16 | (uint32_t)r.uid[2] << 8 | r.uid[3];
}

void LocoStore::load()
{
    static Record records[MAX_STORED];
    const uint8_t n = readRecords(records, MAX_STORED);
    uint8_t added = 0;
    for (uint8_t i = 0; i < n; i++)
    {
        Record &r = records[i];
        r.name[sizeof(r.name) - 1] = 0;
        const uint32_t uid = uidOf(r);
        if (Loco::findByUid(uid) != nullptr)
            continue; // declared in main.cpp: main.cpp wins
        uint8_t address = r.addr;
        if (address == 0 || address > 127 || Loco::findLoco(address) != nullptr)
        {
            // Address taken by main.cpp: the first free one
            address = 0;
            for (uint8_t a = 1; a < 128; a++)
                if (Loco::findLoco(a) == nullptr)
                {
                    address = a;
                    break;
                }
            if (address == 0)
                break;
            Serial.printf("LocoStore: %s (UID %08lX): address %u taken, address %u given\n", r.name,
                          (unsigned long)uid, r.addr, address);
        }
        Loco *l = Loco::createLoco(address, String(r.name), r.uid[0], r.uid[1], r.uid[2], r.uid[3]);
        if (!Message::addLoco(l))
        {
            Serial.println("LocoStore: too many locomotives, see MAX_LOCOS in main.cpp");
            break;
        }
        added++;
    }
    Serial.printf("LocoStore: %u locomotive(s) in memory, %u added to those of main.cpp\n", n, added);
}

void LocoStore::save()
{
    static Loco copies[MAX_STORED];
    static Record records[MAX_STORED];
    const uint8_t n = Loco::list(copies, MAX_STORED);
    for (uint8_t i = 0; i < n; i++)
    {
        Record &r = records[i];
        memset(&r, 0, sizeof(r));
        r.addr = copies[i].addr();
        for (uint8_t b = 0; b < 4; b++)
            r.uid[b] = copies[i].UID(b);
        strncpy(r.name, copies[i].name().c_str(), sizeof(r.name) - 1);
    }
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false))
    {
        prefs.putBytes(NVS_KEY, records, n * sizeof(Record));
        prefs.end();
    }
}

void LocoStore::print()
{
    static Loco copies[MAX_STORED];
    static Record records[MAX_STORED];
    const uint8_t stored = readRecords(records, MAX_STORED);
    const uint8_t n = Loco::list(copies, MAX_STORED);
    Serial.printf("\nLocomotives (%u known, %u in memory):\n", n, stored);
    for (uint8_t i = 0; i < n; i++)
    {
        const uint32_t uid = copies[i].uid32();
        bool inMemory = false;
        for (uint8_t k = 0; k < stored; k++)
            if (uidOf(records[k]) == uid)
                inMemory = true;
        Serial.printf("  address %3u  UID %08lX  %-16s%s\n", copies[i].addr(), (unsigned long)uid,
                      copies[i].name().c_str(), inMemory ? "" : "  (main.cpp only)");
    }
}

void LocoStore::erase()
{
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false))
    {
        if (prefs.isKey(NVS_KEY))
            prefs.remove(NVS_KEY);
        prefs.end();
    }
}
