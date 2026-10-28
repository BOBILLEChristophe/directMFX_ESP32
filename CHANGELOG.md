# Changelog

## Unreleased

### Added (feedback, under test)
- **1-bit feedback** (`src/Feedback.cpp`): `Feedback::ask()` sends a frame followed by the feedback request (11 synchronisations, bits 0011) and two windows of 6.4 ms at constant level. The PCNT counts the rising edges of the sensor comparator in the answer window and in a reference window, from 200 µs after the start of each window (the tuned circuit still rings 60 to 80 µs after the last polarity change). A decoder answered when the answer window holds at least 100 edges and 4 times more than the reference window. Sensor: see the README; comparator output on GPIO 26.
- **Automatic discovery** (`src/Discovery.cpp`): every 2 s while the track is powered, a decoder search with count 0; if a decoder answers, its UID is read bit by bit (Krauss 3.2.3) and checked. A new decoder gets the first free address, is added to the refresh and receives a Bind; the serial port prints its UID and the line to add to `main.cpp`.
- `MfxFrame::search()` (command 111 010), `MfxSignal::hold()` and `MfxSignal::feedbackWindows()`, with unit tests. Items now carry their duration.
- **Reading of CVs** (data feedback, Krauss 2.2.8, 2.3.3, 3.2.1): `Feedback::readCV(sid, cv, index, bytes, out)` sends the read command (`MfxFrame::readCV()`, 111 000 + CV + index + count), then the data request: 11 synchronisations, bits 0011, and a pause with 25 µs clock pulses, 23 every 912 µs then 2 x (3 + 8n + 8 + 4) every 456 µs (`MfxSignal::dataWindow()`; 49.2 ms for 2 bytes, as measured on a Marklin Gleisbox). An interrupt times the rising edges of the comparator during the pause with the CPU cycle counter; `MfxDemod` (`lib/MfxDemod`) measures the carrier period, follows its phase, finds the 180° phase changes, decodes the bi-phase bits, finds the preamble 010 and checks the checksum. Two tries per read.
- The read CV frames are identical, bit for bit, to frames recorded on the track of a Gleisbox; `MfxDemod` is tested on four answers recorded there (CV 000/00, 080/00, 003/01, 02F/01), on synthetic answers (noise, drift, false edges at the pulses) and on error cases.
- `Discovery::readName()`: name of the locomotive (CV 003/01 on, 16 characters). Read for each new decoder (printed with the line for `main.cpp`) and, as a test, for every declared locomotive when the track is first powered.
- Room for 16 locomotives (`MAX_LOCOS`): the declared ones and those found by the discovery. `Message::addLoco()`; the locomotive map is protected by a mutex.

### Changed
- RMT channel: the 8 memory blocks (512 items), so that a chunk with a data feedback of 8 bytes (up to 395 items) fits without refill.
- After a feedback request, the signal task sends one chunk from `mfxQueue` before the next request: the locomotives keep their refresh during a search.

### Fixed
- **Bit stuffing sent one bit too many** (new finding): after each inserted 0, the encoder read one more byte after the end of the frame and sent it on the track before the synchronisation. It concerned every frame with 8 consecutive 1 bits, for example the address assignment of the 150 X 5 (UID `FF E9 B1 94`).
- **Signal gaps** (#6): with nothing to send, the signal task sent a 0.5 ms synchronisation then slept 1 ms, leaving the track about 0.5 ms without transition at the RMT idle level; the level variable ignored this idle level, which could remove the expected transition at the start of the next chunk. The signal is now continuous (see below).
- The item buffer (100 items) could overflow with a long frame plus stuffing; chunks are now checked against their capacity.

### Added
- `MfxSignal` (`lib/MfxSignal`): turns frames into 50 µs half-bits (synchronisation, bits, stuffing) and keeps the track level from one chunk to the next. Pure C++, 5 unit tests.
- Orders from Rocrail (speed, direction, function) are sent at once, at the head of the queue (`Message::sendNow()`), instead of waiting for the next refresh (up to 50 ms). Requests send nothing.
- `MfxFrame` (`lib/MfxFrame`): builds the mfx frames (address on 7, 9, 11 or 14 bits, drive, functions F0-F15, Bind, command station broadcast, CRC). Pure C++, testable on a PC.
- Unit tests: `pio test -e native`. They check that `MfxFrame` gives exactly the same bits as the code of 0.10.0, and the CRC 0xD9 of a frame seen on an MS2.
- GitHub Actions: unit tests and ESP32 build on every push and pull request.

### Changed
- Track signal: one task keeps the RMT busy. It builds the next chunk while the current one is sent (two buffers), and sends synchronisations when there is nothing else, so the signal never stops. Each frame is sent between two synchronisations. The RMT idle level follows the level of the track, so the short pause between two chunks adds no transition. Whole chunks fit in the RMT memory (3 blocks).
- `Message::periodic()`, `centrale()` and `setSID()` build their frames with `MfxFrame`; `addr0()`, `addr()`, `CRC()` and `bCRC()` are removed. The track signal is unchanged.
- Project renamed **MFX Track Station** (formerly directMFX_ESP32); the startup banner shows the new name.
- README rewritten: what works today and what remains to do, compilation, Rocrail setup (CS2, port 15731), how to declare a locomotive; duplicated feature list removed.

## 0.10.0 — 2026-10-13

Corrections from the audit of 13 October 2026 (step 1 of the roadmap), without changing the architecture.

### Fixed
- **MFX re-registration counter** (#1): it was incremented by the default constructor of `Loco`, which `std::unordered_map::operator[]` calls. A Rocrail command for an unknown locomotive changed the counter and made every locomotive on the track register again. The counter now lives in `Centrale`, is sent on 16 bits, is kept in NVS and only changes on request (`Centrale::setCounter()`). Its default value, 5, is the one sent by 0.9.2 with 4 locomotives, so the update does not force a re-registration.
- **CS2 requests taken as orders** (#2): `data[]` kept the bytes of the previous frame, so a speed or function request (short frame) applied the value of the previous command, often to another locomotive. `data[]` is cleared, and requests no longer change anything.
- **Direction** (#7): CS2 values 0 (keep) and 3 (toggle) were read as "reverse". 0 keeps, 3 toggles, and a change of direction sets the speed to 0, as a CS2 does.
- **Unknown locomotives**: `Loco::findLoco()` returns `nullptr` instead of creating a locomotive that was never refreshed; the command is ignored with a message.
- **Overload** (#3, #4): the monitoring task had 1 KB of stack and printed a float at the moment of the overload (stack overflow). Stack is 4 KB, values are printed as integers. After an overload the track stays off until the next Go from Rocrail; it was powered again after 5 s, endlessly on a persistent short, and even after a Stop.
- **Current measurement** (#12): read in millivolts (`analogReadMilliVolts()`) and converted to mA with `CURRENT_MV_PER_A`, instead of comparing raw ADC counts to a value written in mV. Check `CURRENT_MV_PER_A` against your board.
- **Rocrail connection** (#5): the track signal and the tasks start at boot, without waiting for Rocrail. Rocrail can disconnect and reconnect without restarting the command station; the first frame is no longer lost. The network code is in `main.cpp` only, so `RxTask` no longer depends on WiFi.
- `1 << 31` on an `int` (undefined behaviour) when sending the UID of the command station (#16).
- The UID and counter of the command station are set before the first broadcast.
- `printf("%s")` was given a `String` object instead of `const char*` in the traces of `periodic()` and `setSID()` (crash as soon as `trace` was enabled).

### Changed
- TCP port 15731, the port of the Marklin CS2 used by default by Rocrail (it was 2560).
- Platform pinned to `espressif32@6.9.0` (Arduino core 2.0.17). Unpinned, PlatformIO now takes a platform with the Arduino core 3.x, where the project did not compile.
- IN2 inversion uses `esp_rom_gpio_pad_select_gpio()` and `esp_rom_gpio_connect_out_signal()`, available in the Arduino cores 2.x and 3.x, instead of `GPIO_PIN_MUX_REG` and `gpio_matrix_out()`, removed from the core 3.x.
- WiFi credentials go in `src/secrets.h`, ignored by Git (copy `src/secrets.h.example`).
- Removed dead code: second creation of `mfxQueue`, global `data[8]`, `TP` / `T_S88` / `Time`, `Pa[]`, `togglePin()`, `rrHash`, `Loco::zahler` and `locoActive`; unused `ACAN_ESP32` dependency (#15).
- `.DS_Store` files removed from the repository and ignored.
- Commands received from Rocrail are printed when `Message::setTrace(true)` is called (on by default in `main.cpp`); the very verbose traces of the internal tasks stay under `trace`.
- A locomotive declared with an address already used prints an error at boot (the second one replaces the first).
- UID of the 150 X 5 corrected: `FF E9 B1 94`, as seen on the layout with the MS2. Addresses: 150 X 5 on 5, 040 DE 9 moved to 4.

## 0.9.2

Version before the audit (commit `f02aa52`).
