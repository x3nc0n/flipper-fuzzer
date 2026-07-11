# Task: Raspberry Pi implementation

Implements the shared contract in `spec.md` on Raspberry Pi (any model with Linux + USB, e.g. Pi
Zero 2 W, Pi 3/4/5). This is the highest-flexibility/highest-power target: full Linux userspace,
multiple USB radios possible, but **no built-in RF transmit path via RTL-SDR** (see spec.md §4 —
RTL-SDR is receive-only). Wi-Fi and BLE broadcasting use standard adapters; any RF-level
transmission beyond Wi-Fi/BLE is a separate, explicitly opt-in experimental track.

## Hardware/OS prerequisites
- [ ] Raspberry Pi OS (Lite is fine — headless).
- [ ] At least one USB Wi-Fi adapter that supports **monitor mode + packet injection**
      (e.g. chipsets: Atheros AR9271, Ralink RT3070/RT5370, or the Pi's onboard chip if it
      supports injection — many onboard Broadcom chips do not; a USB adapter is the safer bet).
- [ ] Onboard or USB Bluetooth adapter supported by BlueZ.
- [ ] (Optional/experimental) RTL-SDR dongle — for §5 experimental RF track only, see below.

## Wi-Fi identifier rotation
- [ ] Put chosen adapter into monitor mode (`airmon-ng` or `iw` directly:
      `ip link set <if> down; iw <if> set monitor none; ip link set <if> up`).
- [ ] Build/send raw 802.11 beacon frames from Linux userspace. Recommended approaches, pick one:
      - (a) Python + Scapy (`Dot11Beacon`) — simplest to write/iterate, adequate throughput for
        moderate N.
      - (b) A small C tool using raw sockets (`AF_PACKET`) for higher throughput if Scapy proves
        too slow for the target concurrent-identity count.
      Recommendation: start with (a), profile achievable ticks/sec, drop to (b) only if needed.
- [ ] Batch-transmit N beacon frames per rotation tick. Expect the highest N of the three
      platforms here given more CPU — target starting at 50–100+ and tune based on measured
      adapter throughput and CPU headroom, especially if running multiple monitor-mode interfaces
      in parallel for even higher counts.
- [ ] Rotate BSSID + SSID text per §5.2 of spec.md every tick with jittered interval.
- [ ] Implement SSID generator per spec.md §5.2.1: `ssid_words.py`/`ssid_words.yaml` with
      `protest_words[]`, `everyday_words[]`, `suffixes[]`, `connectors[]` lists + template picker
      + weighted mix (~70% protest-flavored) + random case variation. Enforce 32-byte SSID cap.
- [ ] Support multiple simultaneous monitor-mode interfaces (if multiple adapters are plugged in)
      to multiply concurrent identity count.

## Bluetooth/BLE identifier rotation
- [ ] Use BlueZ via `bluetoothctl`/D-Bus API (preferred: `bluezero` or `dbus-python` binding) or
      `hcitool`/`hciconfig` for simpler address rotation, though modern BlueZ deprecates
      `hcitool` — prefer the D-Bus `org.bluez` advertising API (`LEAdvertisingManager1`).
- [ ] Rotate the BLE random address per tick using the D-Bus advertising API or by resetting the
      adapter's random address (`btmgmt` can set random static addresses).
- [ ] Rotate advertisement payload (name, service UUIDs, manufacturer data) alongside the address.
- [ ] If multiple BT adapters are available (onboard + USB), run independent advertisers on each
      to increase concurrent BLE identity count.

## RTL-SDR / RF track (experimental, opt-in, flagged separately)
- [ ] Confirm explicitly with the user before implementing this section — it requires additional
      transmit-capable hardware (RTL-SDR alone cannot transmit).
- [ ] If pursued: evaluate `rpitx` (uses a Pi GPIO pin as a crude RF transmitter, no separate TX
      hardware needed beyond an antenna/filter) as the lowest-cost transmit path, clearly labeled
      experimental/unsupported-power-level in the README.
- [ ] Keep this entirely separate/optional from the core Wi-Fi+BLE broadcaster (own script,
      own enable flag, disabled by default) given the regulatory caveats in spec.md §4.
- [ ] Do not implement this section unless explicitly requested — default scope for "Raspberry Pi
      implementation done" is Wi-Fi + BLE only.

## Shared infra
- [ ] RNG: use Python's `secrets`/`os.urandom` (CSPRNG, backed by kernel entropy) — do not use
      `random.random()` for identifier generation.
- [ ] Config: YAML or `.env` file for tunables (rotation interval, jitter, enable flags,
      concurrency, interface names).
- [ ] Run as a `systemd` service so it starts on boot and restarts on crash
      (`Restart=on-failure`).
- [ ] No persistent logging of generated identifiers to disk by default; optional
      `--verbose`/debug flag for interactive troubleshooting only.
- [ ] Handle adapter hot-unplug / init failure gracefully — log and continue on remaining
      radios rather than crashing the whole service.

## Verification
- [ ] Confirm multiple distinct SSIDs appear and rotate every ~1s via a second scanning device
      (`airodump-ng` on a separate machine, or a phone Wi-Fi picker).
- [ ] Confirm BLE address/name rotation via `nRF Connect` or `bluetoothctl scan on`.
- [ ] Soak test 1+ hour as a systemd service; confirm service stays up (or auto-restarts cleanly)
      and CPU/memory stay stable (no runaway growth).
- [ ] Record achieved concurrent-identity counts (Wi-Fi and BLE separately, and combined if
      multiple adapters used); feed back into spec.md §7 if different from estimate.

## Deliverables
- [ ] `raspberrypi/` project (Python package or scripts + optional C helper), with a
      `systemd` unit file.
- [ ] `raspberrypi/README.md`: adapter requirements, setup/install steps, config knobs, and an
      explicit "RTL-SDR is receive-only" callout plus experimental-RF-track opt-in instructions
      if that section was implemented.
