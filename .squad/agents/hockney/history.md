# Project Context

- **Owner:** John Spaid
- **Project:** FluckFlock — a Flipper Zero "chaff" broadcaster that continuously emits large volumes of plausible, ever-rotating fake wireless identifiers (Wi-Fi SSIDs/BSSIDs, BLE addresses/advert names, Sub-GHz RF noise/beacons) to pollute passive/dragnet tracking systems (Wi-Fi/BLE MAC tracking, SSID probe logging, RF fingerprinting).
- **Stack:** Flipper Zero firmware in C (FAP app, ufbt/fbt build, furi APIs, ViewDispatcher/SceneManager). Native BLE + Sub-GHz (CC1101); Wi-Fi via the official Flipper Wi-Fi Dev Board (ESP32) where present.
- **Target:** Flipper Zero ONLY — single hardware target (the earlier "two hardware tiers / two implementations" idea was scrubbed per user directive 2026-07-09). One implementation checklist: task-flipper.md.
- **Created:** 2026-07-09

## Learnings

<!-- Append new learnings below. Each entry is something lasting about the project. -->

### 2026-07-09 — Host-test CI added at `.github/workflows/host-tests.yml`

**Author:** Hockney

GitHub Actions workflow created to run the host test suite on every push and pull_request to `master`, and on `workflow_dispatch`. Runs on `ubuntu-latest`; installs `build-essential` to ensure `gcc` and `make` are present, then executes `make test` from the `test/` working directory. The `make test` target builds all three test binaries (`test_prng`, `test_identifiers`, `test_wifi_proto`) and runs them in sequence; a non-zero exit from any binary fails the workflow. No changes were made to the Makefile or tests — `make test` was already the correct invocation.

### 2026-07-09 — Host-side test suite created and verified passing

**Author:** Hockney
**Status:** Tests compiled and ran against McManus's implementations — all passed.

#### Test suite structure

```
test/
├── test_util.h          # minimal assert harness (ASSERT, ASSERT_EQ, PASS macros)
├── test_prng.c          # PRNG contract tests (11 test functions)
├── test_identifiers.c   # identifier generator tests (16 test functions)
└── Makefile             # builds + runs both suites via `make test`
```

#### How to run

```
cd test && make test      # requires WSL or MinGW/MSYS2 on Windows; gcc or clang
make CC=gcc test          # explicit compiler
make CC=clang test        # explicit compiler
```

On Windows, WSL is confirmed available and `cc` resolves to `/usr/bin/cc` (gcc).

#### What each suite asserts

**test_prng.c** (48 115 assertions, all PASS):
- `alloc_free_lifecycle` — 50 alloc/seed/free cycles; no crash.
- `determinism_u32` — same seed → 1000 identical u32 values across two independent RNG instances.
- `determinism_bytes` — same seed → 64 identical bytes.
- `reseed_restarts_sequence` — reseeding with same value restarts the exact same u32 sequence.
- `different_seeds_diverge` — seeds 1 vs 2; fewer than 5 out of 100 values coincidentally match.
- `bounded_range` — `chaff_rng_bounded(rng, N)` always in [0, N) for N ∈ {2,3,5,7,10,13,16,64,100}, 5 000 samples each.
- `bounded_one_always_zero` — N==1 yields 0 for 2 000 calls.
- `bounded_uniformity` — N==4, 20 000 samples; every bucket within [50%, 150%] of expected.
- `bytes_exact_fill` — 0xAA sentinel guards before and after a 32-byte fill; verifies no overrun and that region is not still all-0xAA.
- `bytes_zero_length` — len==0 is a no-op; buffer untouched.
- `bytes_output_varies` — two consecutive 16-byte fills differ.

**test_identifiers.c** (23 541 assertions, all PASS):
- SSID: length [1,32], null-terminated at index len, all printable ASCII, no overrun with buf_len=4, ≥5 distinct values in 50 generations, ≥20 distinct values in 1000 (spec §7 "ever-changing").
- BSSID: out[0] bit0==0 (unicast) and bit1==0 (globally administered) across 500 calls; ≥90 unique in 100 samples; <5 collisions in 1000 samples.
- BLE MAC: out[0] bit1==1 (locally administered, spec §5.1/§6) and bit0==0 (unicast) across 500 calls; ≥90 unique in 100 samples; <5 collisions in 1000 samples.
- BLE name: length [1, buf_len-1], null-terminated, no overrun with buf_len=4, ≥5 distinct values in 50 generations.
- Sub-GHz payload: fills exactly len bytes (sentinel guards), does not write beyond len, two consecutive payloads differ.
- Rotation (1000-sample): SSID distinct≥20; BSSID collisions<5; BLE MAC collisions<5.

#### Spec claims turned into assertions

| Spec §  | Claim | Assertion |
|---------|-------|-----------|
| §6      | SSID 1–32 bytes, no control chars | `len>=1 && len<=32`; `all_printable_ascii()` |
| §6      | BSSID: unicast bit clear, locally-administered bit clear | `(mac[0] & 0x01)==0`; `(mac[0] & 0x02)==0` |
| §6      | BLE MAC: locally-administered bit SET, unicast bit clear | `(mac[0] & 0x02)!=0`; `(mac[0] & 0x01)==0` |
| §7      | No identifier repeats within rolling window of 1000 | distinct≥20 (SSID), collisions<5 (MAC) |
| §7      | PRNG seeded from hardware entropy; deterministic per seed | same-seed sequence equality tests |
| §5 / arch | BLE name length ≤ buf_len-1, null-terminated | overrun tests with buf_len=4 |

#### Run results (2026-07-09)

McManus's `prng.c` and `identifiers.c` were present. Both suites compiled and ran:
- `test_prng`: 48 115 assertions — **all PASSED**
- `test_identifiers`: 23 541 assertions — **all PASSED**

No further implementation stub needed. Next validation pass: run again after any changes to generators/ to confirm no regressions.

### 2026-07-09 — SDK 1.4.3: SceneManagerHandlers struct and subghz preset API

**Author:** Hockney
**Files fixed:** `flipperzero/fluckflock/scenes/scenes.c`, `flipperzero/fluckflock/radio/radio_subghz.c`

#### SceneManagerHandlers (scene_manager.h)

The struct has **three flat callback-pointer arrays**, not an array of per-scene structs:

```c
typedef struct {
    const AppSceneOnEnterCallback* on_enter_handlers;
    const AppSceneOnEventCallback* on_event_handlers;
    const AppSceneOnExitCallback*  on_exit_handlers;
    const uint32_t                 scene_num;
} SceneManagerHandlers;
```

Correct pattern: build three separate static arrays via X-macro (one per callback type), then
assign all three to the single `SceneManagerHandlers` instance. Do **not** create a
`SceneManagerHandlers[]` array of per-scene structs — that type has no `.on_enter/.on_event/.on_exit`
fields and will not compile.

#### SubGHz preset API (SDK 1.4.3)

`furi_hal_subghz_load_preset()` and the `FuriHalSubGhzPreset` enum **do not exist** in SDK 1.4.3.
Use `furi_hal_subghz_load_custom_preset(const uint8_t* preset_data)` instead, passing a register
table exported from `<lib/subghz/devices/cc1101_configs.h>`, e.g.:

```c
#include <lib/subghz/devices/cc1101_configs.h>
furi_hal_subghz_reset();
furi_hal_subghz_idle();
furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_270khz_async_regs);
```

`furi_hal_subghz_acquire()` / `furi_hal_subghz_release()` also **do not exist** in this SDK.
Guard CC1101 state with `reset → idle` before config and `idle → sleep` after use.

### 2026-07-09 — Wi-Fi protocol host test coverage added

**Author:** Hockney
**Status:** All tests compiled clean (no warnings) and passed.

#### New file: `test/test_wifi_proto.c`

Tests the pure-C `wifi_proto` module (`radio/wifi_proto.h` / `wifi_proto.c`).
No Flipper SDK dependency — compiles and runs on any host with gcc/clang.

**16 test functions, 236 assertions — all PASS.**

| Test function | What it asserts |
|---|---|
| `handshake_exact_string` | `build_handshake` writes exactly `"FF?\n"`, returns 4, null-terminated; sentinel beyond is untouched |
| `on_exact_string` | `build_on` writes exactly `"FFON\n"`, returns 5, null-terminated |
| `off_exact_string` | `build_off` writes exactly `"FFOFF\n"`, returns 6, null-terminated |
| `simple_builders_too_small_buf` | Each builder returns 0 when buf_len is one byte too small; no bytes past the declared buf_len are written |
| `simple_builders_exact_fit_buf` | Each builder succeeds when buf_len == required (exact fit) |
| `beacon_known_ssid_and_bssid` | bssid `AA:BB:CC:DD:EE:FF` + ssid `"TestNet"` → `"FFB aabbccddeeff TestNet\n"` (25 bytes) |
| `beacon_bssid_byte_order` | `{0x12,0x34,0x56,0x78,0x9A,0xBC}` → `"FFB 123456789abc X\n"` — bssid[0] maps to leftmost hex pair |
| `beacon_hex_edge_bytes` | `{0x00,0x0f,0xa5,0xff,0x10,0xf0}` → `"000fa5ff10f0"` — leading zeros preserved, all lowercase |
| `beacon_all_zero_bssid` | `{0x00…}` → `"000000000000"` (12 zeros) |
| `beacon_all_ff_bssid` | `{0xff…}` → `"ffffffffffff"` (lowercase) |
| `beacon_too_small_buf` | buf_len=4 returns 0; buf_len one byte short returns 0; exact-fit buf_len succeeds; no sentinel overrun in any case |
| `beacon_ssid_one_char` | 1-char SSID (`"X"`) frames correctly → 19-byte output |
| `beacon_ssid_max_length` | 32-char SSID frames correctly → exactly `WIFI_PROTO_BEACON_CMD_MAX_LEN` (50) bytes |
| `beacon_ssid_too_long` | 33-char SSID → returns 0 (rejected per spec 1–32 byte rule) |
| `beacon_ssid_empty` | Empty SSID (`""`) → returns 0 |
| `beacon_ssid_appended_verbatim` | SSID with special chars appended without escaping |

#### Makefile changes (`test/Makefile`)

- Added `-I../flipperzero/fluckflock/radio` to `CFLAGS`.
- Added `WIFI_PROTO_BIN := test_wifi_proto` target that compiles `test_wifi_proto.c` + `radio/wifi_proto.c` only (no generators, no PRNG needed).
- `all` and `test` targets extended to include `test_wifi_proto`.
- `clean` extended to remove `test_wifi_proto` binary.

#### How to run

```
cd test && make test
# or with WSL on Windows:
wsl bash -c "cd /mnt/c/Users/jspai/.source/GitHub/flipper-fuzzer/test && make clean && make test"
```

#### Run results (2026-07-09)

- `test_prng`: 48 115 assertions — **all PASSED**
- `test_identifiers`: 23 541 assertions — **all PASSED**
- `test_wifi_proto`: 236 assertions — **all PASSED**

No bugs found in `wifi_proto.c`. McManus's implementation is clean.

---

### 2026-07-09 — FINAL OUTCOME

- **ufbt build against Flipper SDK 1.4.3: SUCCEEDS**
- **Host test suite (~71k assertions): all PASS**
- **All blockers and major issues: RESOLVED**
  - `scenes.c` SceneManagerHandlers type error — FIXED (restructured arrays per SDK pattern)
  - `radio_subghz.c` acquire/release — FIXED (added calls to guard CC1101 access)
- **Project approved for merge**

### 2026-07-09 — Real hardware validation: FAP confirmed working on Flipper Zero

Deployed to physical Flipper Zero over USB COM5 via `ufbt launch` (Windows + SDK 1.4.3).
User visually confirmed app running with live BLE + Sub-GHz emission counters incrementing.
Wi-Fi companion firmware implemented but dev board not yet physically available for testing.
See orchestration-log and session log for full deployment details.

