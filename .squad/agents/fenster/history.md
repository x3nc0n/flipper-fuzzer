# Project Context

- **Owner:** John Spaid
- **Project:** FluckFlock — a Flipper Zero "chaff" broadcaster that continuously emits large volumes of plausible, ever-rotating fake wireless identifiers (Wi-Fi SSIDs/BSSIDs, BLE addresses/advert names, Sub-GHz RF noise/beacons) to pollute passive/dragnet tracking systems (Wi-Fi/BLE MAC tracking, SSID probe logging, RF fingerprinting).
- **Stack:** Flipper Zero firmware in C (FAP app, ufbt/fbt build, furi APIs, ViewDispatcher/SceneManager). Native BLE + Sub-GHz (CC1101); Wi-Fi via the official Flipper Wi-Fi Dev Board (ESP32) where present.
- **Target:** Flipper Zero ONLY — single hardware target (the earlier "two hardware tiers / two implementations" idea was scrubbed per user directive 2026-07-09). One implementation checklist: task-flipper.md.
- **Created:** 2026-07-09

## Learnings

<!-- Append new learnings below. Each entry is something lasting about the project. -->

### 2026-07-09 — FOUNDATIONAL DELIVERY: App + Engine + Drivers + Tests Complete

**Architecture finalized:**
- App entry: `fluckflock.c` with ViewDispatcher + SceneManager (MainMenu → Running / Settings / About)
- ChaffEngine: alloc/free/start/stop/step lifecycle; three radios (BLE / Sub-GHz / Wi-Fi) with enable/disable per-radio
- Radio drivers: BLE `emit(mac[6], name)`, Sub-GHz `emit(payload, len)`, Wi-Fi `detect/init/deinit/send(ssid, bssid)`
- Generators: pure-C `chaff_generate_ssid`, `chaff_generate_bssid`, `chaff_generate_ble_mac`, `chaff_generate_ble_name`, `chaff_generate_subghz_payload` + seedable PRNG (ChaffRng)
- Settings: binary struct persisted to `/ext/apps_data/fluckflock/settings.dat` (magic=0xF1F2, v1)
- Host tests: ~71k assertions, all pass
- **ufbt build against SDK 1.4.3: SUCCEEDS**; project APPROVED for merge

**Key UI decisions:**
- Running scene: full-width vertical layout (x=0) for all identifiers (BLE MACs, Sub-GHz payloads) — no two-column splits due to width overflow
- Identifiers bounded to 22 chars + snprintf truncation to avoid display overflow
- Wi-Fi toggle visible in Settings only when `wifi_detected == true`

**SDK detail:** GapAddressType enum (f7) has `GapAddressTypePublic=0` and `GapAddressTypeRandom=1`; SDK renamed from old `BLE_GAP_ADDR_TYPE_RANDOM_STATIC`.

**Icon format:** 10×10 px, 1-bit PNG, non-interlaced; generated with Pillow; ufbt validates at build time.

### 2026-07-18 — ESP32-S2 Companion Build & Flash on Windows

**PlatformIO setup:**
- `pip install --user platformio` installs Core 6.1.19; `pio` not on PATH — always use `python -m platformio`
- Auto-downloads espressif32 toolchain (~300 MB) to `%USERPROFILE%\.platformio\`

**Build fix (critical):**
- Remove `-DARDUINO_USB_CDC_ON_BOOT=1` from `platformio.ini` (incompatible with espressif32 3.20017.241212; HardwareSerial.cpp:51 error: 'Serial' not declared)
- Replace all `Serial0` with `Serial` in `main.cpp` (Serial0 undefined without CDC-on-boot flag)
- Side effect: debug output shared with Flipper UART link (UART0, GPIO 43/44) instead of USB CDC
- Result: Flash 45.9% (601 KB / 1.25 MB), RAM 11.2% (36 KB / 320 KB)

**Flash procedure (manual bootloader required — no DTR/RTS auto-reset on native USB CDC):**
1. Hold BOOT button, tap RESET (while holding BOOT), release BOOT
2. Device enumerates as VID_303A/PID_0002 on COM8 (NOT COM6/COM7)
3. Run: `python -m platformio run -t upload --upload-port COM8`
4. Post-flash: board resets, attempts COM6/COM7 (PID_4001) but CDC driver not fully ready

**Flipper FAP deployment:**
- Command: `python -m ufbt launch` (in `flipperzero/fluckflock/`)
- Prerequisite: Flipper at main desktop (no app running, USB-UART Bridge not open)
- Flipper detected as FLIP_DARWIN on COM5 (VID_0483/PID_5740)
- Installs to `/ext/apps/Tools/fluckflock.fap`, launches automatically

### 2026-07-18 — End-to-End Handshake Verification via Bridge

**Result: PASS** (via Flipper USB-UART Bridge on COM5 at 115200 baud)

**Verified commands:**
- `FF?\n` → received `b'[FF] rx: FF?\r\nFF!v1\r\n'` ✅
- `FFON\n` → received `b'[FF] rx: FFON\r\n[FF] injection ON ch=6\r\nOK\r\n'` ✅
- `FFOFF\n` → received `b'[FF] rx: FFOFF\r\n[FF] injection OFF\r\nOK\r\n'` ✅

**Key detail:** One-time `\n` pre-flush before first real command required (clears bridge-init UART noise from ESP rx buffer). FAP driver's 500ms open-delay covers this.

**Proof:** Protocol strings + baud + GPIO wiring all match exactly between Flipper driver (`radio_wifi.c`) and ESP32 firmware (`main.cpp`). UART0 GPIO 43 (TX) → Flipper GPIO 14 (RX), GPIO 44 (RX) ← Flipper GPIO 13 (TX). Second `Serial.begin()` in setup() binds UART0 to these pins at 115200. Flipper expansion header link is production-ready.

**Flipper app UI indicators:**
- Wi-Fi board detected: Settings shows Wi-Fi toggle; Running scene shows `WiFi N` counter + last SSID
- Wi-Fi board not detected: Settings hides Wi-Fi toggle; Running scene shows only `Back=Stop` hint

### 2026-07-18 — USB CDC Debug Path: Blocked (awaiting fix)

**Issue:** Firmware routes command protocol to UART0 (Flipper link) only, not USB CDC (removed `-DARDUINO_USB_CDC_ON_BOOT=1` as build workaround). Board's USB CDC (COM6/COM7) idle; no FF commands received over USB.

**Status:** Under Keaton review. Recommended fix: restore `ARDUINO_USB_CDC_ON_BOOT=1`, resolve `HardwareSerial.cpp:51` error via platform upgrade or explicit HWCDC include.

**Current state:** Flipper physical integration path READY. USB CDC verification path BLOCKED until fix applied.
