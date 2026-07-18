# FluckFlock — Specification

**Version:** 1.1
**Date:** 2026-07-18
**Target:** Flipper Zero (primary host / controller)
**Companion platforms:** ESP32, ESP8266, Raspberry Pi (see §11)

---

## 1. Overview

FluckFlock is a device-identifier "chaff" broadcaster. It continuously emits large volumes of plausible-looking wireless identifiers — BLE advertisement names/addresses, Sub-GHz RF beacons/noise, and optionally Wi-Fi SSIDs/BSSIDs (when the official Flipper Wi-Fi Dev Board is present) — that constantly rotate at random. The goal is to pollute passive/dragnet-style tracking systems with a high volume of fake, ever-changing identifiers, making it materially harder to distinguish and follow a real device or person among the noise.

The Flipper Zero is the primary host and controller. Companion firmware for ESP32, ESP8266, and Raspberry Pi targets extends FluckFlock's radio coverage by pairing with the Flipper as additional broadcast radios (see §11 — Companion Platforms).

## 2. Goals

1. Emit a high volume of plausible wireless identifiers across available radios.
2. Rotate identifiers rapidly so no single fake identity persists long enough to be correlated.
3. Keep the app useful with only the Flipper Zero's native radios (BLE + Sub-GHz). Wi-Fi chaff is an optional bonus when the dev board is detected.
4. Provide simple runtime controls: start/stop all, per-radio toggles, live counters.
5. Ship a clean, testable codebase — identifier generators are pure functions testable on-host without hardware.
6. Support companion platforms (ESP32, ESP8266, Raspberry Pi) as optional paired broadcast radios extending Flipper coverage.

## 3. Non-Goals

- Jamming or denial-of-service against legitimate networks.
- Connecting to or interacting with real access points, BLE peripherals, or other devices.
- Emulating specific real-world devices or identities (i.e., no targeted spoofing).
- Persistent tracking / logging of nearby real devices.
- Over-the-air firmware updates or remote control.
- Replacing the Flipper Zero as host — companion platforms augment, not substitute.

## 4. Threat Model Targeted

FluckFlock targets **passive / dragnet-style surveillance** that relies on:

| Surveillance vector | How FluckFlock counters it |
|---|---|
| Wi-Fi probe-request MAC logging | Floods fake BSSIDs/SSIDs (requires dev board) |
| BLE MAC / advertisement tracking | Emits rotating BLE advertisements with plausible names and locally-administered MACs |
| RF fingerprinting on ISM bands | Emits Sub-GHz noise/beacons on common ISM frequencies via CC1101 |

**Assumption:** The adversary passively collects identifiers in bulk. FluckFlock's value is proportional to the volume and plausibility of chaff emitted.

**Out-of-model:** Active, targeted adversaries who fingerprint the Flipper Zero's own radio characteristics (e.g., TX power profile, timing jitter) are not addressed. This is a volume play, not a stealth play.

## 5. Capabilities by Radio

### 5.1 BLE Advertising (Native)

- Emit BLE advertisement packets using the Flipper's built-in BLE stack.
- Each advertisement cycle uses:
  - A **locally-administered random MAC** (bit 1 of the first octet set; unicast).
  - A **plausible BLE device name** drawn from realistic patterns (e.g., "Galaxy Buds Pro", "JBL Flip 6", "Tile Slim", "AirPods", short alphanumeric IDs).
  - Valid advertisement structure (flags, complete/shortened local name, TX power level).
- Rotate MAC + name every cycle (configurable interval, default ≤ 2 seconds).

### 5.2 Sub-GHz via CC1101 (Native)

- Emit OOK or 2-FSK beacons/noise on common ISM frequencies (e.g., 315 MHz, 433.92 MHz, 868 MHz, 915 MHz — region-dependent).
- Payload: short pseudo-random bursts. No requirement for protocol-valid framing — goal is RF-level noise, not protocol spoofing.
- Rotate frequency and payload each cycle.
- Respect the Flipper's built-in region lock and duty-cycle limits.

### 5.3 Wi-Fi via Dev Board (Optional)

- **Gated on detection** of the official Flipper Wi-Fi Dev Board (ESP32-S2-WROVER-I) over UART.
- **Detection:** At app start the Flipper sends `FF?\n` on the expansion-header USART (FuriHalSerialIdUsart, GPIO 13 TX / GPIO 14 RX, 115200 8N1) and waits up to 400 ms for the companion to reply with an `FF!` prefix. If no board responds, Wi-Fi is silently disabled for the session.
- **UART protocol:** ASCII line-oriented, `\n`-terminated, Flipper always the host. Commands: `FF?` (detect), `FFON` (enter injection mode), `FFOFF` (exit injection mode), `FFB <bssid12hex> <ssid>` (beacon, fire-and-forget). Frame builders live in `wifi_proto.h` / `wifi_proto.c` (pure C, no Flipper SDK dependency, host-testable).
- **Companion firmware:** `esp32/fluckflock_companion/` — PlatformIO / Arduino + ESP-IDF targeting the ESP32-S2. On `FFON`, enters 802.11 promiscuous mode (required for `esp_wifi_80211_tx`) on a configured channel (default ch 6, compile-time `FLUCKFLOCK_CHANNEL`). On `FFOFF`, exits injection mode.
- **Beacon injection:** For each chaff cycle the Flipper sends one `FFB` command. The companion builds a minimal 802.11 management beacon (24-byte MAC header, 12-byte fixed params, SSID tagged element, supported rates IE, DS parameter IE) and calls `esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false)`.
- **Autonomous re-beacon (Option B):** The companion autonomously re-transmits the last-received beacon frame at ~100 ms intervals until the next `FFB` replaces it or `FFOFF` stops injection. This ensures beacons persist long enough for Wi-Fi scanners to detect them.
- **Identifier plausibility:** Same pools as other radios — plausible SSID strings drawn from the `chaff_generate_ssid` table; BSSID = vendor OUI prefix + 3 random bytes via `chaff_generate_bssid`.
- **Rotation:** SSID + BSSID rotate every chaff cycle, same cadence as BLE / Sub-GHz.
- **If board absent:** the Wi-Fi radio toggle is disabled/hidden in the UI. The app runs with BLE + Sub-GHz only. No errors are surfaced to the user.
- **Limitation:** Channel is fixed at compile time (`FLUCKFLOCK_CHANNEL`, default 6). Per-emission or timed channel-hopping is not yet implemented.
- **Legal / safety note:** 802.11 frame injection is regulated. Users are responsible for compliance with local laws. The app's first-launch disclaimer applies. See `esp32/fluckflock_companion/README.md`.

## 6. Identifier Plausibility Requirements

All generated identifiers must look "real enough" to survive cursory automated inspection:

| Identifier | Plausibility rules |
|---|---|
| **SSID** | 1–32 bytes. Drawn from a pool of realistic templates (common ISP default names, café/business names, home router patterns like `NETGEAR-XX`, `TP-Link_XXXX`). Mix of static-looking and randomized-suffix names. No null bytes or control characters. |
| **BSSID** | 6 bytes. First 3 bytes from a curated OUI table of common AP vendors (TP-Link, Netgear, Cisco, Ubiquiti, etc., ≥ 20 OUIs). Last 3 bytes random. Unicast bit set, locally-administered bit clear (to look like a real vendor MAC). |
| **BLE MAC** | 6 bytes. Locally-administered bit SET (bit 1 of first octet). Unicast (bit 0 of first octet clear). Remaining bits random. This is standard for BLE random addresses. |
| **BLE Name** | 1–20 bytes. Drawn from a pool of realistic BLE device names (earbuds, fitness trackers, smart home devices, Tile/AirTag-style names, phone model names). |
| **Sub-GHz payload** | Pseudo-random bytes. No plausibility requirement — goal is RF noise. |

**No obvious garbage.** All string identifiers must be valid UTF-8, printable, and look like something a real device would emit.

## 7. Rotation / Randomization Requirements

- **High volume:** Emit as many distinct identifiers per minute as the radio hardware allows without violating duty-cycle / regulatory limits.
- **Ever-changing:** No identifier (MAC, SSID, BSSID, BLE name) should repeat within a rolling window of at least 1000 emitted identifiers per radio.
- **Low predictability:** Identifiers are generated via a PRNG seeded from hardware entropy (Flipper's `furi_hal_random`). The PRNG state is not persisted — a fresh seed on every app launch.
- **Rotation interval:** Configurable per-radio, default 1–2 seconds per cycle. Each cycle emits one new set of identifiers per enabled radio.

## 8. Runtime / UX Requirements

- **Main menu:** List of radios with current enable/disable state.
- **Start / Stop:** Global toggle to begin/cease all chaff emission.
- **Per-radio toggles:** Enable or disable each radio independently (BLE, Sub-GHz, Wi-Fi). Wi-Fi toggle visible only when dev board detected.
- **Live status / counters:** While running, display per-radio count of identifiers emitted since start. Update at least once per second.
- **Minimal footprint:** The app should use ≤ reasonable RAM. No heap-heavy data structures.

## 9. Constraints & Safety

- **Legal / ethical:** This tool is intended for **security research and personal anti-tracking** use. Users are responsible for compliance with local laws regarding radio emissions. The app should display a brief disclaimer on first launch.
- **Region limits:** Sub-GHz frequencies and duty cycles must respect the Flipper's built-in region configuration. Do not bypass region locks.
- **Radio coexistence:** BLE advertising must coexist with the Flipper's own BLE stack (used for the companion app connection). If coexistence is not feasible, document the limitation.
- **Power:** Continuous multi-radio TX will drain the battery. No specific runtime target, but avoid unnecessary CPU wake-ups when paused.
- **No network interaction:** The app never connects to, authenticates with, or sends data to any external network or device (beyond broadcasting chaff).

## 10. Out of Scope

- Desktop/mobile companion app integration.
- Logging or recording of emitted identifiers.
- Receiving or scanning for real devices.
- GPS/location tagging.
- Firmware modification or custom Flipper firmware. This is a standard FAP.
- Protocol-valid Wi-Fi association / deauth frames (we emit beacons only).

## 11. Companion Platforms

FluckFlock supports optional **companion platform** firmware that extends chaff broadcast coverage beyond the Flipper Zero's native radios. Each companion pairs with the Flipper as an additional broadcast radio — the Flipper remains the host/controller.

### 11.1 Architecture

- **Host:** Flipper Zero — owns the chaff engine, identity generation, rotation timing, UX, and radio coordination.
- **Companions:** Standalone firmware images that receive broadcast commands from the Flipper (or run autonomously when configured for standalone operation) and emit chaff on their native radios.
- **Protocol:** Companions that pair with the Flipper use the UART line protocol defined in §5.3. Standalone operation is an alternative deployment mode documented in each platform's task file.

### 11.2 Supported Companion Targets

| Platform | Radio capabilities | Task file | Notes |
|---|---|---|---|
| **ESP32** | Wi-Fi (802.11 raw frame injection) + BLE (dual-mode) | [`task-esp32.md`](task-esp32.md) | Highest-capability embedded companion. Reference companion implementation. |
| **ESP8266** | Wi-Fi only (raw frame injection, no BLE radio) | [`task-esp8266.md`](task-esp8266.md) | Constrained/low-cost Wi-Fi-only companion. Single-core, less RAM. |
| **Raspberry Pi** | Wi-Fi (monitor-mode USB adapter) + BLE (BlueZ) + experimental RF (rpitx/SDR) | [`task-raspberrypi.md`](task-raspberrypi.md) | Highest flexibility. Full Linux userspace. RTL-SDR is receive-only — transmit requires separate hardware. |

### 11.3 Shared Behavioral Contract for Companions

All companion implementations share these requirements (derived from the Flipper host spec):

- **Identity rotation:** Same cadence as Flipper host — base ~1 s, jittered ±400 ms. No identifier repeats within the no-repeat window (1000 per radio, matching §7).
- **Identifier plausibility:** Same rules as §6 — OUI-prefixed BSSIDs, realistic SSID templates, locally-administered BLE MACs.
- **Graceful degradation:** If one radio fails to init, continue on others.
- **No network interaction:** Transmit-only. No connections to external networks.
- **Legal compliance:** Same disclaimer and regulatory constraints as §9. RTL-SDR transmit paths (rpitx) are experimental/opt-in and clearly flagged.

### 11.4 Legal & Regulatory Notes for Companion Platforms

- Transmitting on Wi-Fi/BLE ISM bands with standard radios (ESP32/ESP8266 Wi-Fi+BT, RPi with USB dongles) is normal use of unlicensed spectrum in most jurisdictions.
- **RTL-SDR is receive-only.** RF-level broadcasting on Raspberry Pi requires separate transmit-capable hardware (rpitx GPIO, HackRF, LimeSDR, etc.). Transmitting outside ISM bands or at excessive power may violate spectrum regulations (FCC Part 15 or local equivalent). Treat any rpitx-style path as experimental/opt-in.
- MAC address randomization with locally-administered addresses (U/L bit set) is standards-compliant and preferred over spoofing real OUI blocks.

## 12. Repo Structure

```
FluckFlock/
  spec.md                          <- this file (primary contract)
  task-flipper.md                  <- Flipper Zero implementation checklist
  task-esp32.md                    <- ESP32 companion implementation checklist
  task-esp8266.md                  <- ESP8266 companion implementation checklist
  task-raspberrypi.md              <- Raspberry Pi companion implementation checklist
  flipperzero/fluckflock/          <- Flipper Zero FAP source (C, ufbt build)
  esp32/fluckflock_companion/      <- ESP32 companion firmware (PlatformIO)
  esp8266/                         <- ESP8266 companion (created when work starts)
  raspberrypi/                     <- Raspberry Pi companion (created when work starts)
  test/                            <- Host-side unit tests (pure C, no SDK dependency)
```
