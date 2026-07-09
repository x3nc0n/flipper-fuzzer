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

## Governance

- All meaningful changes require team consensus
- Document architectural decisions here
- Keep history focused on work, decisions focused on direction
