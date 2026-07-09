# FluckFlock Wi-Fi Companion Firmware

Companion firmware for the **official Flipper Zero Wi-Fi Dev Board** (ESP32-S2-WROVER-I).
Receives simple ASCII commands from the Flipper Zero over UART and injects 802.11 beacon
management frames using the ESP-IDF raw-injection API.

---

## Hardware

| Item | Detail |
|---|---|
| SoC | ESP32-S2 (Xtensa LX7, single-core) |
| Board target | `esp32-s2-saola-1` (generic ESP32-S2 SAOLA / WROVER-I) |
| Framework | Arduino + ESP-IDF (via PlatformIO `espressif32`) |

### Wiring (Flipper ↔ ESP32-S2)

| Flipper Zero pin | ESP32-S2 pin | Direction | Signal |
|---|---|---|---|
| GPIO 13 | GPIO 44 (UART0 RX / U0RXD) | Flipper → ESP32 | TX from Flipper |
| GPIO 14 | GPIO 43 (UART0 TX / U0TXD) | ESP32 → Flipper | RX to Flipper |
| GND | GND | — | Ground |

The official Flipper Wi-Fi Dev Board mates directly to the Flipper Zero expansion connector; 
no extra wiring is required when using that board.

---

## Building & Flashing

Prerequisites: [PlatformIO Core](https://platformio.org/install/cli) or PlatformIO IDE.

```bash
# From the esp32/fluckflock_companion/ directory:

# Build
pio run

# Build + flash (USB CDC on ESP32-S2)
pio run -t upload

# Monitor debug output over USB
pio device monitor
```

Ensure the Flipper Zero is **not** connected while flashing over USB.

---

## UART Line Protocol

All frames are ASCII, newline-terminated (`\n`).  The Flipper is always the host (initiator).

| Flipper → ESP32 | ESP32 → Flipper | Purpose |
|---|---|---|
| `FF?\n` | `FF!v1\n` | Handshake / board detection |
| `FFON\n` | `OK\n` | Enter 802.11 raw-injection mode |
| `FFOFF\n` | `OK\n` | Exit injection mode (low-power) |
| `FFB <bssid12> <ssid>\n` | *(none)* | Inject one 802.11 beacon frame |

**Field details:**

- `bssid12` — 12 lowercase hex chars, no separators (e.g. `aabbccddeeff`)
- `ssid` — raw SSID string, 1–32 printable ASCII/UTF-8 bytes, no newlines

Beacon frames are fire-and-forget; the companion does not ACK `FFB` commands.

---

## How It Works

1. **Handshake:** On receiving `FF?` the companion replies `FF!v1` so the Flipper driver can
   detect the board with a short timeout.
2. **Injection mode:** `FFON` calls `esp_wifi_set_promiscuous(true)` on the configured
   channel (default: channel 6, set via `FLUCKFLOCK_CHANNEL` in `platformio.ini`).
3. **Beacon injection:** `FFB` parses the BSSID + SSID, builds a minimal 802.11 management
   beacon frame (header, fixed params, SSID element, supported rates, DS parameter), and
   calls `esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false)`.
4. **Debug output:** All received commands and injection events are logged to USB CDC
   (`Serial0`) at 115200 baud.

---

## ⚠ Legal Notice

802.11 frame injection is regulated in most jurisdictions.  This firmware is intended
**solely** for:
- Personal RF privacy / anti-passive-tracking research
- Controlled lab / educational environments

You are solely responsible for ensuring that your use complies with applicable laws and
regulations (FCC Part 15, CE R&TTE, etc.).  Do **not** use this firmware to disrupt
legitimate wireless networks or in any prohibited manner.

---

## Protocol Module (host-testable)

The frame-building logic on the Flipper side lives in:
```
flipperzero/fluckflock/radio/wifi_proto.h
flipperzero/fluckflock/radio/wifi_proto.c
```
These files have no Flipper SDK dependency and can be compiled and tested on the host
(Hockney's domain).
