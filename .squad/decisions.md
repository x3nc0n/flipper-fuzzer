# Squad Decisions

## Active Decisions

### 2026-07-09: Single hardware target — Flipper Zero only

**By:** John Spaid (via Copilot)
**Status:** Final

Scrub the "two independent implementations / two hardware tiers" concept from the FluckFlock spec. Focus the entire project on the Flipper Zero platform only. One implementation, one target.

**Rationale:** User directive — "just focus this on the Flipper Zero platform."

**Implication:** No `task-<platform>.md` fan-out across tiers; a single Flipper implementation checklist. Charters/spec should not reference a second hardware tier or companion-MCU-as-separate-implementation.

---

### 2026-07-09: Architecture — Repo Layout, Chaff Engine Interface, Identifier Generator Interface, File Ownership

**By:** Keaton (Lead / Firmware Architect)
**Status:** Approved

#### 1. Repo / Source Layout

```
flipper-fuzzer/                          # repo root
├── spec.md                              # FluckFlock specification
├── task-flipper.md                      # implementation checklist
├── flipperzero/
│   └── fluckflock/
│       ├── application.fam              # FAP manifest
│       ├── fluckflock.c                 # app entry point, ViewDispatcher, SceneManager setup
│       ├── fluckflock.h                 # App struct, shared types/forward declarations
│       ├── chaff_engine.h               # chaff engine public interface (header)
│       ├── chaff_engine.c               # chaff engine implementation
│       ├── generators/
│       │   ├── identifiers.h            # identifier generator interface (header)
│       │   ├── identifiers.c            # identifier generator implementation
│       │   ├── oui_table.h              # OUI prefix table (data)
│       │   ├── ssid_table.h             # SSID template table (data)
│       │   ├── ble_name_table.h         # BLE device name table (data)
│       │   └── prng.h / prng.c          # seedable PRNG (xoshiro256** or similar)
│       ├── radio/
│       │   ├── radio_ble.h              # BLE radio interface
│       │   ├── radio_ble.c              # BLE radio implementation
│       │   ├── radio_subghz.h           # Sub-GHz radio interface
│       │   ├── radio_subghz.c           # Sub-GHz radio implementation
│       │   ├── radio_wifi.h             # Wi-Fi dev board interface
│       │   └── radio_wifi.c             # Wi-Fi dev board implementation
│       ├── scenes/
│       │   ├── scene_main_menu.c        # main menu scene
│       │   ├── scene_running.c          # running/status scene
│       │   ├── scene_settings.c         # settings scene
│       │   └── scene_about.c            # about/disclaimer scene
│       ├── views/
│       │   └── view_status.c / .h       # custom view for live counters (if needed)
│       └── icons/                       # app icon assets
└── test/
    ├── test_identifiers.c               # host-side tests for identifier generators
    ├── test_prng.c                      # host-side tests for PRNG determinism
    └── Makefile                         # builds tests with host cc (gcc/clang), no Flipper SDK
```

#### 2. Chaff Engine Interface — `flipperzero/fluckflock/chaff_engine.h`

Contract between app layer (Fenster) and radio/generator layer (McManus). App calls lifecycle + enable functions; engine internally calls generators and radio drivers.

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ChaffRadioBLE    = 0,
    ChaffRadioSubGhz = 1,
    ChaffRadioWifi   = 2,
    ChaffRadioCount  = 3,
} ChaffRadioType;

typedef struct {
    uint32_t emitted_count;
    char     last_ident[33];
} ChaffStats;

typedef struct ChaffEngine ChaffEngine;

ChaffEngine* chaff_engine_alloc(void);
void chaff_engine_free(ChaffEngine* engine);
void chaff_engine_start(ChaffEngine* engine, uint32_t interval_ms);
void chaff_engine_stop(ChaffEngine* engine);
bool chaff_engine_is_running(const ChaffEngine* engine);
void chaff_engine_set_radio_enabled(ChaffEngine* engine, ChaffRadioType radio, bool enabled);
bool chaff_engine_get_radio_enabled(const ChaffEngine* engine, ChaffRadioType radio);
void chaff_engine_get_stats(const ChaffEngine* engine, ChaffRadioType radio, ChaffStats* out);
void chaff_engine_reset_stats(ChaffEngine* engine);
void chaff_engine_step(ChaffEngine* engine);
```

#### 3. Identifier Generator Interface — `flipperzero/fluckflock/generators/identifiers.h`

Pure functions. No Flipper SDK dependency. Testable on any host with a C compiler.

```c
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct ChaffRng ChaffRng;

size_t chaff_generate_ssid(char* buf, size_t buf_len, ChaffRng* rng);
void chaff_generate_bssid(uint8_t out[6], ChaffRng* rng);
void chaff_generate_ble_mac(uint8_t out[6], ChaffRng* rng);
size_t chaff_generate_ble_name(char* buf, size_t buf_len, ChaffRng* rng);
void chaff_generate_subghz_payload(uint8_t* buf, size_t len, ChaffRng* rng);
```

#### 4. PRNG interface — `flipperzero/fluckflock/generators/prng.h`

```c
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct ChaffRng ChaffRng;

ChaffRng* chaff_rng_alloc(void);
void chaff_rng_free(ChaffRng* rng);
void chaff_rng_seed(ChaffRng* rng, uint64_t seed);
uint32_t chaff_rng_u32(ChaffRng* rng);
void chaff_rng_bytes(ChaffRng* rng, uint8_t* buf, size_t len);
uint32_t chaff_rng_bounded(ChaffRng* rng, uint32_t upper_bound);
```

#### 5. File Ownership Map

| Owner | Files |
|---|---|
| **Keaton** | `spec.md`, `task-flipper.md`, `chaff_engine.h` (interface changes only), `.squad/decisions/` |
| **Fenster** | `fluckflock.c`, `fluckflock.h`, `scenes/*`, `views/*`, `application.fam`, `chaff_engine.c` |
| **McManus** | `generators/*`, `radio/*` |
| **Hockney** | `test/*` |

**Key Rules:** Generators are pure (no SDK), radios are Flipper-only, engine owns the loop, Wi-Fi gated on detection.

---

### 2026-07-09: Review Verdict — REQUEST CHANGES (2 blockers, 1 major, all resolved)

**Date:** 2026-07-09T16:10:46-05:00
**Reviewer:** Keaton (Lead)
**Scope:** Integrated FluckFlock Flipper Zero app
**Initial Verdict:** REQUEST CHANGES

#### Blockers (both resolved)

##### 1. scenes.c — SceneManagerHandlers type error → FIXED

**Issue:** Used `SceneManagerHandlers` for both array elements and outer struct — incompatible field sets.

**Fix (by Hockney):** Restructured to use three separate callback arrays, assigned to correct `SceneManagerHandlers` fields.

**Status:** ✅ RESOLVED

##### 2. fluckflock.c — Double-free / NULL-free crash → FIXED

**Issue:** Unconditional `furi_timer_free(NULL)` in teardown; use-after-free on abnormal path.

**Fix (by McManus):** Replaced entire block with proper NULL-guard:
```c
if(app->status_refresh_timer) {
    furi_timer_stop(app->status_refresh_timer);
    furi_timer_free(app->status_refresh_timer);
    app->status_refresh_timer = NULL;
}
```

**Status:** ✅ RESOLVED

#### Major Issues (resolved)

##### 3. radio_subghz.c — Missing furi_hal_subghz acquire/release → FIXED

**Issue:** `furi_hal_subghz_acquire()` / `release()` not called despite being documented as required.

**Fix (by Hockney):** Called `furi_hal_subghz_acquire()` in `radio_subghz_init()` and `furi_hal_subghz_release()` in `radio_subghz_deinit()`.

**Status:** ✅ RESOLVED

#### Final Status

**All blockers and major issues resolved.** ufbt build against Flipper SDK 1.4.3 SUCCEEDS. Host test suite (71k+ assertions) all PASS.

**Conformance Summary:** 13/13 checks PASS.

---

### 2026-07-09: Radio Driver Signatures — Fenster Engine Integration Reference

**By:** Fenster (Embedded Firmware Dev)
**Status:** Informational — all signatures finalized

#### Stable Radio Signatures (McManus-owned)

**radio_ble.h:**
```c
void radio_ble_init(void);
void radio_ble_deinit(void);
bool radio_ble_emit(const uint8_t mac[6], const char* name);
```

**radio_subghz.h:**
```c
void radio_subghz_init(void);
void radio_subghz_deinit(void);
bool radio_subghz_emit(const uint8_t* payload, size_t len);
```

**radio_wifi.h:**
```c
bool radio_wifi_detect(void);
bool radio_wifi_init(void);
void radio_wifi_deinit(void);
bool radio_wifi_emit(const char* ssid, const uint8_t bssid[6]);
```

#### Timer-Task Constraint

`chaff_engine_step()` called from FuriTimer callback. All `_emit()` calls must complete within configured interval (minimum 500 ms). Worst-case budget: < 200 ms per emit.

---

### 2026-07-09: McManus — Radio Driver Interface Signatures

**By:** McManus (Wireless / RF Dev)
**Status:** Final

All three emit functions follow consistent pattern. See **Radio Driver Signatures** decision above for full reference.

---

### 2026-07-09: Review — FluckFlock Wi-Fi UART Implementation

**By:** Keaton (Lead / Firmware Architect)  
**Date:** 2026-07-09  
**Reviewing:** McManus's Wi-Fi dev-board implementation (radio_wifi.c, wifi_proto.h/c, esp32/fluckflock_companion/)  
**Verdict:** ✅ APPROVE — no blockers found

#### Summary

The Flipper-side driver builds clean under ufbt SDK 1.4.3 (confirmed in PR description).  
All `furi_hal_serial_*` usage is correct. The detect handshake is timeout-safe. The ESP32  
companion's beacon frame construction is structurally valid 802.11. Protocol constants are  
consistent across host and companion. No HIGH-confidence issues were found.

#### Detailed Findings

**Flipper Side (`radio_wifi.c`)**

| Check | Result |
|---|---|
| acquire → init → (use) → deinit → release ordering in detect() | ✓ Correct |
| acquire → init → (use) in init(); deinit → release in deinit() | ✓ Correct |
| NULL handle guard after acquire (busy USART → return false) | ✓ radio_wifi_detect() and radio_wifi_init() both check |
| Double-acquire guard in radio_wifi_init() | ✓ `wifi_initialized` and `wifi_serial` guards prevent re-entry |
| No leak on semaphore alloc failure in wifi_do_detect() | ✓ Early return before alloc; no resource held at that point |
| async_rx_start / async_rx_stop symmetric around semaphore wait | ✓ Both paths (timeout and success) call rx_stop + sem_free |
| async_rx / async_rx_available called only from callback | ✓ Both in wifi_rx_callback only |
| tx_wait_complete called after every tx | ✓ wifi_tx_str() wraps both |
| deinit ordering: FFOFF sent before serial_deinit | ✓ Correct |

**detect() timeout logic — PASS**  
Uses `furi_semaphore_acquire(…, furi_ms_to_ticks(400))`. On timeout `status != FuriStatusOk` → returns false cleanly. Async RX and semaphore are freed on all paths. No blocking-forever risk.

**Semaphore max-count saturation in callback — PASS**  
Semaphore allocated with max_count=1. If the callback fires again after releasing (before async_rx_stop), the second `furi_semaphore_release` returns FuriStatusErrorResource and does not over-count. Buffer remains intact.

**chaff_engine.c — radio_wifi_emit call site — PASS**  
`radio_wifi_emit(ssid, bssid)` at line 209 matches the function signature in radio_wifi.h. SSID is null-terminated via `chaff_generate_ssid`; BSSID is a 6-byte array from `chaff_generate_bssid`. Arguments are correctly ordered.

**ESP32 Companion (`esp32/fluckflock_companion/src/main.cpp`)**

**802.11 Beacon Frame Construction — PASS (medium confidence; can't compile ESP32 here)**

Frame layout in `build_beacon()`:
- Frame Control `0x80 0x00` → Mgmt type (0), Beacon subtype (8) ✓
- DA = broadcast `ff:ff:ff:ff:ff:ff` ✓
- SA (offset 10) = BSSID patched at runtime ✓
- BSSID (offset 16) = BSSID patched at runtime ✓
- Timestamp 8 bytes, Beacon interval 100 TU, Capability 0x0431 (ESS + Short Preamble + Short Slot Time) ✓
- SSID tagged element (tag=0x00) ✓
- Supported Rates IE (tag=0x01, 8 rates) ✓
- DS Parameter Set (tag=0x03, channel) ✓
- `esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false)` ✓

Header offset arithmetic verified: SA at offset 10, BSSID at offset 16 matches the `BEACON_HDR` layout.  
Promiscuous mode correctly enabled before injection (`esp_wifi_set_promiscuous(true)`).

**Protocol consistency (host ↔ companion) — PASS**  
Command strings match across `wifi_proto.h` and `main.cpp`: `FF?`, `FFON`, `FFOFF`, `FFB`.  
Reply prefixes match: `FF!v1`, `OK`.  
BSSID encoding: 12 lowercase hex chars, big-endian — consistent.

#### Medium-Confidence Observations (ESP32 side; no fix required to approve)

**M1 — `Serial.println()` sends `\r\n`, spec says `\n`**  
`process_line()` uses `Serial.println("FF!v1")` which appends `\r\n` rather than just `\n`.  
The Flipper's `wifi_rx_callback` triggers on `\n`, so `rx.buf` contains `FF!v1\r`. The prefix-only check (`FF!`) is unaffected. *Works correctly with current implementation.* If a future change performs a full string comparison against `WIFI_PROTO_HANDSHAKE_REPLY_V1` ("FF!v1\n"), it would fail on the `\r`. Not a blocker — note for Fenster or Hockney if detection logic is ever hardened.

**M2 — `en_sys_seq=false` → all injected beacons have sequence number 0**  
`esp_wifi_80211_tx(..., false)` preserves the `0x0000` sequence control field from the template. For chaff purposes this is inconsequential. Using `true` would generate incrementing sequence numbers for more realistic-looking frames. Not a blocker.

**M3 — `wifi_injection_start()` calls non-idempotent init functions**  
`nvs_flash_init()` and `esp_event_loop_create_default()` return error codes if already initialized. Protocol prevents double-FFON (Flipper only calls FFON once), so this is harmless in practice. Not a blocker.

#### Documentation Issue (minor, no author lock-out constraint)

`radio_wifi.h` — `radio_wifi_detect()` doc block still says:  
> "NOTE: Currently stubbed to return false — see TODO in radio_wifi.c."

This is stale; the function is fully implemented. McManus is locked out of self-fixes. Either  
**Fenster** or **Hockney** should update this comment as a one-line cleanup (no logic change).  
Severity: MINOR / documentation only.

#### Checklist Review

- [x] furi_hal_serial acquire/init/tx/async_rx/release correct
- [x] NULL handle (busy USART) guarded → returns false
- [x] No leaks / no double-acquire
- [x] deinit ordering correct (FFOFF before serial_deinit)
- [x] detect() timeout-safe, no blocking-forever
- [x] Beacon frame structurally valid 802.11
- [x] addr2/addr3 (SA/BSSID) = bssid ✓
- [x] SSID tagged element present
- [x] esp_wifi_80211_tx called correctly
- [x] Protocol strings consistent host ↔ companion
- [x] radio_wifi_emit called correctly from chaff_engine.c

#### Action Items

| # | Severity | File | Issue | Assigned to |
|---|---|---|---|---|
| M1 | Medium | `esp32/.../main.cpp` | `Serial.println` → `\r\n`; consider `Serial.print(…"\n")` for spec compliance | Fenster (cleanup, not blocking) |
| M2 | Medium | `esp32/.../main.cpp` | Pass `true` to `esp_wifi_80211_tx` for auto seq numbers (more realistic) | Fenster (optional enhancement) |
| D1 | Minor | `radio/radio_wifi.h` | Remove stale "currently stubbed" comment | Fenster or Hockney (one-liner) |

McManus (original author) is **locked out** of all fix assignments per reviewer-rejection lockout policy. Items above are improvements, not blockers — no lock is triggered since the verdict is APPROVE.

---

### 2026-07-09: Decision — FluckFlock Wi-Fi UART Protocol & Companion Firmware

**By:** McManus (Wireless / RF Dev)  
**Date:** 2026-07-09  
**Status:** Final  
**For:** Keaton (task-flipper.md gating), Hockney (host test authorship)

#### 1. UART Line Protocol — Complete Specification

Physical layer: **FuriHalSerialIdUsart**, 115200 baud, 8N1.  
Pins (Flipper expansion header): GPIO 13 = TX out, GPIO 14 = RX in.  
All frames are ASCII, newline-terminated (`\n`).  The Flipper is always the host (initiator).

| Flipper → ESP32 | ESP32 → Flipper | Purpose |
|---|---|---|
| `FF?\n` | `FF!v1\n` | Handshake / board detection (used by `radio_wifi_detect()`) |
| `FFON\n` | `OK\n` | Enter 802.11 raw-injection mode (sent by `radio_wifi_init()`) |
| `FFOFF\n` | `OK\n` | Exit injection mode (sent by `radio_wifi_deinit()`) |
| `FFB <bssid12> <ssid>\n` | *(none)* | Inject one 802.11 beacon (sent by `radio_wifi_emit()`) |

**Field details:**

- `bssid12`: 12 **lowercase** hex chars, no separators.  
  Example: `aabbccddeeff` (encoding of `AA:BB:CC:DD:EE:FF`)
- `ssid`: raw null-terminated SSID, 1–32 printable bytes, no newlines.  
  The chaff engine always validates this before calling emit().
- Beacon command is **fire-and-forget** — no ACK is expected from the companion.
- `FF?` / `FFON` / `FFOFF` replies are used by detect/init paths; the driver waits up to
  400 ms for the handshake reply, then returns false (board absent / busy).

#### 2. `wifi_proto` Function Signatures (for Hockney's host tests)

File location: `flipperzero/fluckflock/radio/wifi_proto.h` + `wifi_proto.c`

**No Flipper SDK dependencies** — only `<stdint.h>`, `<stddef.h>`, `<string.h>`.

```c
/* Build "FF?\n" into buf. Returns bytes written (excl. '\0'), or 0 on error. */
size_t wifi_proto_build_handshake(char* buf, size_t buf_len);

/* Build "FFON\n" into buf. Returns bytes written (excl. '\0'), or 0 on error. */
size_t wifi_proto_build_on(char* buf, size_t buf_len);

/* Build "FFOFF\n" into buf. Returns bytes written (excl. '\0'), or 0 on error. */
size_t wifi_proto_build_off(char* buf, size_t buf_len);

/*
 * Build "FFB <bssid12hex> <ssid>\n" into buf.
 * bssid:   6-byte array, big-endian (network byte order).
 * ssid:    null-terminated, 1–32 bytes, no newlines.
 * Returns: bytes written (excl. '\0'), or 0 on error.
 * Required buf_len: at least (19 + strlen(ssid)) bytes.
 * Maximum output length (WIFI_PROTO_BEACON_CMD_MAX_LEN): 50 bytes + '\0'.
 */
size_t wifi_proto_build_beacon(
    char*          buf,
    size_t         buf_len,
    const char*    ssid,
    const uint8_t  bssid[6]);
```

**Constants defined in `wifi_proto.h`:**

```c
#define WIFI_PROTO_HANDSHAKE_CMD          "FF?\n"
#define WIFI_PROTO_HANDSHAKE_REPLY_PREFIX "FF!"
#define WIFI_PROTO_HANDSHAKE_REPLY_V1     "FF!v1\n"
#define WIFI_PROTO_ON_CMD                 "FFON\n"
#define WIFI_PROTO_OFF_CMD                "FFOFF\n"
#define WIFI_PROTO_OK_REPLY               "OK\n"
#define WIFI_PROTO_BEACON_CMD_MAX_LEN     50u
```

#### 3. ESP32 Board Target

| Item | Value |
|---|---|
| SoC | ESP32-S2 (Xtensa LX7) |
| Board | Official Flipper Zero Wi-Fi Dev Board (ESP32-S2-WROVER-I) |
| PlatformIO board ID | `esp32-s2-saola-1` |
| Framework | Arduino (espressif32) |
| Monitor speed | 115200 |

#### 4. UART Pin Mapping

| Flipper Zero GPIO | ESP32-S2 GPIO | Direction | Notes |
|---|---|---|---|
| 13 (TX) | 44 (UART0 RX / U0RXD) | Flipper → ESP32 | Flipper transmits; ESP32 receives |
| 14 (RX) | 43 (UART0 TX / U0TXD) | ESP32 → Flipper | ESP32 replies; Flipper receives |
| GND | GND | — | Common ground |

The official Flipper Wi-Fi Dev Board mates to the expansion connector directly — no extra 
wiring required.

Acquiring `FuriHalSerialIdUsart` on the Flipper suspends the Flipper serial console 
(expected for dev-board apps).

#### 5. Files Created / Modified

| File | Status |
|---|---|
| `flipperzero/fluckflock/radio/wifi_proto.h` | **Created** — pure-C protocol module header |
| `flipperzero/fluckflock/radio/wifi_proto.c` | **Created** — pure-C protocol module implementation |
| `flipperzero/fluckflock/radio/radio_wifi.c` | **Modified** — full UART driver (replaces stub) |
| `esp32/fluckflock_companion/platformio.ini` | **Created** — PlatformIO project config |
| `esp32/fluckflock_companion/src/main.cpp` | **Created** — ESP32-S2 companion firmware |
| `esp32/fluckflock_companion/README.md` | **Created** — build/flash/wiring/legal docs |

#### 6. SDK Symbols Used (verified against Flipper SDK 1.4.3)

```c
furi_hal_serial_control_acquire(FuriHalSerialIdUsart)  → FuriHalSerialHandle*
furi_hal_serial_control_release(FuriHalSerialHandle*)
furi_hal_serial_init(FuriHalSerialHandle*, uint32_t baud)
furi_hal_serial_deinit(FuriHalSerialHandle*)
furi_hal_serial_tx(FuriHalSerialHandle*, const uint8_t*, size_t)
furi_hal_serial_tx_wait_complete(FuriHalSerialHandle*)
furi_hal_serial_async_rx_start(handle, callback, context, report_errors)
furi_hal_serial_async_rx_stop(FuriHalSerialHandle*)
furi_hal_serial_async_rx_available(FuriHalSerialHandle*)  → bool  [call from callback only]
furi_hal_serial_async_rx(FuriHalSerialHandle*)            → uint8_t [call from callback only]
furi_semaphore_alloc(max_count, initial_count)            → FuriSemaphore*
furi_semaphore_free(FuriSemaphore*)
furi_semaphore_acquire(FuriSemaphore*, uint32_t timeout)  → FuriStatus
furi_semaphore_release(FuriSemaphore*)                    → FuriStatus
furi_ms_to_ticks(uint32_t ms)                             → uint32_t
```

---

### 2026-07-18: ESP32 companion firmware — PlatformIO invocation on Windows

**Date:** 2026-07-18  
**Author:** Fenster  
**Status:** Active

#### Context

First-time build and flash of `esp32\fluckflock_companion\` on a Windows machine.

#### Decisions Made

##### 1. PlatformIO invoked as `python -m platformio`, never as `pio`

The `pio` shim is not on PATH after `pip install --user platformio` on the Windows Store Python (3.13). All build and flash commands must use `python -m platformio run [...]`. This is the canonical invocation for this machine.

##### 2. Remove `-DARDUINO_USB_CDC_ON_BOOT=1` from `platformio.ini` build_flags

The flag causes a build failure in espressif32 framework 3.20017.241212 on ESP32-S2:
`HardwareSerial.cpp:51: error: 'Serial' was not declared in this scope`
This is a framework bug (HWCDC replaces Serial but HardwareSerial.cpp doesn't see the extern).

**Removed flag from `platformio.ini`.** Consequence: debug output goes to UART0 (same as Flipper link) rather than USB CDC. Acceptable for v1 — revisit when a dedicated debug UART or USB CDC approach is chosen.

##### 3. Replace `Serial0` with `Serial` in `main.cpp`

`Serial0` is undefined without `ARDUINO_USB_CDC_ON_BOOT=1` in espressif32 3.x. All `Serial0` calls replaced with `Serial`. Both debug prints and Flipper-link UART now use `Serial` (UART0, GPIO43/44). Minor protocol noise risk if Flipper firmware doesn't tolerate unexpected lines; accepted for now.

##### 4. ESP32-S2 native USB requires manual bootloader entry for flashing

The official Flipper Wi-Fi Dev Board (ESP32-S2-WROVER-I) does not support esptool's DTR/RTS auto-reset over native USB CDC. Auto-upload fails with "No serial data received" on both COM6 and COM7.

**Required flash procedure:**
1. Hold BOOT button
2. Tap RESET (while holding BOOT)
3. Release BOOT
4. Immediately run: `python -m platformio run -t upload --upload-port COM6` (or COM7)

This must be documented in the project README and any flash scripts.

##### Files Modified

- `esp32\fluckflock_companion\platformio.ini` — removed `-DARDUINO_USB_CDC_ON_BOOT=1`
- `esp32\fluckflock_companion\src\main.cpp` — replaced all `Serial0` with `Serial`

---

### 2026-07-18: ESP32-S2 USB CDC Handshake Verification — FAIL

**Filed by:** Fenster  
**Date:** 2026-07-18T21:27Z  
**Severity:** Blocker — Wi-Fi companion board cannot be verified or used via USB CDC  
**Status:** Under review (Keaton)

#### Summary

The `FF?` → `FF!v1` handshake verification attempt on the freshly-flashed ESP32-S2 board **FAILED** for two compounding reasons:

1. **Hardware not present**: Board's USB CDC (VID_303A/PID_4001) shows `Present: False` in Windows PnP. COM6 and COM7 cannot be opened.

2. **Firmware routes command protocol to UART0 hardware, not USB CDC** (critical design issue): `ARDUINO_USB_CDC_ON_BOOT=1` was removed from `platformio.ini` as a workaround for a build error. Without it, `Serial` = UART0 hardware (GPIO 43 TX / GPIO 44 RX). All `Serial.read()` / `Serial.println()` calls in `main.cpp` — including `process_line()` and the `FF?` / `FF!v1` handler — operate on UART0 hardware pins, not USB CDC.

#### Root Cause

The build fix that removed `ARDUINO_USB_CDC_ON_BOOT=1` broke the intended serial routing. The original design expected `Serial` = USB CDC (for debug + command protocol), with hardware UART0 as the Flipper physical link.

#### Recommended Action

Option A (cleanest): Restore `ARDUINO_USB_CDC_ON_BOOT=1`, fix `HardwareSerial.cpp:51` error via platform upgrade or explicit `HWCDC` include. Matches original design intent.

**Status:** Keaton to review and decide on fix approach. Current firmware edits remain uncommitted pending review.

---

### 2026-07-18: UART0 Build IS Correct for Flipper Physical Link — CONFIRMED

**Filed by:** Fenster  
**Date:** 2026-07-18T21:35Z  
**Status:** CONFIRMED — Flipper physical link path is operational  

#### Summary

Hypothesis *"the current UART0 build works over the Flipper expansion-header link"* is **CONFIRMED**. The prior "handshake FAIL" was USB-CDC-only and does **NOT** affect the Flipper physical link.

#### Evidence / Reasoning

**Protocol exact match:**
- Flipper sends `FF?\n` (wifi_proto.h), checks for `FF!` response (radio_wifi.c)
- ESP32 receives via UART0, responds `FF!v1\n` to `FF?` via same UART0 (main.cpp)
- Both use 115200 baud
- Strings and baud match exactly

**UART routing:**
- `Serial.begin()` called twice in setup(); second call wins: UART0 GPIO 43 (TX) → Flipper GPIO 14 RX, GPIO 44 (RX) ← Flipper GPIO 13 TX at 115200 baud
- Flipper driver acquires FuriHalSerialIdUsart (expansion header USART, GPIO 13/14) at 115200 baud
- **The two endpoints are correctly wired and configured**

**Physical verification (2026-07-18):**
- End-to-end FF handshake verified via Flipper USB-UART Bridge:
  - `FF?\n` → received `FF!v1\r\n` ✅
  - `FFON\n` → received `OK\r\n` ✅  
  - `FFOFF\n` → received `OK\r\n` ✅
- One-time `\n` pre-flush needed on first bridge open to clear bridge-init UART noise; FAP's 500ms open-delay covers this in production

#### Conclusion

**The current flashed firmware is ready for Flipper physical integration.** No reflash or USB CDC fix required for the Flipper expansion header communication path. Only physical mount test remains: board on Flipper expansion connector → FAP deployed via `python -m ufbt launch` → check `wifi_detected` flag.

---

### 2026-07-18: RF Validation Playbook — FluckFlock Live Chaff

**Owner:** McManus (Wireless / RF Dev)  
**Date:** 2026-07-18  
**Status:** Reusable reference — run whenever you need external confirmation that FluckFlock is actually radiating.

#### What the identifiers look like

##### Wi-Fi SSIDs (10 templates, channel 6)

| Template | Example |
|---|---|
| ISP prefix + 4 HEX | `NETGEAR4F2A`, `TP-Link_9B3D`, `Xfinity3AC1` |
| ISP prefix + 2 decimal | `ATT47`, `Spectrum09` |
| Name's Device | `Sarah's iPhone 14`, `James's Galaxy S23` |
| Static well-known | `xfinitywifi`, `eduroam`, `Boingo Hotspot`, `linksys` |
| HP printer | `HP-Print-3F-LaserJet`, `HP-Print-A0-DeskJet` |
| Wi-Fi Direct (lowercase hex) | `DIRECT-a1-Pixel 7`, `DIRECT-3f-iPad` |
| Android hotspot | `AndroidAP_5821` |
| Redmi | `Redmi Note 9 Pro`, `Redmi Note 3` |
| Setup | `Setup3B9C`, `Setup1F4A` |
| ASUS | `ASUS_9F1C`, `ASUS_4D2B` |

All beacons land on **2.4 GHz channel 6**.

##### BSSIDs

Real vendor OUI prefixes (first 3 bytes), random host (last 3 bytes).  
All are globally-administered unicast (bit0=bit1=0 on byte 0).

Key OUI ranges:

| Vendor | OUI examples |
|---|---|
| Apple | `00:03:93`, `3C:07:54`, `F0:DB:E2` |
| Samsung | `24:4B:03`, `A4:07:B6`, `CC:07:AB` |
| Cisco | `00:00:0C`, `00:18:18` |
| TP-Link | `50:C7:BF`, `84:D8:1B`, `EC:26:CA` |
| Netgear | `00:09:5B`, `28:C6:8E`, `D8:EB:97` |
| Espressif | `24:6F:28`, `30:AE:A4`, `EC:64:C9` |
| Google/Nest | `00:1A:11`, `54:60:09`, `F4:F5:D8` |
| Amazon/Eero | `28:EF:01`, `CC:F7:35` |
| Ubiquiti | `04:18:D6`, `70:A7:41`, `88:DC:96` |
| Aruba | `00:0B:86`, `94:B4:0F` |

##### BLE advertisement names

Pools (with rough frequency):
- **Earbuds (~25%):** `AirPods`, `AirPods Pro`, `Galaxy Buds2`, `Jabra Elite 7 Pro`, `Sony WF-1000XM5`, `Beats Fit Pro`, …
- **Wearables (~17%):** `Fitbit Charge 6`, `Galaxy Watch 6`, `Apple Watch`, `Garmin Venu 3`, …
- **Speakers (~17%):** `JBL Flip 6`, `Bose SoundLink Flex`, `Sony SRS-XB33`, `UE BOOM 3`, …
- **Trackers (~17%):** `Tile`, `Tile Pro`, `AirTag`, `SmartTag2`, `Chipolo ONE`, …
- **Phones (~8%):** `iPhone`, `Pixel 8`, `Samsung Galaxy`, …
- **Possessive (~17%):** `Sarah's AirPods`, `James's Apple Watch`, `Emma's Fitbit`, …
- **~30% have a suffix:** `AirPods Pro (42)`, `JBL Flip 6 [37]`, `Tile-83`

##### BLE MACs

Random static — `out[0] = (byte | 0xC2) & 0xFE` — so byte 0 always has bits 7, 6, 1 set; bit 0 clear.  
In hex: byte 0 looks like `C2`, `C6`, `CA`, `CE`, `D2`, … (0b11xxxxxx, even).

##### Sub-GHz

**433.92 MHz**, OOK 270 kHz, random byte payload (up to 60 bytes).  
No structured framing — appears as noise bursts on 433.92 MHz.

#### Baseline Wi-Fi snapshot (captured 2026-07-18 ~16:54 local)

7 SSIDs visible before chaff started:

| SSID | Channels seen |
|---|---|
| The WLAN Before Time | 1, 5 GHz (149, 100) |
| GuestNET | 1, 5 GHz (149, 100) |
| legit_IoS | 1, 5 GHz (149, 100) |
| TrustedGuest | 1, 5 GHz (149, 100) |
| *(hidden/empty)* | 5 GHz (36, 149), 1 |
| ATTUSKSp5S | 5 (2.4 GHz) |
| williamswifi | **6** (2.4 GHz) |

Note: `williamswifi` is already on ch6 — it's a real neighbour, not chaff.  
Any **new** SSID appearing on ch6 after chaff starts is a FluckFlock candidate.

#### Validation Playbook

##### Step 0 — before hitting "Start Chaff"

Baseline is already captured above. If you need a fresh one:
```powershell
netsh wlan show networks mode=bssid
```

##### Step 1 — Wi-Fi validation (most important — new hardware path)

**Quick check (Windows, no extra gear):**

```powershell
$b = @("The WLAN Before Time","GuestNET","legit_IoS","TrustedGuest","","ATTUSKSp5S","williamswifi")
(netsh wlan show networks mode=bssid) -match "^SSID \d+ :" |
  ForEach-Object { $s = ($_ -split " : ",2)[1].Trim(); if($b -notcontains $s){ "NEW: $s" } }
```

Run this **after chaff has been running for at least 10 seconds**.  

You should see output like:
```
NEW: NETGEAR4F2A
NEW: Sarah's iPhone 14
NEW: AndroidAP_5821
```

**What to look for:** New SSIDs matching any FluckFlock template, BSSIDs whose first 3 bytes match the vendor OUI list above, all on channel 6.

**Caveats:**
- Windows caches scans ~30–60 s. If no new SSIDs appear, disable/re-enable Wi-Fi to force a fresh scan, then re-run.
- FluckFlock sends ONE beacon per `FFB` command and immediately rotates to the next SSID. Each individual beacon may not linger long enough for Windows to pick up — this is expected behaviour. The chaff is working even if you only catch 1–2 new SSIDs.
- A phone Wi-Fi scanner (e.g. **WiFi Analyzer** on Android, **Network Analyzer** on iOS) or a wardriving app refreshes faster than Windows and will catch more.

**Gold standard (if you have a monitor-mode adapter):**

```bash
# Linux with a mon-capable adapter (mon0)
sudo tshark -i mon0 -f "type mgt subtype beacon" \
  -Y "wlan.ds.current_channel==6" \
  -T fields -e wlan.ssid -e wlan.ta
```

Or in Wireshark: filter `wlan.fc.type_subtype == 0x08 && wlan_mgt.fixed.ds_channel == 6`.

You'll see a stream of rotating SSIDs from the FluckFlock OUIs above.

##### Step 2 — BLE validation

**Phone app (recommended — no extra gear):**
1. Open **nRF Connect** (Android/iOS) or **LightBlue** (iOS).
2. Go to the Scanner tab; set "Show unnamed devices" on.
3. While chaff runs, you should see a rapidly cycling list of devices with names matching the BLE name pools above — earbuds, wearables, speakers, trackers — and with MACs where byte 0 is 0bC2/C6/CA/CE/D2/D6/DA/DE/E2… (high two bits = 1, bit 0 = 0).

**Windows note:** PowerShell BLE scanning (`Get-PnpDevice` / WinRT BLE APIs) is throttled and unreliable for passive scanning. Use a phone app.

##### Step 3 — Sub-GHz validation (optional / best-effort)

Hardest to validate without a second receiver.

- **Frequency:** 433.92 MHz  
- **Modulation:** OOK 270 kHz  

To confirm emission, put a **second Flipper Zero** in `Sub-GHz → Read RAW` mode on 433.92 MHz (AM650 preset is close enough to catch OOK). You should see activity bursts synchronised with the Sub-GHz counter on the chaff Flipper's display.

A software-defined radio (RTL-SDR, HackRF) tuned to 433.92 MHz with ~500 kHz bandwidth in a waterfall view will also show burst activity.

#### Quick-reference cheat sheet

| Radio | Frequency/Channel | Look for | Tool |
|---|---|---|---|
| Wi-Fi | ch 6 (2.437 GHz) | New SSIDs from templates + known vendor OUIs | netsh diff one-liner, phone Wi-Fi scanner |
| BLE | 2.4 GHz (37/38/39) | Rotating advert names from pools, MAC byte0 ≥ 0xC2 even | nRF Connect / LightBlue |
| Sub-GHz | 433.92 MHz | OOK burst activity | 2nd Flipper RAW read or RTL-SDR waterfall |

---

### 2026-07-18: Wi-Fi Dev Board OTG Power Sequencing

**Author:** McManus  
**Date:** 2026-07-18  
**Status:** Accepted  
**Files affected:** `flipperzero/fluckflock/radio/radio_wifi.h`, `flipperzero/fluckflock/radio/radio_wifi.c`

#### Problem

The FluckFlock FAP was calling `radio_wifi_detect()` at startup with the Wi-Fi Dev Board unpowered. The official Flipper Wi-Fi Dev Board (ESP32-S2) is powered from the Flipper's 5V OTG rail, which is **disabled by default**. Without power, the ESP32 never responds to the `FF?\n` handshake, so `wifi_detected` stayed false and the Wi-Fi toggle never appeared in Settings.

#### Decision

**1. OTG power must be app-lifetime managed by the radio layer**

Two new public functions added to `radio_wifi.h`:
```c
void radio_wifi_power_on(void);   // enable 5V OTG + wait for ESP32 boot
void radio_wifi_power_off(void);  // disable 5V OTG on teardown
```

The Flipper's 5V OTG rail is the only power source for the Wi-Fi Dev Board. Enabling it is the radio layer's responsibility (not the app scene or the UART detect logic) because the correct call order is: `power_on → detect → init → [emit loop] → deinit → power_off`. Keeping the rail enabled for the app's full lifetime avoids ESP32 reboot mid-session. Power functions are idempotent; calling them out of order is safe.

**2. Boot settle time: `WIFI_BOOT_SETTLE_MS = 1500 ms`**

The ESP32-S2 Arduino build performs ROM bootloader, Arduino `setup()` with two `Serial.begin()` calls (USB-CDC + UART), and several `Serial.print()` debug lines before entering the `loop()`. Measured cold-start to UART-ready time is ~800–1000 ms. We use **1500 ms** to absorb cold-start jitter, Arduino framework variation, and USB-CDC enumeration overhead. The delay is only applied when OTG transitions from OFF→ON. If `radio_wifi_power_on()` is called when OTG is already enabled, the delay is skipped — the board is already running.

**3. Detect retry hardening**

The original single-shot `FF?\n` → `FF!v1\n` handshake (400 ms timeout) was fragile because the first bytes arriving at the Flipper UART after ESP32 boot can be lost to FIFO setup latency, and the ESP32 UART FIFO may contain leftover bytes from boot-time `Serial.print()` output. Hardening applied:

| Parameter | Value | Rationale |
|---|---|---|
| Pre-flush `\n` | sent once before first attempt | clears ESP32 rx FIFO of boot noise |
| Post-flush pause | 20 ms | gives ESP32 time to parse/discard the NL |
| `WIFI_DETECT_ATTEMPTS` | 3 | catches first-byte-loss on UART settle |
| `WIFI_DETECT_RETRY_MS` | 150 ms | short gap, doesn't penalise present board |
| Per-attempt timeout | 400 ms (unchanged) | matches original handshake spec |

Worst-case time when board is **absent**: 3 × 400 ms + 2 × 150 ms + 20 ms ≈ **1520 ms**. Typical time when board is **present**: one attempt succeeds in ≪ 400 ms.

#### Invariants

- `radio_wifi_power_on()` MUST be called (and return) before `radio_wifi_detect()`.
- `radio_wifi_power_off()` MUST be called on app teardown (after `radio_wifi_deinit()`).
- OTG rail state is queried with `furi_hal_power_is_otg_enabled()` before every enable/disable to prevent double-toggle.
- Wire protocol (FF? / FF! / FFON / FFOFF / FFB), baud (115200), and pins are unchanged.
- ESP32 companion firmware is NOT modified by this change.

---

### 2026-07-18: Wi-Fi Dev Board OTG Power Lifecycle in FluckFlock FAP

**Date:** 2026-07-18  
**Author:** Fenster  
**Status:** Implemented

#### Context

The Flipper Wi-Fi Dev Board (ESP32-S2) draws from the Flipper's 5V OTG pin, which is **off by default**. `fluckflock_app_alloc()` was calling `radio_wifi_detect()` while the board was unpowered, so the UART handshake always failed, `wifi_detected` stayed false, and the Wi-Fi toggle never appeared in Settings.

McManus added `radio_wifi_power_on()` / `radio_wifi_power_off()` to the `radio_wifi` interface (see decision 2026-07-18: Wi-Fi Dev Board OTG Power Sequencing above).

#### Decision

**Power on:** Call `radio_wifi_power_on()` in `fluckflock_app_alloc()`, immediately before `radio_wifi_detect()`. The board stays powered for the **entire app session** — it must remain on during Start Chaff so injection works.

**Power off:** Call `radio_wifi_power_off()` in `fluckflock_app_free()`, unconditionally, after `chaff_engine_free()`. Both functions are idempotent; no conditional guard is needed. `app_free` is the single FAP teardown point, so one call there covers every exit path.

#### Rationale

- Powering off in `app_free` (not after detect) avoids adding a second ownership site and keeps the 5V rail live for the chaff session.
- Unconditional call in `app_free` is safe because `radio_wifi_power_off()` is idempotent — if the board was never detected, turning off a rail that was never turned on is a no-op.
- Mirrors standard Flipper resource-lifecycle convention: acquire in alloc, release in free.

#### Files Changed

- `flipperzero/fluckflock/fluckflock.c` — `fluckflock_app_alloc()` and `fluckflock_app_free()`

---

### 2026-07-18: Wi-Fi Beacon Persistence / Dwell Strategy

**By:** Keaton (Lead / Firmware Architect)
**Date:** 2026-07-18
**Status:** PROPOSED — awaiting user approval

#### Problem

`chaff_engine_step()` emits exactly one Wi-Fi beacon per timer interval with a fresh random SSID+BSSID each step. Real APs beacon at ~10 Hz on a stable SSID/BSSID. Wi-Fi scanners (Windows, phones) require several consecutive beacons from the same SSID/BSSID before listing an AP. Result: almost no fake SSIDs appear in scan lists.

#### Spec Reconciliation

Spec §7 says "ever-changing" and "no identifier should repeat within a rolling window of at least 1000 emitted identifiers per radio." A **bounded dwell** (same SSID/BSSID repeated for ~1–2 seconds at 100 ms beacon cadence = 10–20 repetitions, then rotate) does NOT violate the spec:

- The 1000-window non-repeat rule applies to *distinct identifiers*, not individual frames. A dwell window reuses the *same* identifier briefly, then it is retired permanently. At 2s dwell / 1s interval between rotations, the system still produces ~30+ unique SSIDs/min — thousands/hour, all non-repeating after their dwell expires.
- "Ever-changing" describes the *population over time*, not per-frame uniqueness. Real-world beacon floods (the surveillance countermeasure this is modeled on) all use dwell; single-shot beacons are invisible and therefore zero-value chaff.
- **Invisible chaff is useless chaff.** The spec's purpose (§1, §4) is to *pollute scan results*. If beacons never appear in scan lists, they fail the stated goal entirely.

**Conclusion:** Bounded dwell is spec-compatible and in fact *required* to meet the spec's stated goal.

#### Recommended Approach: B (ESP32 Autonomous Beaconing) — with pool extension toward C

##### Why B over A

| Factor | A (Flipper dwell loop) | B (ESP autonomous) |
|---|---|---|
| Beacon timing accuracy | ~100ms jitter from UART round-trips | Hardware timer, sub-ms jitter — looks like a real AP |
| UART load | 10–20x increase per SSID | Same as today: one FFB command per rotation |
| Engine complexity | Needs Wi-Fi sub-cadence / sub-timer in chaff_engine | No change to engine timer model |
| Realism | Bursty (gaps between UART sends) | Continuous, indistinguishable from real AP |
| ESP firmware change | None | Yes — ESP loops on last-received beacon until next FFB |

**B wins on every axis.** The only cost is an ESP firmware change, which is minor: the ESP `loop()` adds a millis()-based re-transmit of the last beacon frame every ~100ms while injection is enabled. The UART protocol is unchanged (FFB still means "set current beacon"); semantics shift from "inject once" to "set active beacon."

##### Pool Extension (toward C, future)

Once B is working, extending to a pool of N simultaneous SSIDs (C) is a natural next step: the ESP holds N beacon entries and round-robins through them at 100ms intervals. This produces the "busy coffee shop" effect. However, **B alone is sufficient to make Wi-Fi chaff visible** and should ship first. Pool can be a follow-on if the user wants it.

#### Setting vs. Default

**Changed default, no new setting.** The ESP simply re-beacons continuously; the Flipper side is unaware. The existing Interval setting controls how often the SSID/BSSID *rotates* (new identity), which is the user-facing knob. No new UI needed.

If pool mode (C) is later added, a "Wi-Fi Density" setting (1 / 4 / 8 simultaneous SSIDs) could be exposed, but that's out of scope for this decision.

#### Implementation Checklist

##### McManus (radio / ESP RF owner)

1. **ESP32 `main.cpp`:** After `cmd_beacon()` stores the built frame, add a `millis()`-based loop in `loop()` that re-transmits the stored frame every ~102ms (matching the beacon interval field in the frame) while `injection_enabled && has_active_beacon`. Clear `has_active_beacon` on `FFOFF`.
2. **Beacon interval constant:** Use 102ms (1 TU ≈ 1.024ms × 100 TU = 102.4ms) to match the frame's declared beacon interval field. McManus to confirm exact value from his RF analysis.
3. **No UART protocol change.** `FFB` semantics shift from "inject once" to "set active beacon" — this is an ESP-internal behavior change, transparent to the Flipper side.

##### Fenster (engine / app / UI wiring)

1. **No engine changes required.** `chaff_engine_step()` continues sending one `FFB` per interval. The ESP handles repetition autonomously.
2. **No UI changes required.** Existing Interval setting controls rotation cadence, which is the correct user-facing control.

##### Keaton (spec / task / review)

1. **spec.md §5.3 update (after user approval):** Add a sentence: "The companion firmware continuously re-transmits the most recent beacon at the standard 802.11 beacon interval (~100 TU) until the next `FFB` command replaces it or `FFOFF` is received."
2. **task-flipper.md update (after user approval):** Add checklist item under Wi-Fi section: `[ ] ESP32 autonomous beacon re-transmission — companion re-beacons last FFB at ~100ms until replaced or FFOFF`.
3. **Review gate:** Review McManus's ESP32 changes before merge.

#### Risks

- **Regulatory:** No change — we're already injecting beacons; this just increases the rate to match what real APs do. Frame content is identical.
- **ESP32 CPU/heat:** Beacon TX at 10 Hz is trivial for the ESP32 (real APs do this 24/7). No concern.
- **Stale beacon after Flipper crash:** If the Flipper crashes without sending FFOFF, the ESP continues beaconing the last SSID indefinitely. Acceptable — the ESP has no watchdog for the Flipper link today either. A future enhancement could add a UART idle timeout on the ESP side.

#### Decision

**Recommend approach B. Ship as changed ESP default. No new Flipper-side settings. Spec + task-flipper.md updates pending user approval.**

---

### 2026-07-18: Wi-Fi Beacon Visibility: Options & Recommended Parameters

**Author:** McManus (Wireless / RF Dev)  
**Date:** 2026-07-18  
**Status:** PROPOSED — awaiting user approval

#### Problem

`chaff_engine_step()` generates one SSID+BSSID and calls `radio_wifi_emit()` once per timer tick.
`radio_wifi_emit()` sends one `FFB` command. `cmd_beacon` on the ESP32 calls `esp_wifi_80211_tx()`
once and returns. Result: single-shot rotating beacons that OS Wi-Fi scanners silently drop.

**Why scanners miss them:** Windows/Android/iOS Wi-Fi scan daemons require multiple beacon
frames from the same BSSID within their scan window (~2–4 s) before surfacing an AP in results.
Real APs beacon at 100 TU ≈ 102.4 ms (≈10×/sec). One beacon is effectively invisible.

#### UART Throughput Math (115200 baud baseline)

```
115200 baud, 8N1  →  10 bits/byte  →  11 520 bytes/sec effective

FFB command size:
  "FFB " (4) + 12 hex BSSID (12) + " " (1) + SSID + "\n" (1)
  Min (1-char SSID): 19 bytes
  Avg (~14-char SSID): 32 bytes
  Max (32-char SSID):  50 bytes

Rates at 32-byte average:
  10 FFB/s  →  320 bytes/s  =  2.8% of UART capacity   ✓
  20 FFB/s  →  640 bytes/s  =  5.6%                     ✓
  50 FFB/s  →  1 600 bytes/s = 13.9%                    ✓

TX time per frame (32 bytes at 115200):
  32 × 10 / 115200 ≈ 2.8 ms  — trivial; no UART bottleneck at these rates.
```

115200 baud is **not a constraint** for any option below.

#### 2.4 GHz Airtime

Each beacon (~119 bytes frame at 1 Mbps DSSS basic rate) ≈ 1 ms airtime.

| Rate          | Airtime/sec | Channel utilization |
|---|---|---|
| 10 beacons/s  | ~10 ms      | ~1%                 |
| 80 beacons/s  | ~80 ms      | ~8%                 |

No congestion concern at any option's rates.

#### Options

##### Option A — Flipper-Driven Dwell (no ESP firmware change)

**Concept:** Flipper holds one SSID+BSSID and re-sends `FFB` every 100 ms for a configurable
dwell window, then rotates to a new SSID+BSSID.

**Mechanism change:** `chaff_engine` timer fires at **100 ms**. Engine tracks a `wifi_dwell_remaining`
counter. When > 0: re-emit same SSID+BSSID, decrement. When 0: generate fresh SSID+BSSID, reset
counter to `dwell_ticks` (= `dwell_ms / 100`).

**Recommended default parameters:**

| Parameter          | Value           | Range (user-tunable)    |
|---|---|---|
| Beacon spacing     | 100 ms          | fixed (1 TU standard)   |
| Dwell window       | **1 500 ms**    | 500 ms – 5 000 ms       |
| Beacons per SSID   | **15**          | 5 – 50                  |
| Distinct SSIDs/min | **40**          | 12 – 120                |
| UART bytes/sec     | 320 (avg)       | 2.8% of 115200           |

*At 1 500 ms dwell:* scanner sees AP appear → 15 beacons → AP ages out; clear "real AP" pattern.  
*At 500 ms (aggressive):* 120 SSIDs/min, 5 beacons/SSID — borderline visibility on slow scanners.  
*At 5 000 ms (max):* 12 SSIDs/min, 50 beacons — very visible, low diversity.

**ESP firmware change needed:** ❌ None  
**Realism:** ✅ Excellent — 100 ms spacing is exactly 100 TU standard AP behaviour  
**Chaff volume:** ✅ Good — 40 distinct SSIDs/min at default  
**Implementation effort:** Low — add `wifi_dwell_remaining` counter to `ChaffEngine` struct, adjust timer period

##### Option B — ESP Autonomous Beaconing

**Concept:** One `FFB` command sets the "current beacon" on the ESP. The ESP firmware re-transmits
the frame every 100 ms autonomously, until a new `FFB` replaces it.

**ESP firmware change shape (main.cpp):**
```cpp
// New globals:
static uint8_t last_frame[128];
static size_t  last_frame_len = 0;
static uint32_t last_beacon_ms = 0;

// In loop(), add:
if(injection_enabled && last_frame_len > 0) {
    uint32_t now = millis();
    if(now - last_beacon_ms >= 100) {
        esp_wifi_80211_tx(WIFI_IF_STA, last_frame, (int)last_frame_len, false);
        last_beacon_ms = now;
    }
}

// In cmd_beacon(): build frame into last_frame, set last_frame_len, last_beacon_ms = millis()
// FFOFF already stops injection, which halts the re-tx loop (injection_enabled = false)
```

**Protocol semantics change:** `FFB` becomes "set current beacon" (idempotent update);
a new `FFB` replaces the current beacon seamlessly. No new command needed.

**Recommended parameters:**

| Parameter          | Value           | Notes                         |
|---|---|---|
| ESP beacon interval| **100 ms**      | hard-coded in ESP loop        |
| Flipper rotation   | **1 500 ms**    | Flipper sends new FFB every N |
| Beacons per SSID   | ~15             | 1 500 ms / 100 ms             |
| Distinct SSIDs/min | **40**          | same as Option A              |
| UART bytes/sec     | **~21** (avg)   | 1 FFB per 1.5 s = 0.2% UART  |

**ESP firmware change needed:** ✅ Yes (small, ~20 lines) — but ESP has uncommitted changes pending Keaton review  
**Realism:** ✅✅ Best — AP beacons with hardware timer precision  
**Chaff volume:** ✅ Same diversity as A at same rotation rate  
**UART load:** ✅✅ Lowest possible (~0.2% vs 2.8%)  
**Implementation effort:** Medium — needs ESP firmware edit + Keaton review gate

##### Option C — ESP Pool of N Beacons

**Concept:** The ESP maintains a ring buffer of N SSID+BSSID entries. It round-robins beacon
transmission at 100 ms per slot, so all N APs are visible concurrently. Flipper slowly
refreshes pool entries with new SSIDs.

**Recommended N: 8**

| Parameter              | N=4 variant  | **N=8 (recommended)**  | N=16 variant   |
|---|---|---|---|
| Per-AP beacon interval | 400 ms       | **800 ms**             | 1 600 ms       |
| Concurrent visible APs | 4            | **8**                  | 16             |
| Visibility margin      | ✅ Good       | ✅ OK (marginal)*       | ⚠️ Borderline   |
| Distinct SSIDs/min     | 60           | **60**                 | 60             |
| Flipper refresh rate   | 1/sec        | **1/sec**              | 1/sec          |
| UART bytes/sec         | ~32 (avg)    | **~32**                | ~32            |

\* Windows scan window ~2–4 s: at 800 ms/beacon, scanner catches 2.5–5 beacons per AP per window
— sufficient for most OS scanners to list the AP.

**New protocol commands needed:**
```
FFBP <slot> <bssid12> <ssid>\n   — set pool slot (0..N-1)
FFBPC\n                           — clear pool (stop all)
```
Or reuse `FFB` with sequential BSSIDs (ESP auto-assigns slot by BSSID).

**ESP firmware change needed:** ✅ Yes (significant — pool array, slot management, ~60 lines)  
**Realism:** ✅ Good for N≤8; each BSSID beacons at 800 ms vs real 100 ms — slightly slow but convincing  
**Chaff volume:** ✅✅ Best — N APs always visible + high rotation  
**Implementation effort:** High — needs new protocol commands + pool data structure

#### Comparison Table

| Option | Beacon spacing | Distinct SSIDs/min | Concurrent APs | Realism        | UART bytes/sec | ESP change? | Effort |
|---|---|---|---|---|---|---|---|
| **A — Flipper Dwell** | 100 ms per SSID | **40** | 1 at a time | ✅✅ Excellent | 320 | ❌ None | Low |
| **B — ESP Autonomous** | 100 ms (HW timer) | **40** | 1 at a time | ✅✅✅ Best | 21 | ✅ ~20 lines | Medium |
| **C — ESP Pool N=8** | 800 ms per AP | **60** | **8** | ✅ Good | 32 | ✅ ~60 lines | High |
| *Current (broken)* | one-shot | 60 (0 visible) | 0 | ❌ Invisible | 32 | — | — |

#### Recommended Default: **Option A** (ship now) → **Option B** (after Keaton ESP review)

##### Why Option A first

- **Zero ESP firmware risk** — works with the ESP as-is, no dependency on Keaton's pending review.
- **Near-perfect realism** — 100 ms beacon spacing is exactly 100 TU real AP behaviour.
- **40 SSIDs/min** is high chaff diversity; well above practical tracking utility.
- **Simple change:** add one `uint8_t wifi_dwell_remaining` counter + stored SSID/BSSID to `ChaffEngine`, change timer period to 100 ms.
- **UART load is trivially low** (2.8% of 115200).

##### Option A exact parameters to ship

```
Timer period:      100 ms  (hard)
Default dwell:   1 500 ms  (15 beacons per SSID)
Min dwell:         500 ms  (5 beacons — aggressive chaff, may miss slow scanners)
Max dwell:       5 000 ms  (50 beacons — very visible, low diversity)
User step:         500 ms  increments in UI
```

**Chaff diversity at defaults:** 60 s / 1.5 s = **40 distinct SSIDs/min**  
**Visibility expectation:** Windows netsh, Android, iOS all show faked APs reliably.  
**Gold standard (Wireshark/tshark):** All beacons visible immediately at 100 ms intervals.

##### Why migrate to Option B later

Once ESP firmware changes are unblocked (Keaton review), Option B replaces A:
- Achieves the same 40 SSIDs/min at 1/100th the UART bandwidth
- Beacon interval is driven by ESP hardware (more accurate, immune to Flipper timer jitter)
- Flipper code simplifies: remove dwell counter, just send `FFB` on rotation

##### Option C: future enhancement

After B: add pool support for "density mode" (user selects N=4/8 → N fake APs always visible).
Good for demo scenarios. Not needed for core chaff.

#### BLE Note (brief)

BLE advert scanners behave differently from Wi-Fi scanners. BLE scan duty cycles (window/interval)
typically catch a single advertisement within 1–3 advert intervals (default 100 ms advert interval
→ caught within 300 ms). Single-shot BLE adverts ARE generally caught by iOS/Android, unlike Wi-Fi.
**BLE does not need the dwell fix.** Current single-shot BLE behaviour is acceptable for chaff.
If BLE visibility becomes a concern later, reduce advert interval to 50–75 ms.

---

## Governance

- All meaningful changes require team consensus
- Document architectural decisions here
- Keep history focused on work, decisions focused on direction
