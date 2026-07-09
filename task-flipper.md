# FluckFlock — Flipper Zero Implementation Checklist

**Date:** 2026-07-09
**Target:** Flipper Zero (FAP, ufbt/fbt, C, furi APIs)

---

## App Scaffolding

- [ ] Create `flipperzero/fluckflock/application.fam` — FAP manifest (appid, name, entry point, icon, stack size, fap_category)
- [ ] Create `fluckflock.c` — app entry point (`fluckflock_app()`), allocate `App` struct, init ViewDispatcher + SceneManager, run event loop, free on exit
- [ ] Define `fluckflock.h` — main app struct (`App`) holding ViewDispatcher, SceneManager, chaff engine pointer, GUI handles, settings
- [ ] Create `scenes/` directory with scene handlers: `scene_main_menu.c`, `scene_running.c`, `scene_settings.c`, `scene_about.c`
- [ ] Register all scenes in a scene enum and on_enter/on_event/on_exit table
- [ ] Create `views/` directory with custom views if needed (e.g., `view_status.c` for live counters)

## Chaff Engine Core

- [ ] Create `chaff_engine.h` — public interface: init, start, stop, deinit, set_radio_enabled, get_stats, step/tick
- [ ] Create `chaff_engine.c` — implementation of lifecycle and scheduler
- [ ] Define `ChaffRadioType` enum: `ChaffRadioBLE`, `ChaffRadioSubGhz`, `ChaffRadioWifi`
- [ ] Implement `chaff_engine_alloc()` / `chaff_engine_free()` — allocate/free engine state
- [ ] Implement `chaff_engine_start()` — start a furi timer that calls `chaff_engine_step()` at the configured interval
- [ ] Implement `chaff_engine_stop()` — stop the timer, state → idle
- [ ] Implement `chaff_engine_step()` — for each enabled radio: generate new identifiers via generator, call radio-specific transmit, increment counters
- [ ] Implement `chaff_engine_set_radio_enabled(engine, radio, bool)` — per-radio enable/disable flag
- [ ] Implement `chaff_engine_get_stats(engine, radio)` → returns `ChaffStats` (count, last identifier emitted)
- [ ] Thread safety: engine step runs in timer callback context; protect shared state with furi mutex if UI reads stats concurrently

## Identifier Generators

- [ ] Create `generators/identifiers.h` — pure-function interface for all identifier generation
- [ ] Create `generators/identifiers.c` — implementations
- [ ] `chaff_generate_ssid(buf, buf_len, rng)` → writes a plausible SSID string into `buf`, returns length
- [ ] Build SSID template table: ≥ 30 realistic patterns (ISP defaults, café names, home routers with random suffix)
- [ ] `chaff_generate_bssid(out6, rng)` → writes 6-byte BSSID: OUI from table + 3 random bytes, unicast + globally-administered bits
- [ ] Build OUI table: ≥ 20 common AP vendor OUI prefixes (TP-Link, Netgear, Cisco, Ubiquiti, Aruba, Ruckus, etc.)
- [ ] `chaff_generate_ble_mac(out6, rng)` → writes 6-byte locally-administered random MAC (bit 1 set, bit 0 clear in first octet)
- [ ] `chaff_generate_ble_name(buf, buf_len, rng)` → writes a plausible BLE device name, returns length
- [ ] Build BLE name table: ≥ 30 realistic device names (earbuds, trackers, speakers, phones, smart home)
- [ ] `chaff_generate_subghz_payload(buf, buf_len, rng)` → fills buffer with pseudo-random bytes
- [ ] Implement seedable PRNG wrapper: `ChaffRng` struct, `chaff_rng_seed(rng, seed)`, `chaff_rng_u32(rng)`, `chaff_rng_bytes(rng, buf, len)` — backed by a fast PRNG (xoshiro256** or similar), seedable from `furi_hal_random` at runtime, from fixed seed in tests
- [ ] All generator functions are **pure** (no Flipper SDK dependency) — take an RNG, return data. Testable on host.

## BLE Transmit Path

- [ ] Create `radio/radio_ble.c` + `radio/radio_ble.h`
- [ ] `radio_ble_init()` — any one-time BLE stack setup
- [ ] `radio_ble_send(mac6, name, name_len)` — configure BLE advertising: set random address to `mac6`, build advertisement data (flags + local name + TX power), start advertising for one cycle, then stop
- [ ] `radio_ble_deinit()` — clean up, restore default advertising state
- [ ] Handle coexistence with Flipper's own BLE connection (document if exclusive access required)

## Sub-GHz Transmit Path

- [ ] Create `radio/radio_subghz.c` + `radio/radio_subghz.h`
- [ ] `radio_subghz_init()` — open Sub-GHz worker/hal
- [ ] `radio_subghz_send(frequency_hz, payload, payload_len)` — configure CC1101 to the given frequency, transmit OOK/2-FSK payload, return to idle
- [ ] Build frequency table: common ISM frequencies valid for the device's region (315, 433.92, 868, 915 MHz — filtered by `furi_hal_subghz` region check)
- [ ] `radio_subghz_deinit()` — close worker, release hardware
- [ ] Respect duty-cycle: if the platform exposes duty-cycle limits, honor them; otherwise use conservative TX-on time

## Optional Wi-Fi (Dev Board over UART)

- [x] Create `radio/radio_wifi.c` + `radio/radio_wifi.h` *(McManus, 2026-07-09)*
- [x] `radio_wifi_detect()` → bool — sends `FF?\n`, awaits `FF!` prefix within 400 ms timeout; acquires/inits/releases handle on normal path *(McManus, 2026-07-09)*
- [x] `radio_wifi_init()` — acquires UART handle, inits 115200 8N1, sends `FFON\n` to companion *(McManus, 2026-07-09)*
- [x] `radio_wifi_emit(ssid, bssid)` — fire-and-forget `FFB <bssid12hex> <ssid>\n` via UART *(McManus, 2026-07-09)* *(note: function was renamed from `radio_wifi_send` — drop the `ssid_len` param)*
- [x] `radio_wifi_deinit()` — sends `FFOFF\n`, calls furi_hal_serial_deinit then control_release *(McManus, 2026-07-09)*
- [x] Define UART line protocol — `wifi_proto.h` / `wifi_proto.c`: pure-C frame builders, all constants; see `mcmanus-wifi-protocol.md` *(McManus, 2026-07-09)*
- [x] ESP32 companion firmware created — `esp32/fluckflock_companion/` (PlatformIO / Arduino + ESP-IDF, ESP32-S2-WROVER-I, ch 6 default) *(McManus, 2026-07-09)*
- [ ] Host tests for `wifi_proto.c` frame builders — `test/` coverage of build_handshake / build_on / build_off / build_beacon *(Hockney — pending)*
- [ ] Wire Wi-Fi toggle into UI: visible only when `radio_wifi_detect()` returns true; hidden otherwise *(Fenster — pending, no UI integration yet)*
- [ ] On-device hardware validation with a real Flipper Wi-Fi Dev Board *(outstanding — can't verify without hardware)*
- [ ] Channel-hopping / dynamic channel selection — currently fixed at compile-time (`FLUCKFLOCK_CHANNEL=6`); a future enhancement *(outstanding)*
- [ ] Clean up stale "Currently stubbed to return false" doc comment in `radio_wifi.h` *(minor — McManus locked out; Fenster or Hockney can do as a cleanup pass)*

## UI / Menus

- [ ] **Main menu scene:** List items: "Start Chaff", "BLE [ON/OFF]", "Sub-GHz [ON/OFF]", "Wi-Fi [ON/OFF]" (conditional), "Settings", "About"
- [ ] **Running scene:** Show per-radio live counters (emitted count), elapsed time, "Stop" button
- [ ] **Settings scene:** Rotation interval slider (500ms – 5s), back button
- [ ] **About scene:** App name, version, disclaimer text (research/anti-tracking use, comply with local laws)
- [ ] Wire menu item callbacks to scene transitions
- [ ] Update counters in running scene via a timer or view model callback (≥ 1 Hz refresh)

## Persistence / Settings

- [ ] Define settings struct: per-radio enable bools, rotation interval
- [ ] Save settings to Flipper's persistent storage (`storage` API, e.g., `/ext/apps_data/fluckflock/settings.dat`)
- [ ] Load settings on app start; use sane defaults if file missing
- [ ] Save on change (or on app exit)

## Testing / Validation Hooks

- [ ] Host-side test harness (in `test/`) that compiles `generators/identifiers.c` + PRNG on desktop (no Flipper SDK)
- [ ] Test: SSID output is 1–32 bytes, printable, no control chars
- [ ] Test: BSSID first 3 bytes match an OUI in the table, unicast bit set, locally-administered bit clear
- [ ] Test: BLE MAC has locally-administered bit set, unicast bit clear
- [ ] Test: BLE name is 1–20 bytes, printable
- [ ] Test: generate 2000 SSIDs with same RNG — verify ≤ 1 repeat (birthday bound)
- [ ] Test: generate 2000 BLE MACs — verify 0 repeats
- [ ] Test: PRNG seeded with same seed produces identical output (deterministic)
- [ ] Stretch: on-device smoke test scene that runs 100 cycles and reports pass/fail on screen

## Build

- [ ] Verify `ufbt build` compiles cleanly with no warnings (`-Wall -Werror`)
- [ ] Verify FAP loads on Flipper Zero (or emulator) and reaches main menu
- [ ] Verify start → counters increment → stop cycle works for BLE
- [ ] Verify start → counters increment → stop cycle works for Sub-GHz
- [ ] Verify Wi-Fi toggle hidden when dev board absent
