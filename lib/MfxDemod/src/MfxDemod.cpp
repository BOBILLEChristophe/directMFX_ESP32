/*
  MfxDemod.cpp
*/

#include "MfxDemod.h"

static const float HALF_CLOCK_US = 456.0f;
static const uint8_t MIN_RUN_EDGES = 5;    // shorter runs are ignored (~95 µs)
static const uint8_t MIN_LEADING_ONES = 8; // before the preamble

// x - round(x): in [-0.5, 0.5)
static float wrap(float x)
{
    x -= (float)(int32_t)x;
    if (x >= 0.5f)
        x -= 1.0f;
    else if (x < -0.5f)
        x += 1.0f;
    return x;
}

// Distance between two rising edges of the carrier: one period, or half
// a period more or less at a phase change
static bool regular(uint32_t dticks, float ticksPerUs)
{
    const float dt = (float)dticks / ticksPerUs;
    return (dt > 17.0f && dt < 21.0f) || (dt > 8.0f && dt < 11.0f) || (dt > 27.0f && dt < 30.0f);
}

uint8_t MfxDemod::checksum(const uint8_t *data, uint8_t n)
{
    uint16_t c = 0x00FF;
    for (uint8_t i = 0; i < n; i++)
    {
        c = c ^ (c << 1) ^ (c << 2);
        c = c ^ data[i];
        if (c & 0x0100)
            c = c ^ 0x0107;
        if (c & 0x0200)
            c = c ^ 0x020E;
    }
    return (uint8_t)c;
}

const char *MfxDemod::statusText(Status s)
{
    switch (s)
    {
    case OK:
        return "ok";
    case NO_CARRIER:
        return "no carrier";
    case BAD_RUN:
        return "bad phase run";
    case NO_PREAMBLE:
        return "no preamble";
    case TOO_SHORT:
        return "answer too short";
    case BAD_CHECKSUM:
        return "bad checksum";
    }
    return "?";
}

/*
 Phase of each edge, in carrier periods, compared with a reference phase
 that follows the slow drift; runs of edges of the same phase, in mRunUs.
*/
void MfxDemod::runsPass(const uint32_t *ticks, uint16_t n, float ticksPerUs, float period)
{
    mRuns = 0;
    float ref = 0;          // reference phase, in periods
    bool haveRef = false;
    bool runPhase = false;  // phase of the current run (false: A)
    uint16_t runEdges = 0;
    float runFirst = 0, runLast = 0;  // µs, first and last edge of the run
    float boundary = 0;     // start of the current run (µs)
    bool haveRun = false;
    // Runs of fewer than MIN_RUN_EDGES edges are ignored
    float prevLast = 0;     // last edge of the previous accepted run
    bool prevPhase = false;
    bool havePrev = false;

    for (uint16_t i = 0; i < n; i++)
    {
        const float t = (float)(uint32_t)(ticks[i] - ticks[0]) / ticksPerUs;
        const float phase = t / period;
        if (!haveRef)
        {
            ref = wrap(phase);
            haveRef = true;
        }
        float d = wrap(phase - ref);
        bool b = false;
        if (d > 0.25f || d < -0.25f)
        {
            b = true;
            d = wrap(d - 0.5f);
        }
        // Follow the drift, only on regular edges
        if (i > 0)
        {
            if (regular(ticks[i] - ticks[i - 1], ticksPerUs))
                ref = wrap(ref + 0.05f * d);
        }

        if (haveRun && b == runPhase)
        {
            runLast = t;
            runEdges++;
            continue;
        }
        // The phase changes: close the current run
        if (haveRun)
        {
            if (runEdges >= MIN_RUN_EDGES)
            {
                if (havePrev && prevPhase == runPhase)
                {
                    // Same phase as the previous accepted run (a short run
                    // between them was ignored): it continues
                }
                else
                {
                    if (havePrev)
                    {
                        const float change = (prevLast + runFirst) / 2;
                        if (mRuns < MAX_RUNS)
                            mRunUs[mRuns++] = change - boundary;
                        boundary = change;
                    }
                    else
                        boundary = runFirst;
                    prevPhase = runPhase;
                    havePrev = true;
                }
                prevLast = runLast;
            }
        }
        haveRun = true;
        runPhase = b;
        runEdges = 1;
        runFirst = runLast = t;
    }
    // Last run
    if (haveRun && runEdges >= MIN_RUN_EDGES)
    {
        if (havePrev && prevPhase != runPhase)
        {
            const float change = (prevLast + runFirst) / 2;
            if (mRuns < MAX_RUNS)
                mRunUs[mRuns++] = change - boundary;
            boundary = change;
        }
        prevLast = runLast;
    }
    if (havePrev && mRuns < MAX_RUNS)
        mRunUs[mRuns++] = prevLast - boundary; // the end of the answer
}

MfxDemod::Status MfxDemod::decode(const uint32_t *ticks, uint16_t n, float ticksPerUs, uint8_t bytes, uint8_t *out)
{
    mRuns = 0;
    mBits = 0;
    mBitText[0] = 0;
    mPeriodUs = 0;
    if (bytes > MAX_BYTES)
        bytes = MAX_BYTES;
    if (n < 100)
        return NO_CARRIER;

    // 1. Carrier period: mean of the regular intervals (17 to 21 µs). A
    //    phase change gives about 9.5 or 28.5 µs, the pulses other values.
    float sum = 0;
    uint16_t count = 0;
    for (uint16_t i = 1; i < n; i++)
    {
        const float d = (float)(uint32_t)(ticks[i] - ticks[i - 1]) / ticksPerUs;
        if (d > 17.0f && d < 21.0f)
        {
            sum += d;
            count++;
        }
    }
    if (count < 50)
        return NO_CARRIER;
    const float period = sum / count;
    mPeriodUs = period;

    // Refined with the doubled phase of the edges, which does not change at
    // a 180° phase change: unwrapped from edge to edge, its slope gives the
    // error on the frequency (linear regression).
    float p = period;
    for (uint8_t pass = 0; pass < 2; pass++)
    {
        // Only the edges at a regular distance from the previous one: the
        // false edges at the clock pulses would make the unwrapping slip.
        double st = 0, su = 0, stt = 0, stu = 0;
        float u = 0, prev = 0;
        bool first = true;
        uint16_t used = 0;
        for (uint16_t i = 1; i < n; i++)
        {
            if (!regular(ticks[i] - ticks[i - 1], ticksPerUs))
                continue;
            const float t = (float)(uint32_t)(ticks[i] - ticks[0]) / ticksPerUs;
            const float ph = wrap(2.0f * t / p);
            if (!first)
                u += wrap(ph - prev);
            first = false;
            prev = ph;
            const double x = t;
            st += x;
            su += u;
            stt += x * x;
            stu += x * u;
            used++;
        }
        if (used < 50)
            break;
        const double den = used * stt - st * st;
        if (den <= 0)
            break;
        const double slope = (used * stu - st * su) / den; // doubled periods per µs
        const float corrected = (float)(1.0 / (1.0 / p - slope / 2.0));
        if (!(corrected > 17.0f && corrected < 21.0f))
            break;
        p = corrected;
    }
    mPeriodUs = p;

    // 2. and 3. Runs of edges of the same phase
    runsPass(ticks, n, ticksPerUs, p);

    // 4. Bits. The first run is skipped: the carrier may start in the
    //    middle of a clock (it is a 1 bit of the start anyway). The last run
    //    may be cut by the end of the pause: it is kept only if complete.
    for (uint16_t i = 1; i < mRuns && mBits < MAX_BITS;)
    {
        const float halves = mRunUs[i] / HALF_CLOCK_US;
        const bool last = (i == mRuns - 1);
        if (halves > 1.5f && halves < 2.5f)
        {
            mBitText[mBits++] = '1';
            i++;
        }
        else if (halves > 0.5f && halves <= 1.5f && i + 1 < mRuns)
        {
            const float next = mRunUs[i + 1] / HALF_CLOCK_US;
            if (next > 0.5f && next <= 1.5f)
            {
                mBitText[mBits++] = '0';
                i += 2;
            }
            else
            {
                mBitText[mBits++] = '?';
                i++;
            }
        }
        else
        {
            if (!last)
                mBitText[mBits++] = '?';
            i++;
        }
    }
    mBitText[mBits] = 0;

    // 5. Preamble after at least 8 ones, then the data and the checksum
    uint16_t ones = 0, start = 0;
    bool found = false;
    for (uint16_t i = 0; i + 2 < mBits; i++)
    {
        if (mBitText[i] == '1')
        {
            ones++;
            continue;
        }
        if (ones >= MIN_LEADING_ONES && mBitText[i] == '0' && mBitText[i + 1] == '1' && mBitText[i + 2] == '0')
        {
            start = i + 3;
            found = true;
            break;
        }
        ones = 0;
    }
    if (!found)
    {
        for (uint16_t i = 0; i < mBits; i++)
            if (mBitText[i] == '?')
                return BAD_RUN;
        return NO_PREAMBLE;
    }
    const uint16_t need = 8 * (bytes + 1);
    if (start + need > mBits)
        return TOO_SHORT;
    uint8_t data[MAX_BYTES + 1];
    for (uint8_t k = 0; k <= bytes; k++)
    {
        uint8_t v = 0;
        for (uint8_t j = 0; j < 8; j++)
        {
            const char c = mBitText[start + 8 * k + j];
            if (c == '?')
                return BAD_RUN;
            v = (uint8_t)((v << 1) | (c == '1'));
        }
        data[k] = v;
    }
    if (checksum(data, bytes) != data[bytes])
        return BAD_CHECKSUM;
    for (uint8_t k = 0; k < bytes; k++)
        out[k] = data[k];
    return OK;
}
