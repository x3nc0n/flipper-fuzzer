# Project Context

- **Owner:** John Spaid
- **Project:** FluckFlock — a Flipper Zero "chaff" broadcaster that continuously emits large volumes of plausible, ever-rotating fake wireless identifiers (Wi-Fi SSIDs/BSSIDs, BLE addresses/advert names, Sub-GHz RF noise/beacons) to pollute passive/dragnet tracking systems (Wi-Fi/BLE MAC tracking, SSID probe logging, RF fingerprinting).
- **Stack:** Flipper Zero firmware in C (FAP app, ufbt/fbt build, furi APIs, ViewDispatcher/SceneManager). Native BLE + Sub-GHz (CC1101); Wi-Fi via the official Flipper Wi-Fi Dev Board (ESP32) where present.
- **Target:** Flipper Zero ONLY — single hardware target (the earlier "two hardware tiers / two implementations" idea was scrubbed per user directive 2026-07-09). One implementation checklist: task-flipper.md.
- **Created:** 2026-07-09

## Learnings

<!-- Append new learnings below. Each entry is something lasting about the project. -->

### 2026-07-09: Foundational contract delivered

- **Files created:** `spec.md`, `task-flipper.md`, `.squad/decisions/inbox/keaton-architecture.md`
- **Source layout:** All firmware under `flipperzero/fluckflock/`. Host tests under `test/`.
- **Chaff engine interface:** `chaff_engine.h` — opaque `ChaffEngine*`, lifecycle `alloc/free/start/stop`, per-radio enable via `ChaffRadioType` enum (`ChaffRadioBLE=0, ChaffRadioSubGhz=1, ChaffRadioWifi=2`), stats via `ChaffStats`, tick via `chaff_engine_step()`.
- **Identifier generator interface:** `generators/identifiers.h` — pure C functions: `chaff_generate_ssid`, `chaff_generate_bssid`, `chaff_generate_ble_mac`, `chaff_generate_ble_name`, `chaff_generate_subghz_payload`. All take a `ChaffRng*` — no Flipper SDK dependency, host-testable.
- **PRNG interface:** `generators/prng.h` — `ChaffRng*` with `chaff_rng_seed`, `chaff_rng_u32`, `chaff_rng_bytes`, `chaff_rng_bounded`. Seedable for deterministic tests.
- **File ownership:** Fenster owns app/UI/scenes + engine impl. McManus owns generators + radio drivers. Hockney owns `test/`. Keaton owns spec, checklist, and interface signatures.
- **Key constraint:** Generators are pure (no SDK), radios are Flipper-only, engine owns the loop, Wi-Fi gated on dev board detection.

### 2026-07-09: Reviewer Gate — REQUEST CHANGES (2 blockers, 1 major, all RESOLVED)

Full verdict written to `.squad/decisions.md` (merged from inbox).

**Blockers found → FIXED:**
1. `scenes/scenes.c` — `SceneManagerHandlers` type used for both array elements and outer config struct. Self-contradictory struct field usage = compile error. **Fix applied by Hockney.**
2. `fluckflock.c:fluckflock_app_free()` — unconditional `furi_timer_free(NULL)` on normal path, double-free on abnormal path. Crashes every exit. **Fix applied by McManus.**

**Major → FIXED:**
3. `radio/radio_subghz.c` — `furi_hal_subghz_acquire()`/`release()` not called despite being documented as required. HAL assertions likely on current firmware. **Fix applied by Hockney.**

**Passed checks:** chaff_engine.h verbatim match, all generator/radio signatures correct, BSSID/BLE MAC address bits correct, Wi-Fi gated properly, generators pure, PRNG deterministic, host tests comprehensive, application.fam correct.


### 2026-07-09: Wi-Fi Implementation Review — APPROVE

Reviewed McManus's Wi-Fi dev-board implementation:
- `flipperzero/fluckflock/radio/wifi_proto.h` + `wifi_proto.c` (pure-C protocol module)
- `flipperzero/fluckflock/radio/radio_wifi.c` (full UART driver, replacing stub)
- `esp32/fluckflock_companion/src/main.cpp` + `platformio.ini` + `README.md`

**Verdict: APPROVE — no blockers.**

Flipper-side driver is correct: furi_hal_serial acquire/init/tx/async_rx/release ordering valid, NULL handle guarded, detect timeout safe (furi_semaphore 400 ms, all paths free resources), no leaks, deinit ordering correct (FFOFF before serial_deinit). Protocol constants consistent host ↔ companion. Beacon frame layout is structurally valid 802.11. `radio_wifi_emit(ssid, bssid)` call in chaff_engine.c matches implemented signature.

Three MEDIUM-confidence ESP32-side observations (not blockers): `Serial.println()` sends `\r\n` (works with prefix-only check), `en_sys_seq=false` gives seq=0 on all frames (harmless for chaff), non-idempotent init in `wifi_injection_start()` (harmless since FFON is sent once).

One MINOR doc issue: stale "Currently stubbed" comment in `radio_wifi.h` — assigned to Fenster/Hockney as a cleanup one-liner.

**Docs updated:**
- `task-flipper.md` — Wi-Fi section: checked off 7 completed items, left 4 items unchecked (host tests, UI toggle, on-device validation, channel-hopping), added new TODO items.
- `spec.md` — Section 5.3: replaced "if present" stub description with full implemented approach (UART protocol, companion firmware, beacon frame injection, fixed channel limitation).

Full verdict at `.squad/decisions/inbox/keaton-wifi-review.md`.


- **ufbt build against Flipper SDK 1.4.3: SUCCEEDS**
- **Host test suite (~71k assertions): all PASS**
- **All blockers and major issues: RESOLVED via cross-author fix round**
- **Project approved for merge**

### 2026-07-09: Real hardware validation: FAP confirmed working on Flipper Zero

Deployed to physical Flipper Zero over USB COM5 via `ufbt launch` (Windows + SDK 1.4.3).
User visually confirmed app running with live BLE + Sub-GHz emission counters incrementing.
Wi-Fi companion firmware implemented but dev board not yet physically available for testing.
See orchestration-log and session log for full deployment details.

### 2026-07-09: Public README authored for MIT open-source release

- Authored `README.md` at repo root for the public GitHub release of FluckFlock under the MIT license.

## Learnings

### 2026-07-09: README updated — Wi-Fi implemented-but-not-hardware-validated; CI badge added

- Status table now reflects real state: BLE + Sub-GHz marked ✅ verified on real hardware; Wi-Fi row changed from 🚧 stubbed to 🧪 implemented + host-tested, pending on-device validation.
- Stale "UART protocol is currently stubbed" and "command protocol not yet implemented" language replaced with accurate description of the `wifi_proto` line protocol, ESP32-S2 companion firmware, and honest caveat that on-device flash is blocked by USB enumeration issue.
- File tree updated: `radio_wifi.c` no longer marked stub; `wifi_proto.c/.h`, `test_wifi_proto.c`, and `esp32/fluckflock_companion/` added.
- New "Wi-Fi Dev Board" subsection added covering two-part setup (flash PlatformIO companion, then mount board).
- Host Tests CI badge (`host-tests.yml`) added beneath existing badges.

### 2026-07-18: Fenster ESP32-S2 companion build & flash — Review PENDING

**Fenster (2 sync runs):** Built and flashed `esp32/fluckflock_companion` to Official Flipper Wi-Fi Dev Board (ESP32-S2-WROVER-I).

**Outcome:** ✅ Build SUCCEEDED after 2 espressif32 framework compatibility fixes; ✅ Flash SUCCEEDED on COM8 after manual bootloader entry.

**Changes (UNCOMMITTED — await Keaton review):**
1. `esp32/fluckflock_companion/platformio.ini` — Removed `-DARDUINO_USB_CDC_ON_BOOT=1` (framework bug: HardwareSerial.cpp doesn't resolve Serial when HWCDC enabled). Debug output now shared with Flipper UART link (UART0).
2. `esp32/fluckflock_companion/src/main.cpp` — Replaced all `Serial0` with `Serial` (Serial0 undefined without USB CDC boot flag).

**Binary:** Flash 45.9% / RAM 11.2%  
**Chip verified:** ESP32-S2 rev v1.0, all 4 partitions written/verified  
**ROM bootloader:** VID_303A/PID_0002 on COM8; app mode on COM6/COM7

**Flash procedure:** Manual bootloader entry required (no DTR/RTS auto-reset over USB CDC). Hold BOOT, tap RESET, release BOOT, then `python -m platformio run -t upload --upload-port COM8`.

**Action items for Keaton:** Review platformio.ini and main.cpp changes before merge. Consider dedicated debug UART or USB CDC approach for v2.

