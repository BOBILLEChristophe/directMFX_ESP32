/*
  Unit tests of MfxSignal, run on a PC:  pio test -e native

  The reference encoder below is copied from MfxRMT.cpp of version 0.10.0.
  For frames without bit stuffing, MfxSignal must give exactly the same
  items. With bit stuffing, the old encoder sent one bit too many (read
  beyond the end of the frame); MfxSignal must send exactly the frame.
*/

#include <unity.h>
#include <string.h>
#include "MfxFrame.h"
#include "MfxSignal.h"

// ---------------------------------------------------------------------------
//  Reference: encoder of version 0.10.0 (MfxRMT.cpp)
// ---------------------------------------------------------------------------

struct RefItem
{
    bool level0, level1;
};
static RefItem refItems[200];
static uint16_t refCount;
static bool refLevel;

static void refBit1(RefItem *item)
{
    refLevel = !refLevel;
    item->level0 = refLevel;
    refLevel = !refLevel;
    item->level1 = refLevel;
}

static void refBit0(RefItem *item)
{
    refLevel = !refLevel;
    item->level0 = refLevel;
    item->level1 = refLevel;
}

static void refSync()
{
    refLevel = !refLevel;
    refItems[refCount].level0 = refLevel;
    refItems[refCount].level1 = refLevel;
    refCount++;
    refLevel = !refLevel;
    refItems[refCount].level0 = refLevel;
    refLevel = !refLevel;
    refItems[refCount].level1 = refLevel;
    refCount++;
    refItems[refCount].level0 = refLevel;
    refLevel = !refLevel;
    refItems[refCount].level1 = refLevel;
    refCount++;
    refItems[refCount].level0 = refLevel;
    refLevel = !refLevel;
    refItems[refCount].level1 = refLevel;
    refCount++;
    refLevel = !refLevel;
    refItems[refCount].level0 = refLevel;
    refItems[refCount].level1 = refLevel;
    refCount++;
}

// sendToCentrale() of 0.10.0, for a frame without stuffing
static void refSend(const uint8_t *buff)
{
    uint8_t stuffCount = 0;
    uint8_t endCount = buff[0];
    refCount = 0;
    refSync();
    uint8_t count = 0;
    do
    {
        count++;
        if (buff[count])
        {
            refBit1(&refItems[refCount++]);
            stuffCount++;
        }
        else
        {
            refBit0(&refItems[refCount++]);
            stuffCount = 0;
        }
        if (stuffCount == 8)
        {
            refBit0(&refItems[refCount++]);
            stuffCount = 0;
            endCount++;
        }
    } while (count < endCount);
}

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static uint8_t buff[130];

static void frameToBuffer(const MfxFrame &f)
{
    memset(buff, 0, sizeof(buff));
    TEST_ASSERT_TRUE(f.toBuffer(buff, sizeof(buff)));
}

// Decodes data items back to bits (0: no transition in the middle, 1: one).
// Checks that every item starts with a transition.
static uint8_t decode(const MfxSignal &s, uint16_t first, uint16_t n, bool startLevel, uint8_t *bits)
{
    bool level = startLevel;
    for (uint16_t i = 0; i < n; i++)
    {
        const MfxItem &it = s.item(first + i);
        TEST_ASSERT_TRUE_MESSAGE(it.level0 != level, "every bit starts with a transition");
        bits[i] = (it.level0 != it.level1) ? 1 : 0;
        level = it.level1;
    }
    return (uint8_t)n;
}

// ---------------------------------------------------------------------------
//  Tests
// ---------------------------------------------------------------------------

void setUp() {}
void tearDown() {}

void test_sync_same_as_reference_from_both_levels()
{
    for (int start = 0; start < 2; start++)
    {
        refLevel = start;
        refCount = 0;
        refSync();

        MfxSignal s;
        s.setLevel(start);
        s.sync();

        TEST_ASSERT_EQUAL_UINT16(5, s.count());
        for (uint16_t i = 0; i < 5; i++)
        {
            TEST_ASSERT_EQUAL(refItems[i].level0, s.item(i).level0);
            TEST_ASSERT_EQUAL(refItems[i].level1, s.item(i).level1);
        }
        TEST_ASSERT_EQUAL(refLevel, s.level());
        TEST_ASSERT_EQUAL(start != 0, s.level()); // a sync ends at its start level
    }
}

// Drive frame of the 150 X 5 at speed 12 with F0: no 8 consecutive 1 bits
void test_frame_without_stuffing_same_as_reference()
{
    MfxFrame f;
    f.address(5);
    f.drive(false, 12);
    f.functions16(0x0001);
    f.finish();
    frameToBuffer(f);

    refLevel = false;
    refSend(buff);

    MfxSignal s;
    s.sync();
    s.frame(buff);

    TEST_ASSERT_EQUAL_UINT16(5 + f.size(), s.count());
    TEST_ASSERT_EQUAL_UINT16(refCount, s.count());
    for (uint16_t i = 0; i < s.count(); i++)
    {
        TEST_ASSERT_EQUAL(refItems[i].level0, s.item(i).level0);
        TEST_ASSERT_EQUAL(refItems[i].level1, s.item(i).level1);
    }
}

// Bind of the 150 X 5: its UID starts with 11 bits at 1
void test_stuffing_sends_exactly_the_frame()
{
    MfxFrame f;
    f.broadcast();
    f.bind(5, 0xFFE9B194UL);
    f.finish();
    frameToBuffer(f);
    buff[buff[0] + 1] = 1; // garbage after the frame: must not be sent

    MfxSignal s;
    s.frame(buff);

    // Expected bits: the frame with a 0 after each run of 8 ones
    uint8_t expected[200];
    uint16_t n = 0;
    uint8_t ones = 0;
    for (uint8_t i = 1; i <= buff[0]; i++)
    {
        expected[n++] = buff[i];
        if (buff[i])
        {
            if (++ones == 8)
            {
                expected[n++] = 0;
                ones = 0;
            }
        }
        else
            ones = 0;
    }
    TEST_ASSERT_TRUE(n > buff[0]); // this frame does need stuffing
    TEST_ASSERT_EQUAL_UINT16(n, s.count());

    uint8_t bits[200];
    decode(s, 0, s.count(), false, bits);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, bits, n);

    // The old encoder sent one more bit, read after the end of the frame
    refLevel = false;
    refSend(buff);
    TEST_ASSERT_EQUAL_UINT16(5 + n + 1, refCount);
}

// Chunks sent one after the other join without a missing transition
void test_level_kept_between_chunks()
{
    MfxFrame f;
    f.address(1);
    f.drive(true, 127);
    f.functions16(0xFFFF); // long runs of 1: stuffing
    f.finish();
    frameToBuffer(f);

    MfxSignal s;
    bool levelBefore = s.level();
    for (int chunk = 0; chunk < 4; chunk++)
    {
        s.clear();
        s.sync();
        s.frame(buff);
        s.sync();
        // first item of the chunk starts with a transition
        TEST_ASSERT_TRUE(s.item(0).level0 != levelBefore);
        // last half of the chunk is the level kept for the next one
        TEST_ASSERT_EQUAL(s.item(s.count() - 1).level1, s.level());
        levelBefore = s.level();
    }
}

void test_overflow_detected()
{
    uint8_t big[130];
    big[0] = 128;
    for (uint8_t i = 1; i <= 128; i++)
        big[i] = 1; // 128 ones + 16 stuffing bits
    MfxSignal s;
    s.sync();
    s.frame(big);
    s.sync();
    TEST_ASSERT_FALSE(s.overflow());
    TEST_ASSERT_EQUAL_UINT16(5 + 128 + 16 + 5, s.count());
    // The longest frame still leaves room for the feedback request
    s.feedbackWindows(nullptr, nullptr);
    TEST_ASSERT_FALSE(s.overflow());
    while (s.count() < MfxSignal::MAX_ITEMS)
        s.bit0();
    TEST_ASSERT_FALSE(s.overflow());
    s.bit0();
    TEST_ASSERT_TRUE(s.overflow());
    TEST_ASSERT_EQUAL_UINT16(MfxSignal::MAX_ITEMS, s.count());
}

// Bits and synchronisations last 2 x 50 µs
void test_items_last_100_us()
{
    MfxSignal s;
    s.sync();
    s.bit0();
    s.bit1();
    for (uint16_t i = 0; i < s.count(); i++)
    {
        TEST_ASSERT_EQUAL_UINT16(MfxSignal::HALF_US, s.item(i).duration0);
        TEST_ASSERT_EQUAL_UINT16(MfxSignal::HALF_US, s.item(i).duration1);
    }
    TEST_ASSERT_EQUAL_UINT32(7 * 100, s.durationUs());
}

// hold() keeps the level, without any transition
void test_hold_keeps_level()
{
    for (int start = 0; start < 2; start++)
    {
        MfxSignal s;
        s.setLevel(start);
        s.hold(6400);
        TEST_ASSERT_EQUAL_UINT16(1, s.count());
        TEST_ASSERT_EQUAL(start != 0, s.item(0).level0);
        TEST_ASSERT_EQUAL(start != 0, s.item(0).level1);
        TEST_ASSERT_EQUAL_UINT32(6400, s.durationUs());
        TEST_ASSERT_EQUAL(start != 0, s.level());
    }
}

// Decoder search C = 0 followed by the feedback request
void test_feedback_windows()
{
    MfxFrame f;
    f.broadcast();
    f.search(0, 0);
    f.finish();
    frameToBuffer(f);
    TEST_ASSERT_EQUAL_UINT8(9 + 6 + 6 + 32 + 8, f.size());

    MfxSignal s;
    s.sync();
    s.frame(buff);
    const uint16_t frameItems = s.count() - 5; // no stuffing: only zeros
    TEST_ASSERT_EQUAL_UINT16(f.size(), frameItems);

    uint32_t window = 0, reference = 0;
    s.feedbackWindows(&window, &reference);
    TEST_ASSERT_FALSE(s.overflow());

    // sync + frame + 11 syncs + 0011, 100 µs each
    const uint16_t before = 5 + frameItems + 5 * MfxSignal::FEEDBACK_SYNCS + 4;
    TEST_ASSERT_EQUAL_UINT32(before * 100UL, window);
    TEST_ASSERT_EQUAL_UINT32(window + MfxSignal::FEEDBACK_WINDOW_US + 500, reference);
    TEST_ASSERT_EQUAL_UINT32(reference + MfxSignal::FEEDBACK_WINDOW_US + 5 * 100 * MfxSignal::FEEDBACK_END_SYNCS,
                             s.durationUs());

    // The bits 0011 just before the window, and no transition in the windows
    uint8_t bits[4];
    decode(s, before - 4, 4, s.item(before - 5).level1, bits);
    const uint8_t expected[4] = {0, 0, 1, 1};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, bits, 4);

    const MfxItem &w = s.item(before);
    TEST_ASSERT_EQUAL(w.level0, w.level1);
    TEST_ASSERT_EQUAL(s.item(before - 1).level1, w.level0); // no transition at the start
    TEST_ASSERT_EQUAL_UINT32(MfxSignal::FEEDBACK_WINDOW_US, (uint32_t)w.duration0 + w.duration1);

    const MfxItem &r = s.item(before + 1 + 5);
    TEST_ASSERT_EQUAL(r.level0, r.level1);
    TEST_ASSERT_EQUAL(s.item(before + 5).level1, r.level0);

    // The chunk ends at the level it reached, ready for the next one
    TEST_ASSERT_EQUAL(s.item(s.count() - 1).level1, s.level());
}

// Read CV (address 1, CV 003/01, 4 bytes) followed by the data feedback request
void test_data_window()
{
    for (uint8_t bytes = 1; bytes <= 8; bytes *= 2)
    {
        MfxFrame f;
        f.address(1);
        f.readCV(0x003, 0x01, bytes);
        f.finish();
        frameToBuffer(f);

        MfxSignal s;
        s.sync();
        s.frame(buff);
        const uint16_t frameItems = s.count() - 5;
        uint32_t pause = 0;
        s.dataWindow(bytes, &pause);
        TEST_ASSERT_FALSE(s.overflow());

        // sync + frame + 11 syncs + 0011, 100 µs each
        const uint16_t before = 5 + frameItems + 5 * MfxSignal::FEEDBACK_SYNCS + 4;
        TEST_ASSERT_EQUAL_UINT32(before * 100UL, pause);
        uint8_t bits[4];
        decode(s, before - 4, 4, s.item(before - 5).level1, bits);
        const uint8_t expected[4] = {0, 0, 1, 1};
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, bits, 4);

        // The pause: one pulse at the end of each period, 23 x 912 µs then 456 µs
        const bool l = s.item(before - 1).level1;
        const uint16_t pulses = MfxSignal::DATA_LEAD_CLOCKS + 2 * MfxSignal::dataClocks(bytes);
        uint32_t t = pause;
        for (uint16_t i = 0; i < pulses; i++)
        {
            const MfxItem &it = s.item(before + i);
            TEST_ASSERT_EQUAL(l, it.level0);  // no transition at the start of the pause
            TEST_ASSERT_EQUAL(!l, it.level1); // the pulse
            TEST_ASSERT_EQUAL_UINT16(MfxSignal::DATA_PULSE_US, it.duration1);
            const uint32_t period = i < MfxSignal::DATA_LEAD_CLOCKS ? 912 : 456;
            TEST_ASSERT_EQUAL_UINT32(period, (uint32_t)it.duration0 + it.duration1);
            t += period;
        }
        TEST_ASSERT_EQUAL_UINT32(pause + MfxSignal::dataPauseUs(bytes), t);
        const MfxItem &back = s.item(before + pulses);
        TEST_ASSERT_EQUAL(l, back.level0);
        TEST_ASSERT_EQUAL(l, back.level1);

        // Then one synchronisation, and the chunk ends at the pause level
        TEST_ASSERT_EQUAL_UINT16(before + pulses + 1 + 5, s.count());
        TEST_ASSERT_EQUAL_UINT32(t + 50 + 500, s.durationUs());
        TEST_ASSERT_EQUAL(l, s.level());
    }
    // Pause lengths measured on the Gleisbox: 49.1 ms (2 bytes), 63.5 ms (4 bytes)
    TEST_ASSERT_EQUAL_UINT32(49248, MfxSignal::dataPauseUs(2));
    TEST_ASSERT_EQUAL_UINT32(63840, MfxSignal::dataPauseUs(4));
}

// The longest frame with a data feedback of 8 bytes still fits
void test_longest_data_chunk_fits()
{
    uint8_t big[130];
    big[0] = 128;
    for (uint8_t i = 1; i <= 128; i++)
        big[i] = 1;
    MfxSignal s;
    s.sync();
    s.frame(big);
    s.dataWindow(8, nullptr);
    TEST_ASSERT_FALSE(s.overflow());
    TEST_ASSERT_EQUAL_UINT16(MfxSignal::MAX_ITEMS, s.count());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_sync_same_as_reference_from_both_levels);
    RUN_TEST(test_frame_without_stuffing_same_as_reference);
    RUN_TEST(test_stuffing_sends_exactly_the_frame);
    RUN_TEST(test_level_kept_between_chunks);
    RUN_TEST(test_overflow_detected);
    RUN_TEST(test_items_last_100_us);
    RUN_TEST(test_hold_keeps_level);
    RUN_TEST(test_feedback_windows);
    RUN_TEST(test_data_window);
    RUN_TEST(test_longest_data_chunk_fits);
    return UNITY_END();
}
