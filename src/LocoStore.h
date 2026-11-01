/*

   LocoStore.h

   The locomotives registered by the command station, kept in the NVS of the
   ESP32 (namespace "locos"): address, decoder UID and name.

   An mfx decoder registered by a command station remembers its UID and
   re-registration counter; while it hears them again it keeps its address
   and no longer answers the search. The command station must therefore
   remember every decoder it registered, as an MS2 does. Before, the list
   was only the lines of main.cpp: a locomotive removed from main.cpp was
   forgotten by the command station but not by its decoder, which stayed
   silent until the counter was changed (October 2026).

   At start-up, load() adds the stored locomotives after those declared in
   main.cpp. A declaration in main.cpp wins: same UID -> the stored entry is
   ignored; same address with another UID -> the stored locomotive gets the
   first free address (its decoder receives it with the next bind).

   save() writes every known locomotive. It is called when a new decoder is
   registered (Discovery).

   Serial commands (Discovery): L lists the locomotives, E erases the
   memory and raises the counter (every decoder registers again).

*/

#pragma once

#include <Arduino.h>

namespace LocoStore
{
    // Adds the stored locomotives to the refresh (Message::addLoco). To call
    // after Message::setup().
    void load();

    // Writes every known locomotive to the NVS.
    void save();

    // Prints the known locomotives (address, UID, name, stored or not).
    void print();

    // Erases the stored locomotives. The command station must then raise
    // its counter, so that the decoders register again.
    void erase();
}
