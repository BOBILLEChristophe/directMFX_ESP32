/*

   Discovery.h

   Automatic discovery of the mfx decoders (Krauss 3.2.3, decoder search).

   Every 2 s, while the track is powered, the command station asks
   "is there a decoder not yet registered?" (search with count 0). If one
   answers, its 32-bit UID is read bit by bit: for each bit, the search asks
   whether a decoder has the bits found so far followed by a 0. The UID found
   is checked with a last search on the 32 bits.

   A new decoder gets the first free address, is added to the refresh and
   receives its address (Bind). The serial port prints its UID and the line
   to add to main.cpp to keep it after a restart.

   The name of the locomotive is then read in the decoder (CV 003, Krauss
   6.3.2), and printed with the line for main.cpp. When the track is first
   powered, the names of the locomotives declared in main.cpp are read as
   well: a test of the reading of CVs.

   Needs the feedback sensor (Feedback.h).

*/

#pragma once

#include <Arduino.h>

class Discovery
{
public:
    Discovery() = delete;
    static void setup();

    // Reads the UID of a decoder not yet registered. Returns false if no
    // decoder answers, or if the UID found does not answer the check.
    static bool findUid(uint32_t *uid);

    // Reads the name of the locomotive in the decoder at rail address sid:
    // CV 003, index 01 on, 4 bytes at a time, up to 16 characters.
    // name receives a null-terminated text. Returns false if the decoder
    // does not answer.
    static bool readName(uint16_t sid, char *name, size_t size);

private:
    static void task(void *);
    static bool probe(uint8_t count, uint32_t uid);
    static void registerDecoder(uint32_t uid);
    static void readDeclaredNames();
};
