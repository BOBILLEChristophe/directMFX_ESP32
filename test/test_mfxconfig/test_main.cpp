/*
  Unit tests of MfxConfig, run on a PC:  pio test -e native

  A simulated decoder holds a configuration built like the example of
  Krauss (6.3.1) and the name read on a real decoder ("150 Y 17 3R").
*/

#include <unity.h>
#include <string.h>
#include "MfxConfig.h"

static uint8_t cvs[0x400][64]; // CV number x index
static int failCv = -1;        // this CV number never answers
static bool silent = false;    // no decoder at all
static int readsDone = 0;

static bool simRead(void *, uint16_t cv, uint8_t index, uint8_t bytes, uint8_t *out)
{
    readsDone++;
    if (silent || cv == failCv)
        return false;
    for (uint8_t i = 0; i < bytes; i++)
        out[i] = (index + i < 64) ? cvs[cv][index + i] : 0;
    return true;
}

static void put(uint16_t cv, const uint8_t *v, uint8_t n)
{
    memcpy(cvs[cv], v, n);
}

static void buildDecoder()
{
    memset(cvs, 0, sizeof(cvs));
    failCv = -1;
    silent = false;
    readsDone = 0;
    // Block 01 at CV 000: 1 group of 11 CAs
    const uint8_t b01[] = {0x01, 0x01, 0x01, 0x00, 0x01, 0x0B};
    put(0x000, b01, sizeof(b01));
    const uint8_t symbol[] = {0x16, 0x01, 0x09, 0x00, 0x00, 0x83, 0x03, 0x77, 0x02};
    put(0x002, symbol, sizeof(symbol));
    const uint8_t nameCa[] = {0x18, '1', '5', '0', ' ', 'Y', ' ', '1', '7', ' ', '3', 'R', 0};
    put(0x003, nameCa, sizeof(nameCa));
    // Block table: blocks at 05 x 4 = 014 (04), 0A x 4 = 028 (05), 0C x 4 = 030 (02)
    const uint8_t table[] = {0x17, 0x05, 0x0A, 0x0C, 0x00};
    put(0x004, table, sizeof(table));
    const uint8_t maker[] = {0x10, 0x00, 0x00, 0x00, 0x83, 0x00, 0x06, 0x0D, 0x9C};
    put(0x008, maker, sizeof(maker));
    const uint8_t fw[] = {0x13, 0x01, 0x02, 0x03};
    put(0x009, fw, sizeof(fw));

    // Block 04 at 014: 18 groups of 3 CAs
    const uint8_t b04[] = {0x01, 0x04, 0x01, 0x00, 0x12, 0x03};
    put(0x014, b04, sizeof(b04));
    const uint8_t f0[] = {0x10, 0x03, 0x03, 0x00}; // light
    put(0x015, f0, sizeof(f0));
    const uint8_t f1[] = {0x10, 0x87, 0x07, 0x08}; // momentary, sound, conductor whistle
    put(0x018, f1, sizeof(f1));
    // F2: no function (all 0)
    const uint8_t f3[] = {0x10, 0x0A, 0x0A, 0x00}; // shunting
    put(0x01E, f3, sizeof(f3));

    // Block 05 at 028: 1 group of 8 CAs
    const uint8_t b05[] = {0x01, 0x05, 0x01, 0x00, 0x01, 0x08};
    put(0x028, b05, sizeof(b05));
    const uint8_t motors[] = {0x10, 0x01};
    put(0x029, motors, sizeof(motors));
    const uint8_t acc[] = {0x13, 0x0C, 0x08};
    put(0x02A, acc, sizeof(acc));
    const uint8_t curve[] = {0x17, 0x05, 0xC8, 0x10};
    put(0x02B, curve, sizeof(curve));
    const uint8_t tacho[] = {0x18, 0x78, 0x00};
    put(0x02C, tacho, sizeof(tacho));

    // Block 02 at 030
    const uint8_t b02[] = {0x01, 0x02, 0x01, 0x00, 0x01, 0x03};
    put(0x030, b02, sizeof(b02));
}

void test_full_configuration()
{
    buildDecoder();
    MfxConfig c;
    TEST_ASSERT_EQUAL(MfxConfig::OK, c.read(simRead, nullptr));
    TEST_ASSERT_TRUE(strcmp(c.name, "150 Y 17 3R") == 0);

    TEST_ASSERT_EQUAL_UINT8(4, c.blockCount);
    TEST_ASSERT_TRUE(c.block(0x01) != nullptr);
    TEST_ASSERT_EQUAL_UINT16(0x014, c.block(0x04)->cv);
    TEST_ASSERT_EQUAL_UINT8(0x12, c.block(0x04)->groups);
    TEST_ASSERT_EQUAL_UINT8(3, c.block(0x04)->perGroup);
    TEST_ASSERT_EQUAL_UINT16(0x028, c.block(0x05)->cv);
    TEST_ASSERT_EQUAL_UINT16(0x030, c.block(0x02)->cv);
    TEST_ASSERT_TRUE(c.block(0x06) == nullptr);

    TEST_ASSERT_TRUE(c.haveManufacturer);
    TEST_ASSERT_EQUAL_UINT32(0x83, c.manufacturer);
    TEST_ASSERT_EQUAL_UINT32(0x00060D9C, c.product);

    TEST_ASSERT_EQUAL_UINT8(16, c.functionCount);
    TEST_ASSERT_TRUE(c.functions[0].present);
    TEST_ASSERT_FALSE(c.functions[0].momentary);
    TEST_ASSERT_EQUAL_UINT8(0x03, c.functions[0].group);
    TEST_ASSERT_TRUE(c.functions[1].present);
    TEST_ASSERT_TRUE(c.functions[1].momentary);
    TEST_ASSERT_EQUAL_UINT8(0x07, c.functions[1].group);
    TEST_ASSERT_EQUAL_UINT8(0x08, c.functions[1].s2);
    TEST_ASSERT_FALSE(c.functions[2].present);
    TEST_ASSERT_EQUAL_UINT8(0x0A, c.functions[3].group);
    TEST_ASSERT_FALSE(c.functions[15].present);

    TEST_ASSERT_TRUE(c.haveDriving);
    TEST_ASSERT_EQUAL_UINT8(0x0C, c.acceleration);
    TEST_ASSERT_EQUAL_UINT8(0x08, c.braking);
    TEST_ASSERT_EQUAL_UINT8(0x05, c.vmin);
    TEST_ASSERT_EQUAL_UINT8(0xC8, c.vmax);
    TEST_ASSERT_EQUAL_UINT16(120, c.tachoKmh);

    // Number of reads: about 45 (3 s on the track)
    TEST_ASSERT_EQUAL(readsDone, c.reads);
    TEST_ASSERT_LESS_THAN(60, c.reads);
    TEST_ASSERT_EQUAL(0, c.failures);
}

// Block 04 described as 1 group of 70 CAs (Marklin decoder of a 150 Y 17 3R)
void test_functions_in_one_group()
{
    buildDecoder();
    const uint8_t b04[] = {0x01, 0x04, 0x02, 0x00, 0x01, 0x46};
    put(0x014, b04, sizeof(b04));
    const uint8_t f15[] = {0x10, 0x07, 0x07, 0x01}; // brake sound
    put(0x014 + 1 + 15 * 3, f15, sizeof(f15));
    MfxConfig c;
    TEST_ASSERT_EQUAL(MfxConfig::OK, c.read(simRead, nullptr));
    TEST_ASSERT_EQUAL_UINT8(16, c.functionCount);
    TEST_ASSERT_TRUE(c.functions[0].present);
    TEST_ASSERT_TRUE(c.functions[1].momentary);
    TEST_ASSERT_EQUAL_UINT8(0x0A, c.functions[3].group);
    TEST_ASSERT_TRUE(c.functions[15].present);
    TEST_ASSERT_EQUAL_UINT8(0x01, c.functions[15].s2);
}

void test_no_decoder()
{
    buildDecoder();
    silent = true;
    MfxConfig c;
    TEST_ASSERT_EQUAL(MfxConfig::NO_ANSWER, c.read(simRead, nullptr));
    TEST_ASSERT_EQUAL(1, c.reads); // gives up at once
}

void test_no_block_table()
{
    buildDecoder();
    const uint8_t notTable[] = {0x15, 0x00};
    put(0x004, notTable, sizeof(notTable));
    MfxConfig c;
    TEST_ASSERT_EQUAL(MfxConfig::NO_BLOCK_TABLE, c.read(simRead, nullptr));
}

void test_missing_reads_give_incomplete()
{
    buildDecoder();
    failCv = 0x003; // the name never answers
    MfxConfig c;
    TEST_ASSERT_EQUAL(MfxConfig::INCOMPLETE, c.read(simRead, nullptr));
    TEST_ASSERT_EQUAL_UINT8(0, c.name[0]);
    TEST_ASSERT_TRUE(c.functions[0].present); // the rest is read
    TEST_ASSERT_EQUAL_UINT16(120, c.tachoKmh);
    TEST_ASSERT_GREATER_THAN(0, c.failures);
}

void test_name_of_16_characters()
{
    buildDecoder();
    const uint8_t nameCa[] = {0x18, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'};
    put(0x003, nameCa, sizeof(nameCa)); // index 17 reads 0
    MfxConfig c;
    c.read(simRead, nullptr);
    TEST_ASSERT_TRUE(strcmp(c.name, "ABCDEFGHIJKLMNOP") == 0);
}

void test_group_names()
{
    TEST_ASSERT_TRUE(strcmp(MfxConfig::groupName(0x03), "lumiere") == 0);
    TEST_ASSERT_TRUE(strcmp(MfxConfig::groupName(0x07), "son") == 0);
    TEST_ASSERT_TRUE(strcmp(MfxConfig::groupName(0x55), "?") == 0);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_full_configuration);
    RUN_TEST(test_functions_in_one_group);
    RUN_TEST(test_no_decoder);
    RUN_TEST(test_no_block_table);
    RUN_TEST(test_missing_reads_give_incomplete);
    RUN_TEST(test_name_of_16_characters);
    RUN_TEST(test_group_names);
    return UNITY_END();
}
