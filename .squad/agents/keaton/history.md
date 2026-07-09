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

### 2026-07-09: FINAL OUTCOME

- **ufbt build against Flipper SDK 1.4.3: SUCCEEDS**
- **Host test suite (~71k assertions): all PASS**
- **All blockers and major issues: RESOLVED via cross-author fix round**
- **Project approved for merge**

