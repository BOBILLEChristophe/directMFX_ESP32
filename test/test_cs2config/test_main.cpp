/*
  Unit tests of Cs2Config, run on a PC:  pio test -e native
*/

#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include "Cs2Config.h"

static Cs2Config::Loco sample(const char *name, uint8_t sid, uint32_t uid)
{
    Cs2Config::Loco l;
    memset(&l, 0, sizeof(l));
    l.name = name;
    l.sid = sid;
    l.mfxUid = uid;
    return l;
}

void test_crc_ccitt()
{
    const uint8_t s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_UINT16(0x29B1, Cs2Config::crcCcitt(s, sizeof(s), 0xFFFF));
}

void test_adler32()
{
    const char *s = "Wikipedia";
    TEST_ASSERT_EQUAL_UINT32(0x11E60398, Cs2Config::adler32((const uint8_t *)s, strlen(s)));
}

void test_function_types()
{
    TEST_ASSERT_EQUAL_UINT8(1, Cs2Config::cs2FunctionType(0, false, 0x03, 0x03, 0x00));   // head light
    TEST_ASSERT_EQUAL_UINT8(23, Cs2Config::cs2FunctionType(3, false, 0x08, 0x08, 0x00));  // running sound
    TEST_ASSERT_EQUAL_UINT8(18, Cs2Config::cs2FunctionType(4, false, 0x0B, 0x0B, 0x00));  // ABV
    TEST_ASSERT_EQUAL_UINT8(8, Cs2Config::cs2FunctionType(2, false, 0x0A, 0x0A, 0x00));   // shunting
    TEST_ASSERT_EQUAL_UINT8(9, Cs2Config::cs2FunctionType(2, false, 0x06, 0x20, 0x00));   // Telex
    TEST_ASSERT_EQUAL_UINT8(83, Cs2Config::cs2FunctionType(2, false, 0x00, 0x20, 0x80));  // Telex front
    TEST_ASSERT_EQUAL_UINT8(128 + 11, Cs2Config::cs2FunctionType(5, true, 0x07, 0x07, 0x08)); // whistle, momentary
    TEST_ASSERT_EQUAL_UINT8(5, Cs2Config::cs2FunctionType(6, false, 0x07, 0x07, 0x00));   // sound
    TEST_ASSERT_EQUAL_UINT8(50 + 8, Cs2Config::cs2FunctionType(8, false, 0x02, 0x02, 0x00)); // "F8"
    TEST_ASSERT_EQUAL_UINT8(50 + 1, Cs2Config::cs2FunctionType(1, false, 0x06, 0x21, 0x00)); // unknown: "F1"
    TEST_ASSERT_EQUAL_UINT8(0, Cs2Config::cs2FunctionType(9, false, 0x00, 0x00, 0x00));   // none
}

void test_loco_file()
{
    Cs2Config::Loco l[2] = {sample("150 Y 17 3R", 8, 0x73FA7401UL), sample("Serie 150X", 5, 0xFFFEBF74UL)};
    l[0].haveConfig = true;
    l[0].av = 12;
    l[0].bv = 12;
    l[0].vmin = 4;
    l[0].vmax = 180;
    l[0].tachoMax = 80;
    l[0].functions = 2;
    l[0].fnType[0] = 1;
    l[0].fnType[1] = 0; // none: not listed
    char text[2000];
    const size_t n = Cs2Config::locoFile(l, 2, text, sizeof(text));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL(strlen(text), n);
    TEST_ASSERT_TRUE(strncmp(text, "[lokomotive]\nversion\n .major=0\n .minor=1\n", 41) == 0);
    TEST_ASSERT_TRUE(strstr(text, "lokomotive\n .name=150 Y 17 3R\n .uid=0x4008\n .adresse=0x8\n .typ=mfx\n .sid=0x8\n .mfxuid=0x73fa7401\n") != NULL);
    TEST_ASSERT_TRUE(strstr(text, " .av=12\n .bv=12\n .vmin=4\n .vmax=180\n .tachomax=80\n") != NULL);
    TEST_ASSERT_TRUE(strstr(text, " .funktionen\n ..nr=0\n ..typ=1\n") != NULL);
    TEST_ASSERT_TRUE(strstr(text, "..nr=1\n") == NULL);
    TEST_ASSERT_TRUE(strstr(text, " .name=Serie 150X\n .uid=0x4005\n") != NULL);
    // Too small
    TEST_ASSERT_EQUAL(0, Cs2Config::locoFile(l, 2, text, 100));
}

// Reads back the stream: stored zlib blocks, adler32
static size_t inflateStored(const uint8_t *z, size_t zlen, uint8_t *out, size_t size)
{
    if (zlen < 6 || z[0] != 0x78 || ((z[0] << 8 | z[1]) % 31) != 0)
        return 0;
    size_t i = 2, o = 0;
    for (;;)
    {
        const uint8_t h = z[i++];
        const size_t n = z[i] | z[i + 1] << 8;
        const size_t nn = z[i + 2] | z[i + 3] << 8;
        if ((n ^ 0xFFFF) != nn || o + n > size)
            return 0;
        i += 4;
        memcpy(out + o, z + i, n);
        i += n;
        o += n;
        if (h & 1)
            break;
    }
    const uint32_t ad = (uint32_t)z[i] << 24 | (uint32_t)z[i + 1] << 16 | (uint32_t)z[i + 2] << 8 | z[i + 3];
    return ad == Cs2Config::adler32(out, o) ? o : 0;
}

void test_stream_round_trip()
{
    const size_t sizes[] = {0, 1, 7, 300, 70000};
    for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++)
    {
        const size_t n = sizes[k];
        uint8_t *text = (uint8_t *)malloc(n + 1);
        for (size_t i = 0; i < n; i++)
            text[i] = (uint8_t)('a' + i % 26);
        const size_t room = Cs2Config::streamSize(n);
        uint8_t *buf = (uint8_t *)malloc(room);
        uint32_t length = 0;
        uint16_t crc = 0;
        const size_t padded = Cs2Config::stream(text, n, buf, room, &length, &crc);
        TEST_ASSERT_EQUAL(room, padded);
        TEST_ASSERT_EQUAL(0, padded % 8);
        TEST_ASSERT_TRUE(length <= padded && padded - length < 8);
        TEST_ASSERT_EQUAL_UINT16(Cs2Config::crcCcitt(buf, padded, 0xFFFF), crc);
        // Size of the text, then the zlib data
        TEST_ASSERT_EQUAL_UINT32(n, (uint32_t)buf[0] << 24 | (uint32_t)buf[1] << 16 | (uint32_t)buf[2] << 8 | buf[3]);
        uint8_t *back = (uint8_t *)malloc(n + 1);
        TEST_ASSERT_EQUAL(n, inflateStored(buf + 4, length - 4, back, n + 1));
        TEST_ASSERT_TRUE(memcmp(back, text, n) == 0);
        for (size_t i = length; i < padded; i++)
            TEST_ASSERT_EQUAL_UINT8(0, buf[i]);
        // Too small a buffer
        TEST_ASSERT_EQUAL(0, Cs2Config::stream(text, n, buf, room - 1, &length, &crc));
        free(text);
        free(buf);
        free(back);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_ccitt);
    RUN_TEST(test_adler32);
    RUN_TEST(test_function_types);
    RUN_TEST(test_loco_file);
    RUN_TEST(test_stream_round_trip);
    return UNITY_END();
}
