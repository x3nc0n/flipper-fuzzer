# Task: ESP32 implementation

Implements the shared contract in `spec.md` on ESP32 (dual radio: Wi-Fi + Bluetooth
Classic/BLE). This is the highest-capability embedded target — prioritize it first as the
reference implementation.

## Toolchain
- [ ] Choose framework: ESP-IDF (recommended for raw 802.11 frame injection + `esp_wifi_80211_tx`)
      vs. Arduino-ESP32 (faster prototyping, wraps ESP-IDF anyway). Recommendation: ESP-IDF.
- [ ] Set up PlatformIO or native ESP-IDF project under `esp32/`.
- [ ] Target chip: ESP32 (classic) or ESP32-S3/C3 — note BLE 5 multi-advertising support differs
      by chip revision; document which chip(s) were tested.

## Wi-Fi identifier rotation
- [ ] Init Wi-Fi in a mode that allows raw frame TX (`esp_wifi_set_mode(WIFI_MODE_AP)` or
      promiscuous+raw TX combo) — decide between:
      - (a) Rapid SoftAP reconfiguration (`esp_wifi_set_config` with new SSID/BSSID each tick) — 1
        SSID visible at a time but simple/reliable.
      - (b) Raw 802.11 beacon frame injection via `esp_wifi_80211_tx` — construct beacon frames
        manually so many SSIDs can be broadcast per tick from one radio (higher fake-AP density).
      Recommendation: implement (b) for real "many SSIDs at once" behavior; keep (a) as fallback.
- [ ] Implement a beacon-frame builder: 802.11 management frame header + fixed params + SSID IE +
      supported rates IE + DS parameter IE, with a random BSSID as source/BSSID address.
- [ ] Batch-transmit N beacon frames per rotation tick (N configurable, start with a tested value
      e.g. 20–50 and tune based on radio/timing headroom).
- [ ] Rotate BSSID + SSID text per §5.2 of spec.md every tick with jittered interval.
- [ ] Implement SSID generator per spec.md §5.2.1: `ssid_words.h` with `protest_words[]`,
      `everyday_words[]`, `suffixes[]`, `connectors[]` arrays + template picker + weighted mix
      (~70% protest-flavored) + random case variation. Enforce 32-byte SSID cap.

## Bluetooth/BLE identifier rotation
- [ ] Init NimBLE (preferred, lower footprint) or Bluedroid stack.
- [ ] Use `esp_ble_gap_set_rand_addr` (or NimBLE equivalent) to rotate the BLE random address each
      tick, respecting the locally-administered address rules in spec.md §5.2.
- [ ] Rotate advertisement payload: device name, manufacturer data, service UUID list.
- [ ] If chip/stack supports BLE extended/multi-advertising sets, run multiple simultaneous
      advertisers with independently rotating addresses to increase concurrent identity count.
- [ ] (Stretch) Rotate Bluetooth Classic device name/CoD if BT Classic discoverability is enabled.

## Shared infra
- [ ] RNG: seed from `esp_random()` (uses HW TRNG) — no manual seeding needed, but document it.
- [ ] Config: store tunables (rotation interval, jitter, enable flags, concurrency) in
      `menuconfig`/`sdkconfig` or a simple `config.h`, with NVS override support for field tuning
      without reflashing.
- [ ] Status LED: blink pattern indicating "broadcasting" vs "radio init failed" vs "booting".
- [ ] No persistent logging of generated identifiers to flash by default (RAM/serial only,
      optional debug build flag to enable verbose serial logging).
- [ ] Watchdog: enable ESP-IDF task watchdog so a stuck radio call reboots rather than hangs
      forever.

## Verification
- [ ] Use a laptop with monitor mode (or a phone Wi-Fi scanner) to confirm multiple distinct SSIDs
      appear and change every ~1s.
- [ ] Use `nRF Connect` (or similar BLE scanner) to confirm BLE address/name rotation.
- [ ] Soak test: run 1+ hour, confirm no crash/reboot loop (check via serial log timestamps or
      LED heartbeat).
- [ ] Record achieved concurrent-identity count and any radio timing limits discovered — feed
      back into spec.md §7 capability matrix if numbers differ from estimates.

## Deliverables
- [ ] `esp32/` PlatformIO or ESP-IDF project, builds clean.
- [ ] `esp32/README.md`: build/flash instructions, config knobs, wiring/power notes.
