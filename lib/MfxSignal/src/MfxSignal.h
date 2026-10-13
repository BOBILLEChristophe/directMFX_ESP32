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

  Pure C++, no ESP32 dependency: tested on a PC (pio test -e native).
  MfxRMT copies the items into rmt_item32_t (50 µs per half).

  mfx is a registered trademark of Gebr. Marklin & Cie GmbH.
*/

#pragma once

#include <stdint.h>

struct MfxItem
{
    bool level0; // first 50 µs
    bool level1; // second 50 µs
};

class MfxSignal
{
public:
    // Room for: sync + 128-bit frame + stuffing (1 per 8 bits) + sync
    static const uint16_t MAX_ITEMS = 5 + 128 + 16 + 5;

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

    uint16_t count() const { return mCount; }
    const MfxItem &item(uint16_t i) const { return mItems[i]; }
    bool overflow() const { return mOverflow; }

private:
    void push(bool level0, bool level1);

    MfxItem mItems[MAX_ITEMS];
    uint16_t mCount;
    bool mLevel;
    bool mOverflow;
};
