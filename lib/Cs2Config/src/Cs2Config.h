/*
  Cs2Config.h

  Loco list of the command station in the format of a Marklin Central
  Station 2 (file lokomotive.cs2), and its "config data stream" on the
  CS2 protocol, for Rocrail.

  Exchange (as implemented by can2lan of Gerhard Bertelsmann, railroad
  project, which emulates a CS2 for Rocrail):
    - Rocrail asks:  command 0x20, 8 bytes "loks" (padded with 0).
    - The CS2 answers: the same frame with the response bit, then
      command 0x21: a first frame of 6 bytes (length of the data, 4 bytes,
      and CRC, 2 bytes, big endian), then frames of 8 bytes of data.
    - Data: size of the text (4 bytes, big endian), the text compressed in
      the zlib format, zeros up to a multiple of 8 bytes. The length of the
      first frame does not count the zeros; the CRC (CCITT, start 0xFFFF)
      does.
  The zlib stream here uses "stored" blocks (no compression): any zlib
  reader accepts them, and no compression library is needed.

  Pure C++, no ESP32 dependency: tested on a PC (pio test -e native).
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

class Cs2Config
{
public:
    static const uint8_t FUNCTIONS = 16;

    // One locomotive of the list
    struct Loco
    {
        const char *name;
        uint16_t sid;       // rail address (CS2 loco id 0x4000 + sid)
        uint32_t mfxUid;    // UID of the decoder
        bool haveConfig;    // the values below were read in the decoder
        uint8_t av, bv;     // acceleration, braking (as in the decoder, x 0.25 s)
        uint8_t vmin, vmax;
        uint16_t tachoMax;  // km/h
        uint8_t functions;  // functions known (F0 to F15)
        uint8_t fnType[FUNCTIONS]; // CS2 function type (cs2FunctionType()), 0: none
    };

    // CS2 function type for an mfx function symbol (Krauss 6.4.4:
    // M GGGGGGG S1 S2). fn: number of the function (generic "Fn" symbol).
    // A momentary function gets +128.
    static uint8_t cs2FunctionType(uint8_t fn, bool momentary, uint8_t group, uint8_t s1, uint8_t s2);

    // Text of lokomotive.cs2. Returns its length, 0 if out is too small.
    static size_t locoFile(const Loco *locos, uint8_t count, char *out, size_t size);

    // Data of the config data stream (see above) from text: returns the
    // padded size written in out (a multiple of 8), 0 if out is too small.
    // length: value for the first frame (without the padding).
    // crc: CRC of the padded data.
    static size_t stream(const uint8_t *text, size_t textLen, uint8_t *out, size_t size,
                         uint32_t *length, uint16_t *crc);

    // Room needed by stream() for a text of textLen bytes
    static size_t streamSize(size_t textLen);

    static uint16_t crcCcitt(const uint8_t *data, size_t n, uint16_t seed);
    static uint32_t adler32(const uint8_t *data, size_t n);
};
