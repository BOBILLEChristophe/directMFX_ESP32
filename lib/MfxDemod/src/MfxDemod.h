/*
  MfxDemod.h

  Decodes the data feedback of an mfx decoder (Stefan Krauss,
  "Schienenformat mfx", 2.3.3 and 2.3.4), from the times of the rising
  edges given by the feedback sensor during the pause of the request.

  The decoder draws a current at about 52.6 kHz (period 19 µs) and codes
  its data by 180° phase changes of this carrier (bi-phase, as RDS):
    - a phase change at every clock, every 912 µs;
    - bit 0: one more phase change in the middle of the clock (456 µs);
    - bit 1: no phase change in the middle.
  The answer is: 1 bits (23 clocks), the preamble 010, the data bytes
  (most significant bit first), a checksum byte, then 1 bits.

  How it works:
    1. The carrier period is measured on the regular edges, then refined
       with the doubled phase (which ignores the 180° changes).
    2. Each edge is compared with a reference phase, which follows the slow
       drift: same phase (A) or opposite phase (B).
    3. The runs of edges of the same phase give the time between two phase
       changes: about 456 µs (half a clock) or 912 µs (a whole clock).
       Short runs (a few edges, at the clock pulses of the command station)
       are ignored.
    4. A run of one clock is a 1 bit, two runs of half a clock are a 0 bit.
    5. After at least 8 bits at 1, the preamble 010 gives the start of the
       data; the checksum is then checked.

  Pure C++, no ESP32 dependency: tested on a PC (pio test -e native) with
  answers recorded on the track of a Marklin Gleisbox.

  mfx is a registered trademark of Gebr. Marklin & Cie GmbH.
*/

#pragma once

#include <stdint.h>

class MfxDemod
{
public:
    static const uint8_t MAX_BYTES = 8;
    static const uint16_t MAX_RUNS = 400;  // phase runs in one answer
    static const uint16_t MAX_BITS = 140;  // 23 + 3 + 64 + 8 + spare

    enum Status : uint8_t
    {
        OK = 0,
        NO_CARRIER,   // too few edges
        BAD_RUN,      // a run of phase that is neither 1/2 nor 1 clock
        NO_PREAMBLE,  // no 1 bits followed by 010
        TOO_SHORT,    // the answer ends before the checksum
        BAD_CHECKSUM
    };

    MfxDemod() : mRuns(0), mBits(0), mPeriodUs(0) {}

    // ticks: times of the rising edges, ascending (any origin, the counter
    // may wrap around: only differences are used). ticksPerUs: ticks per µs
    // (240 for the cycle counter of an ESP32 at 240 MHz).
    // bytes: number of data bytes expected (1, 2, 4 or 8).
    // On success, out[0..bytes-1] receives the data.
    Status decode(const uint32_t *ticks, uint16_t n, float ticksPerUs, uint8_t bytes, uint8_t *out);

    // Checksum of the data feedback (Krauss 2.3.4)
    static uint8_t checksum(const uint8_t *data, uint8_t n);

    static const char *statusText(Status s);

    // Diagnostic of the last decode()
    float periodUs() const { return mPeriodUs; }   // carrier period
    uint16_t runs() const { return mRuns; }        // phase runs found
    uint16_t bitCount() const { return mBits; }
    // Bits found, as text ('0', '1', '?'), null-terminated
    const char *bits() const { return mBitText; }
    // Length of the phase run i, in µs
    float runUs(uint16_t i) const { return i < mRuns ? mRunUs[i] : 0; }

private:
    void runsPass(const uint32_t *ticks, uint16_t n, float ticksPerUs, float period);

    float mRunUs[MAX_RUNS]; // length of each phase run
    uint16_t mRuns;
    char mBitText[MAX_BITS + 1];
    uint16_t mBits;
    float mPeriodUs;
};
