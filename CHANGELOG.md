# Changelog

## Unreleased

### Added
- `MfxFrame` (`lib/MfxFrame`): builds the mfx frames (address on 7, 9, 11 or 14 bits, drive, functions F0-F15, Bind, command station broadcast, CRC). Pure C++, testable on a PC.
- Unit tests: `pio test -e native`. They check that `MfxFrame` gives exactly the same bits as the code of 0.10.0, and the CRC 0xD9 of a frame seen on an MS2.
- GitHub Actions: unit tests and ESP32 build on every push and pull request.

### Changed
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
