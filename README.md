# FluckFlock

> A Flipper Zero "chaff" broadcaster that floods passive wireless tracking systems with large volumes of plausible, ever-rotating fake identifiers.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Flipper Zero](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![SDK: 1.4.3](https://img.shields.io/badge/SDK-1.4.3-green)](https://github.com/flipperdevices/flipperzero-firmware)
[![Build: ufbt](https://img.shields.io/badge/build-ufbt-lightgrey)](https://github.com/flipperdevices/flipperzero-ufbt)
![Host Tests](https://github.com/x3nc0n/flipper-fuzzer/actions/workflows/host-tests.yml/badge.svg)

---

## Overview

FluckFlock is a Flipper Zero FAP (Flipper Application Package) that continuously emits large volumes of plausible-looking fake wireless identifiers across multiple radios simultaneously:

- **BLE** — rotating advertisement packets with realistic device names and locally-administered random MACs
- **Sub-GHz** — pseudo-random OOK/2-FSK bursts on common ISM frequencies via the CC1101
- **Wi-Fi** — beacon frames with plausible SSIDs and vendor OUI-correct BSSIDs (optional; requires the official Flipper Wi-Fi Dev Board)

The goal is to pollute passive, dragnet-style surveillance systems — Wi-Fi MAC loggers, BLE advertisement scrapers, RF fingerprinting setups — by burying real identifiers in a high-volume stream of believable chaff. FluckFlock is a **volume play**, not a stealth play.

**Single hardware target: Flipper Zero only.**

---

## Features

- **Three-radio coverage**
  - BLE advertising via the Flipper's native BLE stack
  - Sub-GHz via the built-in CC1101 (315 / 433.92 / 868 / 915 MHz, region-filtered)
  - Wi-Fi via the [official Flipper Wi-Fi Dev Board](https://shop.flipperzero.one/products/wifi-devboard) — auto-detected over UART; gracefully absent otherwise *(see [Wi-Fi Dev Board](#wi-fi-dev-board) and [Status](#status))*

- **Plausible identifier generation**
  - BLE MACs: locally-administered random-static addresses (bit 1 of first octet set, bit 0 clear) — spec-correct for BLE random addressing
  - BSSIDs: real-world AP vendor OUIs (≥ 20 prefixes: TP-Link, Netgear, Cisco, Ubiquiti, Aruba, Ruckus, and more) with random trailing octets; unicast + globally-administered bits set to look like genuine hardware
  - SSIDs: ≥ 30 realistic templates — ISP defaults, café/business names, home router patterns (`NETGEAR-XX`, `TP-Link_XXXX`, `ATT-WIFI-5G`, `eduroam`, etc.)
  - BLE device names: ≥ 30 realistic names — earbuds, fitness trackers, smart speakers, phone models, Tile/AirTag-style labels
  - Sub-GHz payloads: pseudo-random bytes; no protocol framing required — goal is RF-level noise

- **Seedable PRNG** — xoshiro256\*\*-backed `ChaffRng` seeded from `furi_hal_random` at launch; fixed-seed mode available for deterministic testing

- **Per-radio toggles** — enable or disable BLE, Sub-GHz, and Wi-Fi independently at runtime; Wi-Fi toggle is hidden when the dev board is absent

- **Live counters** — per-radio identifier-emitted counts displayed in the running scene, refreshed at ≥ 1 Hz

- **Configurable rotation interval** — 500 ms – 5 s per cycle, adjustable in Settings

- **Disclaimer on launch** — brief About scene reminding users of legal obligations

---

## Status

| Component | Status |
|---|---|
| BLE advertising | ✅ Verified on real hardware |
| Sub-GHz (CC1101) | ✅ Verified on real hardware |
| Wi-Fi beacon (dev board) | 🧪 Implemented + host-tested; on-device validation pending |
| ufbt build (SDK 1.4.3) | ✅ Clean (`-Wall -Werror`) |
| Host unit test suite | ✅ ~71 k assertions, all pass |

Wi-Fi: `radio_wifi_emit()` is fully implemented — the UART handshake-based dev-board detection, per-beacon `FFB` commands over the `wifi_proto` line protocol, and the ESP32-S2 companion firmware (`esp32/fluckflock_companion/`) are all in place and host-tested (236 protocol assertions). **On-device validation is still pending** — the ESP32 companion firmware has not yet been flashed due to a USB enumeration issue on the development machine. BLE and Sub-GHz have been confirmed working on a real Flipper Zero (FAP deployed over USB; emission counters climbing on-device).

---

## Build & Install

### Prerequisites

- A Flipper Zero with firmware ≥ 1.4.3
- [**ufbt**](https://github.com/flipperdevices/flipperzero-ufbt) — the Flipper micro build tool

  ```sh
  pip install ufbt
  ```

### Build

```sh
cd flipperzero/fluckflock
ufbt
```

The compiled application lands at `dist/fluckflock.fap`.

### Deploy to a connected Flipper

```sh
ufbt launch
```

This builds (if needed) and deploys `fluckflock.fap` to a Flipper Zero connected via USB.

---

## Running the Host Tests

The identifier generators are pure C with no Flipper SDK dependency and are tested entirely on-host:

```sh
cd test
make test
```

Requires `gcc` or `clang`. Runs the full suite (~71 k assertions) covering SSID/BSSID/BLE MAC/BLE name generation, PRNG determinism, address-bit correctness, and uniqueness bounds.

---

## Repository Layout

```
flipper-fuzzer/
├── spec.md                          # FluckFlock specification
├── task-flipper.md                  # Implementation checklist
├── flipperzero/
│   └── fluckflock/
│       ├── application.fam          # FAP manifest (appid, entry point, icon)
│       ├── fluckflock.c / .h        # App entry point, App struct, ViewDispatcher / SceneManager
│       ├── chaff_engine.c / .h      # Chaff engine — scheduler, per-radio lifecycle, stats
│       ├── generators/
│       │   ├── identifiers.c / .h   # Pure identifier generators (SSID, BSSID, BLE MAC/name, Sub-GHz payload)
│       │   ├── prng.c / .h          # Seedable PRNG (xoshiro256**)
│       │   ├── oui_table.h          # AP vendor OUI prefix table
│       │   ├── ssid_table.h         # SSID template table
│       │   └── ble_name_table.h     # BLE device name table
│       ├── radio/
│       │   ├── radio_ble.c / .h     # BLE advertising driver
│       │   ├── radio_subghz.c / .h  # Sub-GHz (CC1101) driver
│       │   ├── radio_wifi.c / .h    # Wi-Fi dev board driver (furi_hal_serial UART, handshake + per-beacon emit)
│       │   └── wifi_proto.c / .h    # UART line-protocol builders (pure C, no Flipper SDK — host-testable)
│       └── scenes/
│           ├── scene_main_menu.c    # Main menu
│           ├── scene_running.c      # Live-counter running scene
│           ├── scene_settings.c     # Rotation interval settings
│           └── scene_about.c        # About / disclaimer
├── esp32/
│   └── fluckflock_companion/        # ESP32-S2 companion firmware (PlatformIO, board: esp32-s2-saola-1)
│       ├── src/main.cpp             # Parses UART protocol, injects raw 802.11 beacons via esp_wifi_80211_tx
│       ├── platformio.ini           # PlatformIO project config
│       └── README.md                # Flashing instructions and protocol reference
└── test/
    ├── test_identifiers.c           # Host tests for identifier generators
    ├── test_prng.c                  # Host tests for PRNG determinism
    ├── test_wifi_proto.c            # Host tests for UART protocol builders (236 assertions)
    └── Makefile                     # Host build — no Flipper SDK required
```

---

## Wi-Fi Dev Board

Wi-Fi beacon injection requires the [official Flipper Wi-Fi Dev Board](https://shop.flipperzero.one/products/wifi-devboard) (ESP32-S2-WROVER-I). Setup is two-part:

1. **Flash the companion firmware** — the `esp32/fluckflock_companion/` directory is a PlatformIO project targeting the `esp32-s2-saola-1` board. Open it in VS Code with the PlatformIO extension (or run `pio run --target upload`) to build and flash it. See [`esp32/fluckflock_companion/README.md`](esp32/fluckflock_companion/README.md) for full flashing instructions.

2. **Mount and run** — with the companion firmware flashed, plug the dev board onto the Flipper's expansion header. The FAP auto-detects it at startup via a UART handshake (`FF?` / `FF!`) and enables the Wi-Fi toggle in the UI. No manual configuration required.

> **⚠️ On-device validation pending.** The companion firmware has been implemented and all protocol logic is host-tested (236 assertions pass), but end-to-end beacon injection on the physical dev board has not yet been validated. A USB enumeration issue on the development machine is blocking the initial flash. This section will be updated once on-device testing is complete.

---

## ⚠️ Responsible Use / Legal

FluckFlock is a **security research and personal anti-tracking tool**. It is intended to help individuals protect themselves from passive, dragnet-style wireless surveillance.

**Radio transmission is regulated.** Sub-GHz and Wi-Fi emissions — including the specific frequencies, power levels, duty cycles, and types of transmissions permitted — are governed by national telecommunications law and vary significantly by country. Operating on unauthorized frequencies, exceeding power limits, or violating duty-cycle rules may be illegal where you are.

**You are solely responsible for:**

- Ensuring your use complies with all applicable local, national, and international laws and regulations
- Confirming you are authorized to transmit on any frequency you use
- Not interfering with licensed radio communications, emergency services, or safety-critical systems

**FluckFlock must not be used to:**

- Disrupt, deny, or degrade others' wireless networks or communications
- Harass, stalk, or target individuals
- Evade law enforcement or conduct any unlawful activity

The Flipper Zero's built-in region configuration enforces Sub-GHz frequency and duty-cycle limits; FluckFlock does not bypass these restrictions. Use responsibly.

---

## Contributing

Issues and pull requests are welcome. Please open an issue before undertaking large changes so we can discuss approach. All contributions are subject to the MIT License.

---

## License

[MIT](LICENSE) — see the LICENSE file for full terms.

---

## Credits

FluckFlock was designed and built by an AI "Squad" team: **Keaton** (Lead / Firmware Architect), **Fenster** (Embedded Firmware Dev), **McManus** (Wireless / RF Dev), and **Hockney** (QA / Test). Project owner: [@x3nc0n](https://github.com/x3nc0n).
