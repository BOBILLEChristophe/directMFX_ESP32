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

   The configuration of the decoder is then read through its CVs (MfxConfig:
   name, manufacturer, symbols of the functions, driving parameters) and
   printed, with the line for main.cpp. When the track is first powered,
   the configuration of each locomotive declared in main.cpp is read as
   well (3 s after the track is on, then twice more, 20 s apart, for the
   decoders that did not answer): a test of the reading of CVs.

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

    // Reads the configuration of the decoder at rail address sid and prints
    // it. Returns false if the decoder does not answer. name (optional)
    // receives the name read, null-terminated.
    static bool readConfig(uint16_t sid, char *name = nullptr, size_t size = 0);

private:
    static void task(void *);
    static bool probe(uint8_t count, uint32_t uid);
    static void registerDecoder(uint32_t uid);
    static bool readDeclaredConfigs();
};
