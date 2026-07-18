# McManus: RF/Wireless Developer — History (Summarized 2026-07-18)

**Owner:** John Spaid
**Project:** FluckFlock — Flipper Zero wireless chaff broadcaster
**Role:** RF/wireless specialist
**Created:** 2026-07-09

## Learnings & Status

### 2026-07-09 — RF Drivers, Identifiers, PRNG, HAL (summary)

**PRNG:** xoshiro256** seeded via splitmix64; Lemire's bounded random; host-testable pure C.
**OUI table:** 68 globally-administered unicast entries (Apple, Samsung, Cisco, TP-Link, Netgear, Espressif, Intel, Google, Amazon, Ubiquiti, Realtek, Aruba).
**SSID table:** 10 templates + 40 first names, 14 ISP prefixes, 27 static names.
**BLE names:** 5 pools (earbuds/wearables/speakers/trackers/phones) + possessive pattern (30 × 9).
**BLE MAC:** Random static; out[0] = `(byte | 0xC2) & 0xFE`.
**Radio signatures:** BLE (init/deinit/emit), Sub-GHz (init/deinit/emit), Wi-Fi (detect/init/deinit/emit).
**Sub-GHz:** 433.92 MHz OOK 270 kHz on CC1101; region lock enforced; 60-byte payload max.
**Wi-Fi HAL:** Stubbed pending protocol decision.

**Result:** ufbt build SUCCEEDS; host tests (71k assertions) PASS; all blockers RESOLVED.
**Verdict:** Project approved 2026-07-09.

### 2026-07-09 — Wi-Fi UART Protocol & ESP32 Companion (summary)

**Protocol:** `FF?` → `FF!v1`, `FFON` → `OK`, `FFOFF` → `OK`, `FFB <bssid12> <ssid>`
**Baud:** 115200 8N1 on FuriHalSerialIdUsart (Flipper GPIO 13 TX / 14 RX)
**Companion:** ESP32-S2 Arduino; promiscuous mode; `esp_wifi_80211_tx()` raw beacon injection
**Files:** `wifi_proto.h/c` (pure C, host-testable), `radio_wifi.c` (full UART driver), `esp32/` companion
**Verdict (Keaton):** APPROVE; minor cleanup notes (Serial.println `\r\n`, seq numbers, doc stale comment)

### 2026-07-18 — RF Validation & Identifier Signatures (summary)

**Wi-Fi channel:** 6 (2.4 GHz)
**Sub-GHz frequency:** 433.92 MHz OOK 270 kHz
**SSID pool:** 10 templates (ISP+hex, ISP+decimal, Name's Device, well-known, HP-Print, Wi-Fi Direct, AndroidAP, Redmi, Setup, ASUS)
**BSSID OUI:** 68 real vendors (Apple, Samsung, Cisco, TP-Link, Netgear, Espressif, Intel, Google, Amazon, Ubiquiti, Realtek, Aruba)
**BLE names:** Earbuds (25%), wearables (17%), speakers (17%), trackers (17%), phones (8%), possessive (17%); ~30% numeric suffix
**Validation method:** netsh-diff (Windows), Wireshark monitor-mode (gold standard)

### 2026-07-18 — OTG Power, Boot Settle, Detect Retry (summary)

**OTG API:** `furi_hal_power_enable_otg()` / `disable_otg()` / `is_otg_enabled()`
**Boot settle:** 1500 ms (account for serial init, USB CDC startup, Arduino framework jitter)
**Detect flow:** pre-flush + 3 attempts × 400 ms timeout + 150 ms retry gaps
**Power lifecycle:** enable in `radio_wifi_power_on()`, disable in `radio_wifi_power_off()`
**Idempotent:** guards prevent double-enable; safe to power-off unconditionally

### 2026-07-18 — Beacon Visibility: Options & Recommended Parameters (summary)

**Root cause:** Single-shot beacons invisible to OS scanners (need multiple frames from same BSSID)
**Option A (Flipper dwell):** 100 ms timer, 1.5s hold per SSID; 40 SSIDs/min; no ESP change; 2.8% UART
**Option B (ESP autonomous):** ESP re-tx every 100ms; ~20-line firmware; same 40 SSIDs/min; 0.2% UART
**Option C (ESP pool N=8):** 8 concurrent APs; ~60-line firmware; future "density mode"
**Recommendation:** Ship A now (zero risk), migrate to B later (ideal), add C later (future)
**115200 baud:** Not a bottleneck at any rate (10–50 FFB/sec fit comfortably)

---

### 2026-07-18 — Option B ESP Autonomous Beacon Re-transmission Implemented (summary)

**Architecture:** Store last frame + re-tx every 100ms in ESP loop() while injection enabled
**Idempotent:** Errors don't clear store (stale beacon repeats until next valid FFB)
**Injection flow:** start clears store, stop clears store (no orphaned beacons)
**Millis rollover:** Unsigned subtraction safe on uint32_t
**PlatformIO build:** SUCCESS; 45.9% flash, 11.2% RAM
**Status:** Pending hardware validation + Keaton code review

### 2026-07-09 — Notification API & Hardware Validation (summary)

**Include path (SDK 1.4.3):** `notification/notification.h` + `notification/notification_messages.h` (NOT notification_app.h)
**Hardware test:** FAP deployed successfully to physical Flipper Zero; BLE + Sub-GHz counters confirmed incrementing
**Wi-Fi Dev Board:** Firmware ready, awaiting physical integration test

