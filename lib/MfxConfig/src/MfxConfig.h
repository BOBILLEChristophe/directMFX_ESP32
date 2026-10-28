/*
  MfxConfig.h

  Reads the configuration of an mfx decoder through its CVs (Stefan Krauss,
  "Schienenformat mfx", chapter 6), in the spirit of the fast registration
  of the Marklin Central/Mobile Station 2 (6.3.2).

  The CVs have a 16-bit address V/I: CV number V (10 bits) and index I
  (6 bits). A configuration array (CA) is the set of the 64 CVs of a CV
  number; the CV at index 0 gives its type. The CAs are grouped in blocks;
  the first CA of a block describes it: 01 BB VV 00 GG LL (block type BB,
  GG groups of LL CAs).

  What is read:
    - block 01 (decoder properties) at CV 000, and the block table
      (CA type 17, at CV 004): where the other blocks start;
    - in block 01: manufacturer and product (CA 10), name (CA 18);
    - block 04 (function mapping): the symbol of F0 to F15, first CA
      (type 10) of each group: momentary or not, group, symbol;
    - block 05 (driving): acceleration and braking (CA 13), Vmin and Vmax
      (CA 17), maximum speed shown on the tachometer (CA 18).
  Most CAs are read 4 bytes at a time from index 0: the type and the first
  three values in one answer. About 45 reads, 3 s with the timing of the
  data feedback.

  The reading itself is a function given by the caller (Feedback::readCV()
  on the ESP32, a simulated decoder in the tests).

  Pure C++, no ESP32 dependency: tested on a PC (pio test -e native).

  mfx is a registered trademark of Gebr. Marklin & Cie GmbH.
*/

#pragma once

#include <stdint.h>

class MfxConfig
{
public:
    // Reads `bytes` CVs from cv/index on into out. Returns false if the
    // decoder did not answer.
    typedef bool (*ReadFn)(void *ctx, uint16_t cv, uint8_t index, uint8_t bytes, uint8_t *out);

    static const uint8_t MAX_BLOCKS = 16;
    static const uint8_t FUNCTIONS = 16;

    struct Block
    {
        uint8_t type;     // 01 decoder, 02 functions, 04 mapping, 05 driving...
        uint16_t cv;      // CV number of its first CA
        uint8_t groups;   // GG
        uint8_t perGroup; // LL
    };

    struct Function
    {
        bool present;   // a symbol is defined
        bool momentary; // on while the key is pressed
        uint8_t group;  // 02 F, 03 light, 07 sound... (see groupName())
        uint8_t s1, s2; // symbol
    };

    enum Status : uint8_t
    {
        OK = 0,
        NO_ANSWER,      // the decoder does not answer at all
        NO_BLOCK_TABLE, // CV 004 is not a block table
        INCOMPLETE      // some reads failed, part of the data is missing
    };

    MfxConfig() { clear(); }
    void clear();

    Status read(ReadFn fn, void *ctx);

    // Blocks found (block 01 first). nullptr if absent.
    uint8_t blockCount;
    Block blocks[MAX_BLOCKS];
    const Block *block(uint8_t type) const;

    char name[17];          // UTF-8, null-terminated
    bool haveManufacturer;
    uint32_t manufacturer;  // 00 00 00 83 = Trix/Marklin, 00 00 00 97 = ESU
    uint32_t product;       // Marklin: spare part number of the decoder

    uint8_t functionCount;  // functions read (up to 16)
    Function functions[FUNCTIONS];

    bool haveDriving;
    uint8_t acceleration;   // x 0.25 s, 0 to Vmax
    uint8_t braking;        // x 0.25 s
    uint8_t vmin, vmax;     // 0 to 255
    uint16_t tachoKmh;      // maximum speed shown, km/h

    uint16_t reads;         // read commands sent
    uint16_t failures;      // reads without a good answer

    // Diagnostic: the first failed reads (CV number, index)
    static const uint8_t MAX_FAILED = 8;
    uint16_t failedCv[MAX_FAILED];
    uint8_t failedIndex[MAX_FAILED];
    // Diagnostic: CA types found in block 01 (CV 000 to 01F), 0xFF if not read
    uint8_t block01Types[32];
    uint8_t block01Size;    // CAs of block 01 (GG x LL)

    // Name of a function group (Krauss 6.4.4), in French
    static const char *groupName(uint8_t group);

private:
    bool get(uint16_t cv, uint8_t index, uint8_t bytes, uint8_t *out);
    void readBlock01(const Block &b);
    void readFunctions(const Block &b);
    void readDriving(const Block &b);
    void readName(uint16_t cv);

    ReadFn mFn;
    void *mCtx;
};
