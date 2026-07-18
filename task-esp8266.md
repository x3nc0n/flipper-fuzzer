# Task: ESP8266 implementation

Implements the shared contract in `spec.md` on ESP8266 — Wi-Fi only (no Bluetooth radio), single
core, less RAM/flash than ESP32. Treat this as the constrained/cheap-hardware target; scale
concurrency expectations down accordingly.

## Toolchain
- [ ] Use Arduino-ESP8266 core (best-documented raw-frame TX support via
      `wifi_send_pkt_freedom`) under `esp8266/`, built with PlatformIO.
- [ ] Confirm target module (e.g. ESP-12F/NodeMCU) has enough flash for the build + OTA
      partition if OTA is desired (optional, not required for v1).

## Wi-Fi identifier rotation (only radio available)
- [ ] Set Wi-Fi to a mode permitting raw TX (station mode is sufficient for
      `wifi_send_pkt_freedom`; no need for SoftAP).
- [ ] Implement 802.11 beacon frame builder identical in spirit to the ESP32 one (frame header +
      fixed params + SSID IE + rates IE + DS param IE), with a random source/BSSID MAC.
- [ ] Batch-transmit N beacon frames per rotation tick. Expect a materially lower N than ESP32 due
      to single-core CPU and smaller TX buffer headroom — start conservative (e.g. 8–15) and tune
      up while watching for watchdog resets/heap fragmentation.
- [ ] Rotate BSSID + SSID text per §5.2 of spec.md every tick with jittered interval.
- [ ] Implement SSID generator per spec.md §5.2.1: `ssid_words.h` with `protest_words[]`,
      `everyday_words[]`, `suffixes[]`, `connectors[]` arrays + template picker + weighted mix
      (~70% protest-flavored) + random case variation. Enforce 32-byte SSID cap. Use fixed-size
      char buffers (no dynamic `String` concatenation) per the heap-fragmentation note below.
- [ ] Watch heap fragmentation carefully — build SSID/frame strings using fixed-size buffers, not
      dynamic `String` concatenation, to avoid long-run heap fragmentation crashes typical of
      ESP8266 Arduino sketches.

## No Bluetooth
- [ ] `enable_ble` config flag exists per spec.md but is a no-op on this platform — document
      this clearly in the README so it's not mistaken for a bug.
- [ ] (Optional stretch, out of default scope) If a project wants BT coverage from an ESP8266
      device, that requires an external BT module (e.g. HC-05) — not part of this task's default
      scope; note as a "not implemented" stretch item only if requested later.

## Shared infra
- [ ] RNG: seed from `os_random()`/`RANDOM_REG32` hardware register (do not seed from `millis()`
      alone — too predictable at boot).
- [ ] Config: tunables in a `config.h` (this platform has no NVS-equivalent as convenient as
      ESP32's; a compiled-in config with a couple of `#define`s is acceptable for v1).
- [ ] Status LED: onboard LED blink pattern for alive/broadcasting vs. error, matching the pattern
      semantics used on ESP32 for consistency.
- [ ] No persistent logging of identifiers by default.
- [ ] Enable/verify the software watchdog (`ESP.wdtEnable`) so a stuck TX call reboots instead of
      hanging.

## Verification
- [ ] Confirm multiple distinct SSIDs appear and rotate every ~1s via monitor-mode capture or
      phone Wi-Fi scan.
- [ ] Soak test 1+ hour focused specifically on heap fragmentation / free-heap trend (log free
      heap periodically over serial during test) since this is the most common ESP8266 long-run
      failure mode.
- [ ] Record achieved concurrent-identity count; feed back into spec.md §7 if different from
      estimate.

## Deliverables
- [ ] `esp8266/` PlatformIO project, builds clean.
- [ ] `esp8266/README.md`: build/flash instructions, config knobs, explicit note on BLE
      unavailability.
