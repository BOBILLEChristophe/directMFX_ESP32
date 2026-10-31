/*
  MfxConfig.cpp
*/

#include "MfxConfig.h"
#include <string.h>

static const uint8_t CA_BLOCK = 0x01;        // block description
static const uint8_t CA_MANUFACTURER = 0x10; // block 01
static const uint8_t CA_BLOCK_TABLE = 0x17;  // block 01
static const uint8_t CA_NAME = 0x18;         // block 01
static const uint8_t CA_SYMBOL = 0x10;       // block 04
static const uint8_t CA_ACCELERATION = 0x13; // block 05
static const uint8_t CA_SPEED_CURVE = 0x17;  // block 05
static const uint8_t CA_TACHO = 0x18;        // block 05
static const uint16_t BLOCK_TABLE_CV = 0x004;
static const uint8_t MAX_SCAN = 32; // CAs scanned in a block

void MfxConfig::clear()
{
    blockCount = 0;
    memset(blocks, 0, sizeof(blocks));
    name[0] = 0;
    haveManufacturer = false;
    manufacturer = product = 0;
    functionCount = 0;
    memset(functions, 0, sizeof(functions));
    haveDriving = false;
    acceleration = braking = vmin = vmax = 0;
    tachoKmh = 0;
    reads = failures = 0;
    memset(failedCv, 0, sizeof(failedCv));
    memset(failedIndex, 0, sizeof(failedIndex));
    memset(block01Types, 0xFF, sizeof(block01Types));
    block01Size = 0;
    mFn = nullptr;
    mCtx = nullptr;
}

const MfxConfig::Block *MfxConfig::block(uint8_t type) const
{
    for (uint8_t i = 0; i < blockCount; i++)
        if (blocks[i].type == type)
            return &blocks[i];
    return nullptr;
}

const char *MfxConfig::groupName(uint8_t group)
{
    switch (group)
    {
    case 0x00:
        return "symbole";
    case 0x02:
        return "fonction";
    case 0x03:
        return "lumiere";
    case 0x04:
        return "eclairage interieur";
    case 0x05:
        return "eclairage exterieur";
    case 0x06:
        return "mecanique";
    case 0x07:
        return "son";
    case 0x08:
        return "son de fond";
    case 0x09:
        return "annonce";
    case 0x0A:
        return "manoeuvre";
    case 0x0B:
        return "commande directe";
    default:
        return "?";
    }
}

bool MfxConfig::get(uint16_t cv, uint8_t index, uint8_t bytes, uint8_t *out)
{
    reads++;
    if (mFn(mCtx, cv, index, bytes, out))
        return true;
    if (failures < MAX_FAILED)
    {
        failedCv[failures] = cv;
        failedIndex[failures] = index;
    }
    failures++;
    return false;
}

MfxConfig::Status MfxConfig::read(ReadFn fn, void *ctx)
{
    clear();
    mFn = fn;
    mCtx = ctx;
    uint8_t b[4];

    // Block 01 always starts at CV 000: 01 01 VV 00 GG LL. Only the first
    // 2 bytes are read, as the MS2 does: an old ESU decoder (M4 of the
    // 040 DE 9) gave a too short answer to a 4-byte read of this CV.
    if (!get(0x000, 0, 2, b))
        return NO_ANSWER;
    if (b[0] != CA_BLOCK || b[1] != 0x01)
        return NO_BLOCK_TABLE;
    uint8_t gl[2] = {0, 0};
    get(0x000, 4, 2, gl);
    blocks[blockCount++] = {0x01, 0x000, gl[0], gl[1]};

    // Block table at CV 004: 17 FF FF ... 0 (FF x 4 = CV of the block)
    if (!get(BLOCK_TABLE_CV, 0, 4, b))
        return INCOMPLETE;
    if (b[0] != CA_BLOCK_TABLE)
        return NO_BLOCK_TABLE;
    uint8_t starts[MAX_BLOCKS];
    uint8_t n = 0;
    bool end = false;
    for (uint8_t index = 0; !end && n < MAX_BLOCKS - 1; index += 4)
    {
        if (index > 0 && !get(BLOCK_TABLE_CV, index, 4, b))
            break;
        for (uint8_t i = (index == 0 ? 1 : 0); i < 4; i++)
        {
            if (b[i] == 0 || n >= MAX_BLOCKS - 1)
            {
                end = true;
                break;
            }
            starts[n++] = b[i];
        }
    }

    // Description of each block
    for (uint8_t i = 0; i < n; i++)
    {
        const uint16_t cv = (uint16_t)starts[i] * 4;
        if (!get(cv, 0, 4, b) || b[0] != CA_BLOCK)
            continue;
        Block blk = {b[1], cv, 0, 0};
        gl[0] = gl[1] = 0;
        if (get(cv, 4, 2, gl))
        {
            blk.groups = gl[0];
            blk.perGroup = gl[1];
        }
        blocks[blockCount++] = blk;
    }

    readBlock01(blocks[0]);
    const Block *f = block(0x04);
    if (f)
        readFunctions(*f);
    const Block *d = block(0x05);
    if (d)
        readDriving(*d);

    return failures ? INCOMPLETE : OK;
}

// Name: CA type 18, up to 16 bytes from index 1, null-terminated
void MfxConfig::readName(uint16_t cv)
{
    uint8_t len = 0;
    for (uint8_t index = 1; index < 17; index += 4)
    {
        uint8_t b[4];
        if (!get(cv, index, 4, b))
            break;
        for (uint8_t i = 0; i < 4; i++)
        {
            if (b[i] == 0)
            {
                name[len] = 0;
                return;
            }
            name[len++] = (char)b[i];
        }
    }
    name[len] = 0;
}

void MfxConfig::readBlock01(const Block &blk)
{
    uint16_t cas = (uint16_t)blk.groups * blk.perGroup;
    if (cas == 0 || cas > MAX_SCAN)
        cas = 12; // CV 000 to 00B, as in the decoders observed
    block01Size = (uint8_t)cas;
    block01Types[0] = CA_BLOCK;
    block01Types[BLOCK_TABLE_CV] = CA_BLOCK_TABLE;
    for (uint16_t k = 1; k < cas; k++)
    {
        const uint16_t cv = blk.cv + k;
        uint8_t b[4];
        if (cv == BLOCK_TABLE_CV)
            continue; // already read
        if (!get(cv, 0, 4, b))
            continue;
        if (k < sizeof(block01Types))
            block01Types[k] = b[0];
        if (b[0] == CA_MANUFACTURER && !haveManufacturer)
        {
            // 10 HH HH HH HH PP PP PP PP
            uint8_t h[4], p[4];
            if (get(cv, 1, 4, h) && get(cv, 5, 4, p))
            {
                manufacturer = (uint32_t)h[0] << 24 | (uint32_t)h[1] << 16 | (uint32_t)h[2] << 8 | h[3];
                product = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
                haveManufacturer = true;
            }
        }
        else if (b[0] == CA_NAME && name[0] == 0)
            readName(cv);
    }
}

/*
 F0 to F15. Each function has 3 CAs in a row (types 10, 12, 13, Krauss
 6.4.4), the first one being its symbol: 10 M GGGGGGG S1 S2. The block may
 describe them as 18 groups of 3 CAs (Krauss) or as 1 group of 70 CAs
 (observed on a Marklin decoder): only the total GG x LL counts.
*/
void MfxConfig::readFunctions(const Block &blk)
{
    uint16_t cas = (uint16_t)blk.groups * blk.perGroup;
    if (cas == 0)
        cas = 3 * FUNCTIONS;
    uint16_t count = cas / 3;
    if (count > FUNCTIONS)
        count = FUNCTIONS;
    for (uint8_t fn = 0; fn < count; fn++)
    {
        const uint16_t cv = blk.cv + 1 + (uint16_t)fn * 3;
        uint8_t b[4];
        Function &f = functions[fn];
        if (get(cv, 0, 4, b) && b[0] == CA_SYMBOL)
        {
            f.momentary = (b[1] & 0x80) != 0;
            f.group = b[1] & 0x7F;
            f.s1 = b[2];
            f.s2 = b[3];
            f.present = f.group != 0 || f.s1 != 0 || f.s2 != 0;
        }
        functionCount = fn + 1;
    }
}

void MfxConfig::readDriving(const Block &blk)
{
    uint16_t cas = (uint16_t)blk.groups * blk.perGroup;
    if (cas == 0 || cas > MAX_SCAN)
        cas = 10;
    for (uint16_t k = 1; k < cas; k++)
    {
        const uint16_t cv = blk.cv + k;
        uint8_t b[4];
        if (!get(cv, 0, 4, b))
            continue;
        if (b[0] == CA_ACCELERATION)
        {
            acceleration = b[1];
            braking = b[2];
            haveDriving = true;
        }
        else if (b[0] == CA_SPEED_CURVE)
        {
            vmin = b[1];
            vmax = b[2];
            haveDriving = true;
        }
        else if (b[0] == CA_TACHO)
        {
            tachoKmh = (uint16_t)(b[1] | b[2] << 8); // little endian (Krauss 6.4.5)
            haveDriving = true;
        }
    }
}
