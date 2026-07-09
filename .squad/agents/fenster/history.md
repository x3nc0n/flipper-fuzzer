# Project Context

- **Owner:** John Spaid
- **Project:** FluckFlock — a Flipper Zero "chaff" broadcaster that continuously emits large volumes of plausible, ever-rotating fake wireless identifiers (Wi-Fi SSIDs/BSSIDs, BLE addresses/advert names, Sub-GHz RF noise/beacons) to pollute passive/dragnet tracking systems (Wi-Fi/BLE MAC tracking, SSID probe logging, RF fingerprinting).
- **Stack:** Flipper Zero firmware in C (FAP app, ufbt/fbt build, furi APIs, ViewDispatcher/SceneManager). Native BLE + Sub-GHz (CC1101); Wi-Fi via the official Flipper Wi-Fi Dev Board (ESP32) where present.
- **Target:** Flipper Zero ONLY — single hardware target (the earlier "two hardware tiers / two implementations" idea was scrubbed per user directive 2026-07-09). One implementation checklist: task-flipper.md.
- **Created:** 2026-07-09

## Learnings

<!-- Append new learnings below. Each entry is something lasting about the project. -->

### 2026-07-09T18:05:00Z — Running scene: no two-column layout for wide identifiers

BLE MACs ("AA:BB:CC:DD:EE:FF", ~17 chars, ~100px at FontSecondary) and Sub-GHz identifiers overflow a 64px half-column and collide with adjacent text. The running scene **must use full-width vertical rows** (x=0, full 128px) for all identifier lines — never a two-column split at x=64. Identifiers must also be copied into a bounded buffer (`char ident[22]`) via `snprintf` so strings longer than ~21 FontSecondary chars are silently truncated before being handed to `widget_add_string_element`.

### 2026-07-09 — App scaffold + engine wiring complete

**App structure:**
- Entry point: `fluckflock_app()` in `fluckflock.c`
- App struct `FluckFlockApp` holds: Gui*, ViewDispatcher*, SceneManager*, Submenu*, VariableItemList*, Widget*, NotificationApp*, ChaffEngine*, FluckFlockSettings, `bool wifi_detected`, `FuriTimer* status_refresh_timer`
- Three views registered: `FluckFlockViewSubmenu` (0), `FluckFlockViewVariableItemList` (1), `FluckFlockViewWidget` (2)
- Four scenes: MainMenu → Running, Settings, About. X-macro config in `scenes/scene_config.h`.
- Settings persisted as binary struct to `/ext/apps_data/fluckflock/settings.dat` (magic=0xF1F2, version=1).
- `fluckflock_settings_save()` is called on settings scene exit AND on clean app exit. Load happens at alloc time.

**Scene/view IDs:**

| Enum | Value | Module |
|---|---|---|
| `FluckFlockViewSubmenu` | 0 | `Submenu*` |
| `FluckFlockViewVariableItemList` | 1 | `VariableItemList*` |
| `FluckFlockViewWidget` | 2 | `Widget*` — shared by Running and About scenes |

| Scene | Transition trigger |
|---|---|
| `FluckFlockSceneMainMenu` | App start |
| `FluckFlockSceneRunning` | "Start Chaff" submenu item — starts engine before push |
| `FluckFlockSceneSettings` | "Settings" submenu item |
| `FluckFlockSceneAbout` | "About" submenu item |

**Custom events (FluckFlockCustomEvent):**
- 0 = MainMenuStart, 1 = MainMenuSettings, 2 = MainMenuAbout (match submenu index)
- 100 = RunningTick (500 ms UI refresh sent by status_refresh_timer)

**Chaff engine internals:**
- `struct ChaffEngine`: FuriTimer* (periodic, calls step()), FuriMutex* (protects stats[]), ChaffRng*, radio_enabled[3], stats[3], running bool.
- `chaff_engine_alloc()`: allocs, seeds RNG from `furi_hal_random_fill_buf`, sets BLE+SubGhz enabled by default.
- `chaff_engine_start()`: calls `radio_X_init()` for enabled radios, starts timer.
- `chaff_engine_stop()`: stops timer, calls `radio_X_deinit()` in reverse order.
- `chaff_engine_step()`: called from timer task — MUST be fast (< interval_ms).

**Radio driver signatures observed / defined:**
- `radio_ble_emit(const uint8_t mac[6], const char* name)` — McManus chose this (no name_len; name is null-terminated).
- `radio_subghz_emit(const uint8_t* payload, size_t len)` — McManus chose this (no frequency param; driver manages region-safe frequency internally).
- `radio_wifi_detect() → bool`, `radio_wifi_init() → bool`, `radio_wifi_deinit()`, `radio_wifi_send(ssid, ssid_len, bssid[6]) → bool` — Fenster defined these in `radio/radio_wifi.h`. McManus must implement `radio_wifi.c`.

**WiFi detection pattern:**
- `fluckflock.c` calls `radio_wifi_detect()` once at startup (may block ~500 ms) and stores result in `app->wifi_detected`.
- Engine `radio_enabled[ChaffRadioWifi]` is set by app based on detection result + loaded settings.
- Settings scene shows Wi-Fi toggle only when `app->wifi_detected == true`.

**status_refresh_timer ownership:**
- Allocated in `scene_running_on_enter()`, freed in `scene_running_on_exit()`.
- Stored as `app->status_refresh_timer` (NULL when not in running scene).
- `fluckflock_app_free()` checks for non-NULL and frees if needed (safety guard).

**Key assumption:**
- McManus's `radio_ble_emit()` and `radio_subghz_emit()` must complete within the emission interval (≥ 500 ms). If they block longer, the engine needs a FuriThread worker redesign (Keaton gate required).

**Decision filed:** `.squad/decisions/inbox/fenster-radio-signatures.md`

### 2026-07-09 — SDK 1.4.3 BLE address-type enum

SDK 1.4.3 (f7) defines `GapAddressType` in `targets/f7/ble_glue/extra_beacon.h` with exactly two values: `GapAddressTypePublic = 0` and `GapAddressTypeRandom = 1`. The old name `BLE_GAP_ADDR_TYPE_RANDOM_STATIC` does not exist. Use `GapAddressTypeRandom` when a random static BLE address is needed — it maps to the same value (1). The `GapExtraBeaconConfig.address_type` field is of type `GapAddressType`.

### 2026-07-09 — FINAL OUTCOME

- **ufbt build against Flipper SDK 1.4.3: SUCCEEDS**
- **Host test suite (~71k assertions): all PASS**
- **All blockers and major issues: RESOLVED via cross-author fix round (locked out from own fixes per architecture rules)**
- **Project approved for merge**

