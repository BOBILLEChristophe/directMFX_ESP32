/*
  Cs2Config.cpp
*/

#include "Cs2Config.h"
#include <stdio.h>
#include <string.h>

/*
 CS2 function types (numbers of the CS2, as listed in loco-strings.h of
 the railroad project): 1 head light, 2 interior light, 4 high beam,
 5 sound, 8 shunting, 9 Telex, 11 conductor whistle, 18 ABV off,
 20 brake squeal, 21 switching stages, 22 generator, 23 running sound,
 24 motor, 25 station announcement, 31 firebox, 33-35 table lamps,
 36 shaking grate, 37 rail joint, 38 number plate, 40 train
 destination, 41/42 cab light rear/front, 43 coupling, 44 buffer,
 47 flashing light, 50 + n: "Fn", 82/83 Telex rear/front, 90 running
 gear light, 91 cylinder blow-out, 92 steam blow.
 mfx symbols: Krauss 6.4.4. Symbols without an equivalent give "Fn".
*/
uint8_t Cs2Config::cs2FunctionType(uint8_t fn, bool momentary, uint8_t group, uint8_t s1, uint8_t s2)
{
    uint8_t t = 0;
    (void)group;
    switch (s1)
    {
    case 0x00:
        t = (group == 0 && s2 == 0) ? 0 : 0xFF;
        break;
    case 0x03: // light
        t = s2 == 0x40 ? 41 : (s2 == 0x80 ? 42 : 1);
        break;
    case 0x04: // interior light
        t = s2 == 0x05 ? 35 : (s2 == 0x06 ? 34 : (s2 == 0x07 ? 33 : 2));
        break;
    case 0x05: // exterior light
        switch (s2)
        {
        case 0x01:
            t = 38;
            break;
        case 0x02:
            t = 40;
            break;
        case 0x03:
            t = 90;
            break;
        case 0x06:
            t = 47;
            break;
        case 0x09:
            t = 31;
            break;
        default:
            t = 4;
        }
        break;
    case 0x07: // sound
        switch (s2)
        {
        case 0x01:
            t = 20;
            break;
        case 0x05:
            t = 43;
            break;
        case 0x06:
            t = 37;
            break;
        case 0x08:
            t = 11;
            break;
        case 0x09:
            t = 21;
            break;
        case 0x0A:
            t = 91;
            break;
        case 0x0D:
        case 0x10:
            t = 36;
            break;
        case 0x0E:
            t = 22;
            break;
        case 0x0F:
            t = 44;
            break;
        case 0x13:
            t = 92;
            break;
        case 0x14:
            t = 24;
            break;
        default:
            t = 5;
        }
        break;
    case 0x08: // background sound
        t = 23;
        break;
    case 0x09: // announcement
        t = 25;
        break;
    case 0x0A: // shunting
        t = 8;
        break;
    case 0x0B: // direct control (ABV off)
        t = 18;
        break;
    case 0x20: // Telex
        t = s2 == 0x40 ? 82 : (s2 == 0x80 ? 83 : 9);
        break;
    default:
        t = 0xFF;
    }
    if (t == 0)
        return 0; // no function
    if (t == 0xFF)
        t = (uint8_t)(50 + fn); // "Fn"
    return momentary ? (uint8_t)(t + 128) : t;
}

// Appends with snprintf, keeps track of the room left
struct Writer
{
    char *p;
    size_t left;
    bool full;
    void put(const char *fmt, unsigned long a = 0, unsigned long b = 0)
    {
        if (full)
            return;
        const int n = snprintf(p, left, fmt, a, b);
        if (n < 0 || (size_t)n >= left)
        {
            full = true;
            return;
        }
        p += n;
        left -= (size_t)n;
    }
    void putText(const char *key, const char *text)
    {
        if (full)
            return;
        const int n = snprintf(p, left, "%s%s\n", key, text);
        if (n < 0 || (size_t)n >= left)
        {
            full = true;
            return;
        }
        p += n;
        left -= (size_t)n;
    }
};

size_t Cs2Config::locoFile(const Loco *locos, uint8_t count, char *out, size_t size)
{
    if (size == 0)
        return 0;
    Writer w = {out, size, false};
    w.put("[lokomotive]\nversion\n .major=0\n .minor=1\nsession\n .id=1\n");
    for (uint8_t i = 0; i < count; i++)
    {
        const Loco &l = locos[i];
        w.put("lokomotive\n");
        w.putText(" .name=", l.name);
        w.put(" .uid=0x%lx\n .adresse=0x%lx\n", 0x4000UL + l.sid, l.sid);
        w.put(" .typ=mfx\n .sid=0x%lx\n .mfxuid=0x%lx\n", l.sid, (unsigned long)l.mfxUid);
        w.putText(" .icon=", l.name);
        if (l.haveConfig)
        {
            w.put(" .av=%lu\n .bv=%lu\n", l.av, l.bv);
            w.put(" .vmin=%lu\n .vmax=%lu\n", l.vmin, l.vmax);
            if (l.tachoMax)
                w.put(" .tachomax=%lu\n", l.tachoMax);
        }
        for (uint8_t f = 0; f < l.functions && f < FUNCTIONS; f++)
        {
            if (l.fnType[f] == 0)
                continue;
            w.put(" .funktionen\n ..nr=%lu\n ..typ=%lu\n", f, l.fnType[f]);
        }
    }
    if (w.full)
        return 0;
    return (size_t)(w.p - out);
}

uint16_t Cs2Config::crcCcitt(const uint8_t *data, size_t n, uint16_t seed)
{
    uint16_t crc = seed;
    for (size_t i = 0; i < n; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint32_t Cs2Config::adler32(const uint8_t *data, size_t n)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++)
    {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return b << 16 | a;
}

static const size_t STORED_MAX = 65535;

static size_t zlibSize(size_t n)
{
    const size_t blocks = n == 0 ? 1 : (n + STORED_MAX - 1) / STORED_MAX;
    return 2 + blocks * 5 + n + 4;
}

size_t Cs2Config::streamSize(size_t textLen)
{
    const size_t raw = 4 + zlibSize(textLen);
    return (raw + 7) / 8 * 8;
}

size_t Cs2Config::stream(const uint8_t *text, size_t textLen, uint8_t *out, size_t size,
                         uint32_t *length, uint16_t *crc)
{
    const size_t padded = streamSize(textLen);
    if (size < padded)
        return 0;
    uint8_t *p = out;
    // Size of the text, big endian
    *p++ = (uint8_t)(textLen >> 24);
    *p++ = (uint8_t)(textLen >> 16);
    *p++ = (uint8_t)(textLen >> 8);
    *p++ = (uint8_t)textLen;
    // zlib header: deflate, 32 KB window, no dictionary, (0x78 0x01: 0x7801 % 31 == 0)
    *p++ = 0x78;
    *p++ = 0x01;
    size_t done = 0;
    do
    {
        const size_t n = (textLen - done) > STORED_MAX ? STORED_MAX : (textLen - done);
        const bool last = done + n == textLen;
        *p++ = last ? 0x01 : 0x00; // BFINAL, BTYPE = 00 (stored)
        *p++ = (uint8_t)n;
        *p++ = (uint8_t)(n >> 8);
        *p++ = (uint8_t)~n;
        *p++ = (uint8_t)(~n >> 8);
        memcpy(p, text + done, n);
        p += n;
        done += n;
    } while (done < textLen);
    const uint32_t ad = adler32(text, textLen);
    *p++ = (uint8_t)(ad >> 24);
    *p++ = (uint8_t)(ad >> 16);
    *p++ = (uint8_t)(ad >> 8);
    *p++ = (uint8_t)ad;
    const size_t raw = (size_t)(p - out);
    while ((size_t)(p - out) < padded)
        *p++ = 0;
    if (length)
        *length = (uint32_t)raw;
    if (crc)
        *crc = crcCcitt(out, padded, 0xFFFF);
    return padded;
}
