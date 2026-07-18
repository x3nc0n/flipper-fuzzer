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

## Governance

- All meaningful changes require team consensus
- Document architectural decisions here
- Keep history focused on work, decisions focused on direction
