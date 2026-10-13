/*
  MfxSignal.cpp
*/

#include "MfxSignal.h"

void MfxSignal::push(bool level0, bool level1)
{
    if (mCount >= MAX_ITEMS)
    {
        mOverflow = true;
        return;
    }
    mItems[mCount].level0 = level0;
    mItems[mCount].level1 = level1;
    mCount++;
}

void MfxSignal::bit0()
{
    mLevel = !mLevel; // transition at the start
    push(mLevel, mLevel);
}

void MfxSignal::bit1()
{
    const bool first = !mLevel; // transition at the start
    mLevel = !first;            // and in the middle
    push(first, mLevel);
}

/*
 Same pattern as setMFXsync() of version 0.10.0 (from the original DirectMFX).
 Starting from level L:
   item 1 : !L  !L   (transition, 100 µs)
   item 2 :  L  !L   (transition, then transition in the middle)
   item 3 : !L   L   (no transition at the start)
   item 4 :  L  !L   (no transition at the start)
   item 5 :  L   L   (transition, 100 µs)
 It ends at L.
*/
void MfxSignal::sync()
{
    const bool l = mLevel;
    push(!l, !l);
    push(l, !l);
    push(!l, l);
    push(l, !l);
    push(l, l);
    mLevel = l;
}

void MfxSignal::frame(const uint8_t *buff)
{
    const uint8_t nbits = buff[0];
    uint8_t ones = 0;
    for (uint8_t i = 1; i <= nbits; i++)
    {
        if (buff[i])
        {
            bit1();
            if (++ones == 8)
            {
                // Bit stuffing: a 0 after 8 consecutive 1 bits.
                // It does not consume a bit of the frame.
                bit0();
                ones = 0;
            }
        }
        else
        {
            bit0();
            ones = 0;
        }
    }
}
