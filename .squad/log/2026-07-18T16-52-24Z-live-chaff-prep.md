# Session Log: Live-Chaff-Prep (2026-07-18T16:52:24Z)

**Spawned:** Fenster (Embedded Firmware Dev) + McManus (Wireless/RF Dev)  
**Batch:** Live-Session Launch Preparation  
**User:** x3nc0n (John Spaid)  

## Objective

Prepare live-session validation procedures for FluckFlock chaff emission on Flipper Zero + Wi-Fi dev board.

## Outcomes

### Fenster

- FAP deployment checklist (USB-UART Bridge exit, `python -m ufbt launch`, auto-launch flow)
- Running-screen success criteria (BLE/SG/WiFi counters incrementing, identifiers rotating)
- Settings Wi-Fi toggle as startup handshake gate
- Failure-mode guidance (board not powered → re-seat + power-cycle)
- Handshake verification complete (FF? → FF!v1, FFON → OK, FFOFF → OK via USB-UART Bridge)

### McManus

- RF signature documentation (SSID templates, BSSID OUI table, BLE name pools, BLE MAC bit rules, Sub-GHz frequency/modulation)
- Baseline Wi-Fi snapshot (7 pre-chaff SSIDs, `williamswifi` identified as real neighbour on ch6)
- 3-step validation playbook (netsh-diff for Wi-Fi, nRF Connect/LightBlue for BLE, optional 2nd Flipper/RTL-SDR for Sub-GHz)
- Quick-reference cheat sheet

## Status

- Fenster: Ready for user live-session deployment
- McManus: Ready for user external validation
- Keaton: platformio.ini + main.cpp review pending before merge

## Notes

- Live-session results NOT yet reported by user (test in progress)
- Both agents produced reusable reference documents for future validation runs
- Fenster history and McManus history merged into main decisions.md

## Next (User-Driven)

1. Deploy FAP via `python -m ufbt launch`
2. Run live chaff, observe counters + rotating identifiers
3. Run netsh-diff one-liner to confirm Wi-Fi SSIDs on ch6
4. (Optional) Use phone app / RTL-SDR for deeper RF validation
5. Report results to team
