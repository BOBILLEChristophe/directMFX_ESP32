/*
  MfxSignal.h

  Turns mfx frames into the track signal, as a list of items of two
  50 µs halves (Stefan Krauss, "Schienenformat mfx", §2.2.1):
    - bit 0 : a transition at the start, then 100 µs without transition;
    - bit 1 : a transition at the start and another one in the middle;
    - synchronisation : 5 items (500 µs) with deliberate rule violations;
    - bit stuffing : a 0 is inserted after 8 consecutive 1 bits.

  The level of the track is kept from one call to the next, so that
  consecutive chunks join without a missing or spurious transition.

  Feedback (decoder -> command station): after a frame that asks for an
  answer, feedbackWindows() adds the request sequence and two windows in
  which the track keeps a constant level. The decoder answers in the
  first window by drawing a current at about 52.6 kHz; the second window
  is a reference (no answer expected) to measure the noise.

  Data feedback (reading CVs): after the read command, dataWindow() adds
  the request sequence and a long pause in which the command station gives
  the clock to the decoder with short pulses (Krauss 2.2.8): 23 pulses
  every 912 µs (the decoder sends 1 bits), then a pulse every 456 µs for
  the preamble, the data, the checksum and 4 spare clocks. The decoder
  answers during the whole pause, by phase modulation of its 52.6 kHz
  current (see MfxDemod).

  Pure C++, no ESP32 dependency: tested on a PC (pio test -e native).
  MfxRMT copies the items into rmt_item32_t (1 µs per tick).

  mfx is a registered trademark of Gebr. Marklin & Cie GmbH.
*/

#pragma once

#include <stdint.h>

struct MfxItem
{
    bool level0;        // first half
    bool level1;        // second half
    uint16_t duration0; // µs, 50 for a bit or a synchronisation
    uint16_t duration1; // µs
};

class MfxSignal
{
public:
    static const uint16_t HALF_US = 50; // half of a bit

    // 1-bit feedback (Krauss; sequence as measured by Alyda on real decoders)
    static const uint8_t FEEDBACK_SYNCS = 11;     // synchronisations after the frame
    static const uint16_t FEEDBACK_WINDOW_US = 6400; // window of the answer
    static const uint8_t FEEDBACK_END_SYNCS = 2;  // synchronisations at the end

    // Data feedback (Krauss 2.2.8, timing measured on a Marklin Gleisbox)
    static const uint16_t DATA_CLOCK_US = 912;   // one clock = one bit
    static const uint16_t DATA_PULSE_US = 25;    // clock pulse, opposite level
    static const uint8_t DATA_LEAD_CLOCKS = 23;  // clocks without data (1 bits)
    static const uint8_t DATA_MAX_BYTES = 8;
    // Clocks with a pulse in the middle: preamble 010, data, checksum, 4 spare
    static uint16_t dataClocks(uint8_t bytes) { return 3 + 8 * bytes + 8 + 4; }
    // Length of the pause (the decoder answers in it), in µs
    static uint32_t dataPauseUs(uint8_t bytes)
    {
        return (uint32_t)(DATA_LEAD_CLOCKS + dataClocks(bytes)) * DATA_CLOCK_US;
    }

    // Room for the longest chunk: sync + 128-bit frame + stuffing
    //   + data feedback of 8 bytes: 11 syncs, 4 bits, pause (one item per
    //   pulse, plus one), sync. The 1-bit feedback is shorter.
    static const uint16_t MAX_ITEMS = 5 + 128 + 16 + 5 * FEEDBACK_SYNCS + 4 +
                                      DATA_LEAD_CLOCKS + 2 * (3 + 8 * DATA_MAX_BYTES + 8 + 4) + 1 + 5;

    MfxSignal() : mCount(0), mLevel(false), mOverflow(false) {}

    // Empties the item list. The track level is kept.
    void clear()
    {
        mCount = 0;
        mOverflow = false;
    }

    // Level of the track at the end of the last item.
    bool level() const { return mLevel; }
    void setLevel(bool level) { mLevel = level; }

    // Synchronisation pattern, 500 µs. It ends at the level it started from.
    void sync();

    // Frame in the format of MfxFrame::toBuffer():
    // buff[0] = number of bits, buff[1..n] = one bit per byte.
    // Exactly buff[0] bits are sent, plus the stuffing bits.
    void frame(const uint8_t *buff);

    void bit0();
    void bit1();

    // Keeps the current level for us µs, without any transition (one item,
    // two halves of us/2). us <= 65534.
    void hold(uint32_t us);

    // Request for a 1-bit feedback, to add after the frame:
    //   11 synchronisations, bits 0011,
    //   window of 6.4 ms (the decoder answers here),
    //   synchronisation, reference window of 6.4 ms (no answer),
    //   2 synchronisations.
    // windowUs and referenceUs receive the start time of the two windows,
    // in µs from the start of the chunk.
    void feedbackWindows(uint32_t *windowUs, uint32_t *referenceUs);

    // Request for a data feedback of `bytes` bytes (1, 2, 4 or 8), to add
    // after a read CV frame:
    //   11 synchronisations, bits 0011,
    //   pause at constant level with a short pulse of the opposite level at
    //   the end of each period: 23 periods of 912 µs, then 2 x dataClocks()
    //   periods of 456 µs, then 50 µs,
    //   synchronisation.
    // pauseUs receives the start time of the pause, in µs from the start of
    // the chunk. It lasts dataPauseUs(bytes).
    void dataWindow(uint8_t bytes, uint32_t *pauseUs);

    // Duration of the items, in µs
    uint32_t durationUs() const;

    uint16_t count() const { return mCount; }
    const MfxItem &item(uint16_t i) const { return mItems[i]; }
    bool overflow() const { return mOverflow; }

private:
    void push(bool level0, bool level1, uint16_t duration0 = HALF_US, uint16_t duration1 = HALF_US);

    MfxItem mItems[MAX_ITEMS];
    uint16_t mCount;
    bool mLevel;
    bool mOverflow;
};
