/*
  Unit tests of MfxDemod, run on a PC:  pio test -e native

  1. Answers recorded on the track of a Marklin Gleisbox (recorded.h).
  2. Synthetic answers: carrier with phase changes, noise, clock pulses.
*/

#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "MfxDemod.h"
#include "recorded.h"

static MfxDemod demod;

static void checkRecorded(const uint32_t *t, uint16_t n, uint8_t bytes, const uint8_t *expected)
{
    uint8_t out[8] = {0};
    const MfxDemod::Status s = demod.decode(t, n, 10.0f, bytes, out);
    TEST_ASSERT_EQUAL_MESSAGE(MfxDemod::OK, s, MfxDemod::statusText(s));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, bytes);
}

void test_recorded_answers()
{
    const uint8_t a[] = {0x01, 0x01};             // CV 000/00: block 01 starts here
    checkRecorded(cv_000_00, sizeof(cv_000_00) / 4, 2, a);
    const uint8_t b[] = {0x01, 0x06};             // CV 080/00: block 06 starts here
    checkRecorded(cv_080_00, sizeof(cv_080_00) / 4, 2, b);
    const uint8_t c[] = {'1', '5', '0', ' '};     // CV 003/01: name of the locomotive
    checkRecorded(cv_003_01, sizeof(cv_003_01) / 4, 4, c);
    const uint8_t d[] = {0x07, 0x29};
    checkRecorded(cv_02F_01, sizeof(cv_02F_01) / 4, 2, d);
    // 23 clocks at 1 before the preamble
    TEST_ASSERT_TRUE(strncmp(demod.bits(), "11111111111111111111111010", 26) == 0 ||
                     strncmp(demod.bits(), "1111111111111111111111010", 25) == 0);
}

void test_checksum()
{
    // Krauss 2.3.4: for one byte, data ^ checksum == 0xF3
    for (int v = 0; v < 256; v++)
    {
        const uint8_t b = (uint8_t)v;
        TEST_ASSERT_EQUAL_HEX8(0xF3, b ^ MfxDemod::checksum(&b, 1));
    }
    const uint8_t two[] = {0x01, 0x01};
    TEST_ASSERT_EQUAL_HEX8(0xD1, MfxDemod::checksum(two, 2)); // recorded
}

// ---------------------------------------------------------------------------
//  Synthetic answers
// ---------------------------------------------------------------------------

static uint32_t syn[8000];
static uint16_t synCount;

static double rnd() { return (double)rand() / RAND_MAX; }

// Bits of the answer: 23 ones, 010, data, checksum, 4 ones
static uint16_t answerBits(const uint8_t *data, uint8_t bytes, char *bits)
{
    uint16_t k = 0;
    for (int i = 0; i < 23; i++)
        bits[k++] = 1;
    bits[k++] = 0;
    bits[k++] = 1;
    bits[k++] = 0;
    uint8_t all[9];
    memcpy(all, data, bytes);
    all[bytes] = MfxDemod::checksum(data, bytes);
    for (int b = 0; b <= bytes; b++)
        for (int j = 7; j >= 0; j--)
            bits[k++] = (all[b] >> j) & 1;
    for (int i = 0; i < 4; i++)
        bits[k++] = 1;
    return k;
}

/*
 Rising edges of a carrier of period `period` µs whose phase changes by
 180° at every clock and in the middle of the clocks of the 0 bits.
 jitter: noise on each edge (µs). pulses: a few false edges at each clock
 pulse of the command station. startUs: the carrier starts later.
*/
static void synth(const uint8_t *data, uint8_t bytes, double period, double halfUs, double jitter,
                  bool pulses, double startUs)
{
    char bits[140];
    const uint16_t nb = answerBits(data, bytes, bits);
    // Times of the phase changes
    double changes[300];
    uint16_t nc = 0;
    for (uint16_t i = 0; i < nb; i++)
    {
        changes[nc++] = (2 * i) * halfUs;
        if (!bits[i])
            changes[nc++] = (2 * i + 1) * halfUs;
    }
    const double end = 2 * nb * halfUs;
    synCount = 0;
    double phaseShift = 0;
    uint16_t c = 1; // changes[0] = 0 is the start
    for (double t = 0.3 * period; t < end && synCount < 8000; t += period)
    {
        while (c < nc && changes[c] <= t)
        {
            phaseShift = phaseShift == 0 ? 0.5 : 0;
            c++;
        }
        const double e = t + phaseShift * period + (rnd() - 0.5) * 2 * jitter;
        if (e < startUs)
            continue;
        if (pulses && fmod(t, halfUs) < period && rnd() < 0.7)
        {
            // ringing of the tuned circuit at a pulse: two false edges
            syn[synCount++] = (uint32_t)((e - 7) * 240 + 1000000);
            syn[synCount++] = (uint32_t)((e - 3) * 240 + 1000000);
        }
        syn[synCount++] = (uint32_t)(e * 240 + 1000000); // ESP32 cycles, any origin
    }
}

void test_synthetic_all_sizes()
{
    srand(1);
    const uint8_t data[8] = {0x00, 0xFF, 0x55, 0xAA, 0x31, 0x80, 0x01, 0x7E};
    for (uint8_t bytes = 1; bytes <= 8; bytes *= 2)
    {
        synth(data, bytes, 19.0, 456, 0.5, false, 0);
        uint8_t out[8] = {0};
        const MfxDemod::Status s = demod.decode(syn, synCount, 240.0f, bytes, out);
        TEST_ASSERT_EQUAL_MESSAGE(MfxDemod::OK, s, MfxDemod::statusText(s));
        TEST_ASSERT_EQUAL_HEX8_ARRAY(data, out, bytes);
    }
}

void test_synthetic_noise_drift_pulses()
{
    srand(2);
    const uint8_t data[4] = {0x13, 0x37, 0xC0, 0xDE};
    // carrier 1 % off, clock of the station 1 % short, noise, pulses, late start
    synth(data, 4, 19.2, 451.5, 1.5, true, 700);
    uint8_t out[4] = {0};
    const MfxDemod::Status s = demod.decode(syn, synCount, 240.0f, 4, out);
    TEST_ASSERT_EQUAL_MESSAGE(MfxDemod::OK, s, MfxDemod::statusText(s));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(data, out, 4);
}

// 200 random answers: carrier 18.5 to 19.5 µs, clock 444 to 460 µs, noise
// up to ±2 µs on each edge (interrupt latency), false edges at the pulses
void test_random_answers()
{
    for (int seed = 0; seed < 200; seed++)
    {
        srand(seed);
        uint8_t data[8];
        for (int i = 0; i < 8; i++)
            data[i] = (uint8_t)rand();
        const uint8_t bytes = (uint8_t)(1 << (seed % 4));
        synth(data, bytes, 18.5 + rnd(), 444 + 16 * rnd(), 2 * rnd(), true, 900 * rnd());
        uint8_t out[8] = {0};
        const MfxDemod::Status s = demod.decode(syn, synCount, 240.0f, bytes, out);
        TEST_ASSERT_EQUAL_MESSAGE(MfxDemod::OK, s, MfxDemod::statusText(s));
        TEST_ASSERT_EQUAL_HEX8_ARRAY(data, out, bytes);
    }
}

// The recorded answer with ±1.5 µs of noise added to each edge
void test_recorded_with_latency()
{
    srand(7);
    const uint16_t n = sizeof(cv_003_01) / 4;
    for (uint16_t i = 0; i < n; i++)
        syn[i] = cv_003_01[i] + (uint32_t)(15 + (rnd() - 0.5) * 30);
    uint8_t out[4] = {0};
    const MfxDemod::Status s = demod.decode(syn, n, 10.0f, 4, out);
    TEST_ASSERT_EQUAL_MESSAGE(MfxDemod::OK, s, MfxDemod::statusText(s));
    const uint8_t name[] = {'1', '5', '0', ' '};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(name, out, 4);
}

void test_errors_are_detected()
{
    uint8_t out[8];
    // No carrier
    TEST_ASSERT_EQUAL(MfxDemod::NO_CARRIER, demod.decode(syn, 10, 240.0f, 2, out));

    // Cut in the middle of the data
    const uint8_t data[2] = {0xA5, 0x5A};
    synth(data, 2, 19.0, 456, 0.5, false, 0);
    TEST_ASSERT_EQUAL(MfxDemod::TOO_SHORT, demod.decode(syn, synCount * 2 / 3, 240.0f, 2, out));

    // One data bit changed: move the edges of one half clock by half a period
    synth(data, 2, 19.0, 456, 0.5, false, 0);
    const uint32_t from = (uint32_t)((2 * 30 + 1) * 456.0 * 240 + 1000000);
    const uint32_t to = from + (uint32_t)(456.0 * 240);
    for (uint16_t i = 0; i < synCount; i++)
        if (syn[i] >= from && syn[i] < to)
            syn[i] += (uint32_t)(9.5 * 240);
    const MfxDemod::Status s = demod.decode(syn, synCount, 240.0f, 2, out);
    TEST_ASSERT_TRUE(s == MfxDemod::BAD_CHECKSUM || s == MfxDemod::BAD_RUN);

    // Expecting 4 bytes from an answer of 2
    synth(data, 2, 19.0, 456, 0.5, false, 0);
    const MfxDemod::Status s2 = demod.decode(syn, synCount, 240.0f, 4, out);
    TEST_ASSERT_TRUE(s2 != MfxDemod::OK);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_recorded_answers);
    RUN_TEST(test_checksum);
    RUN_TEST(test_synthetic_all_sizes);
    RUN_TEST(test_synthetic_noise_drift_pulses);
    RUN_TEST(test_random_answers);
    RUN_TEST(test_recorded_with_latency);
    RUN_TEST(test_errors_are_detected);
    return UNITY_END();
}
