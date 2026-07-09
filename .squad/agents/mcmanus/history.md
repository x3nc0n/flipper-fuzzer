# Project Context

- **Owner:** John Spaid
- **Project:** FluckFlock — a Flipper Zero "chaff" broadcaster that continuously emits large volumes of plausible, ever-rotating fake wireless identifiers (Wi-Fi SSIDs/BSSIDs, BLE addresses/advert names, Sub-GHz RF noise/beacons) to pollute passive/dragnet tracking systems (Wi-Fi/BLE MAC tracking, SSID probe logging, RF fingerprinting).
- **Stack:** Flipper Zero firmware in C (FAP app, ufbt/fbt build, furi APIs, ViewDispatcher/SceneManager). Native BLE + Sub-GHz (CC1101); Wi-Fi via the official Flipper Wi-Fi Dev Board (ESP32) where present.
- **Target:** Flipper Zero ONLY — single hardware target (the earlier "two hardware tiers / two implementations" idea was scrubbed per user directive 2026-07-09). One implementation checklist: task-flipper.md.
- **Created:** 2026-07-09

## Learnings

<!-- Append new learnings below. Each entry is something lasting about the project. -->

### 2026-07-09 — Generator approach, table sizes, radio HAL assumptions, radio driver signatures

**PRNG:**  
Chose xoshiro256** seeded via splitmix64 (4 × 64-bit state, 256-bit total).
splitmix64 guarantees non-zero state even from seed=0.  chaff_rng_bounded()
uses Lemire's nearly-divisionless algorithm to eliminate modulo bias.
Pure C, malloc/free only — host-testable without any Flipper SDK.

**OUI table (`oui_table.h`):**  
68 entries covering Apple, Samsung, Cisco, TP-Link, Netgear, Espressif, Intel,
Google, Amazon, Ubiquiti, Realtek, Aruba.  `CHAFF_OUI_COUNT` macro for iteration.
All entries verified to have bit0=bit1=0 (globally-administered unicast); the
generator also enforces this with an explicit mask on out[0].

**SSID table (`ssid_table.h`):**  
10 templates (SSID_TEMPLATE_COUNT): ISP-prefix+hex, ISP-prefix+decimal,
"<Name>'s <Device>", static/well-known names, HP-Print pattern, Wi-Fi Direct,
AndroidAP, Redmi Note, Setup<hex>, ASUS_<hex>.  Pool sizes: 40 first names,
14 ISP prefixes, 27 static names, 5 HP models, 11 hotspot devices.

**BLE name table (`ble_name_table.h`):**  
5 pools: earbuds (23), wearables (15), speakers (14), trackers (10), phones (9).
Possessive pattern "<Name>'s <Device>" uses 30 first-names × 9 device-suffixes.
~30% chance of appending a 2-digit numeric suffix for population diversity.

**BLE MAC address bit convention:**  
Reconciled Keaton's contract (bit0=unicast, bit1=LA) with BLE Core Spec §6.B.1.3.2.1
(random static: bits[47:46]=0b11 = byte[0] bits[7:6]).  Final mask on out[0]:
`(out[0] | 0xC2) & 0xFE` — sets bits 7,6,1; clears bit 0.

**Radio HAL — BLE:**  
Implemented against `furi_hal_bt_extra_beacon_set_data()` / `set_config()` /
`start()` / `stop()` / `is_active()`.  GapExtraBeaconConfig fields assumed:
min/max_adv_interval_ms (uint16_t), adv_channel_map (uint8_t, 0x07),
adv_power_level (int8_t), address_type (BLE_GAP_ADDR_TYPE_RANDOM_STATIC=1),
address[6].  MAC byte order for set_config: reversed (little-endian, index 5→0).
Advertisement payload: Flags(0x01/0x06) + Complete Local Name(0x09) + TX Power(0x0A/0x00).

**Radio HAL — Sub-GHz:**  
Uses `FuriHalSubGhzPresetOok270Async` (OOK 270 kHz).  Default freq: 433.92 MHz.
furi_hal_subghz_set_frequency_and_path() enforces region lock; returns 0 on
rejection.  furi_hal_subghz_write_packet() assumed present; if missing, use
manual FIFO writes + furi_hal_subghz_tx() strobe.  Payload capped at 60 bytes.

**Radio HAL — Wi-Fi:**  
Fully stubbed. detect() returns false unconditionally.  UART protocol proposed:
Marauder-compatible ASCII at 115200 baud on FuriHalSerialIdUsart.
Command: `beacon -s <SSID> -b <XX:XX:XX:XX:XX:XX>\n`.  Waiting on Keaton to
gate the protocol decision before implementing.

### 2026-07-09 — FINAL OUTCOME

- **ufbt build against Flipper SDK 1.4.3: SUCCEEDS**
- **Host test suite (~71k assertions): all PASS**
- **All blockers and major issues: RESOLVED via cross-author fix round (locked out from own fixes per architecture rules)**
- **Project approved for merge**

**Radio driver signatures (stable — published to mcmanus-radio-interface.md):**
```
void radio_ble_init(void);
void radio_ble_deinit(void);
bool radio_ble_emit(const uint8_t mac[6], const char* name);

void radio_subghz_init(void);
void radio_subghz_deinit(void);
bool radio_subghz_emit(const uint8_t* payload, size_t len);

bool radio_wifi_detect(void);
bool radio_wifi_init(void);
void radio_wifi_deinit(void);
bool radio_wifi_emit(const char* ssid, const uint8_t bssid[6]);
```

Note: radio_wifi.h was pre-stubbed by another agent with `radio_wifi_send(ssid, ssid_len, bssid)`.
McManus updated it to `radio_wifi_emit(ssid, bssid)` for naming consistency.
Fenster must use the updated header.

### 2026-07-09 — Correct notification include path in SDK 1.4.3 (f7)

`notification/notification_app.h` does **not** exist in Flipper SDK 1.4.3.  The correct
includes are:

```c
#include <notification/notification.h>          // NotificationApp typedef + notification_message()
#include <notification/notification_messages.h>  // built-in NotificationSequence presets
```

`NotificationApp` (opaque struct) and `notification_message()` live in `notification.h`.
`notification_messages.h` provides the pre-built sequences (e.g. `&sequence_blink_red_10`).
Verified via: `/home/x3nc0n/.ufbt/current/sdk_headers/f7_sdk/applications/services/notification/notification.h`.

