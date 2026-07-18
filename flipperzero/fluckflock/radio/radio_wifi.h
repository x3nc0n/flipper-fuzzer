/*
 * radio_wifi.h — Wi-Fi Dev Board (ESP32 via UART) interface.
 *
 * Flipper-only: depends on furi_hal_serial / UART to the official dev board.
 * Do NOT include in host test builds.
 *
 * Fenster (chaff_engine.c) calls these functions; McManus owns this file.
 * Signatures are stable — do not change without a Keaton-gated decision.
 *
 * DECISION (2026-07-09, McManus): emit function named radio_wifi_emit() to be
 * consistent with radio_ble_emit() / radio_subghz_emit() naming convention.
 * ssid_len parameter dropped — callers use strlen() or know their buffer length.
 * radio_wifi_init() returns bool (unlike ble/subghz void inits) because the
 * UART open can fail independently of detect().  See mcmanus-radio-interface.md.
 *
 * DECISION (2026-07-18, McManus): Added radio_wifi_power_on() / _power_off() to
 * manage the Flipper 5V OTG rail that powers the Wi-Fi Dev Board.  The board is
 * off by default; callers MUST power it on and let it boot before calling detect().
 * See .squad/decisions/inbox/mcmanus-wifi-otg-power.md.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Enable the Flipper 5V OTG power rail to the Wi-Fi Dev Board and block until
 * the ESP32-S2 has finished booting.
 *
 * Idempotent: safe to call when OTG is already on (will NOT re-assert or re-wait).
 * Must be called BEFORE radio_wifi_detect().
 * Must be matched by radio_wifi_power_off() on app teardown.
 */
void radio_wifi_power_on(void);

/*
 * Disable the Flipper 5V OTG power rail to the Wi-Fi Dev Board.
 *
 * Idempotent: safe to call when OTG is already off.
 * Call during app teardown after radio_wifi_deinit().
 */
void radio_wifi_power_off(void);

/*
 * Probe UART for the official Flipper Wi-Fi Dev Board (ESP32).
 *
 * Sends a detection ping with up to WIFI_DETECT_ATTEMPTS retries; each attempt
 * waits up to WIFI_DETECT_TIMEOUT_MS ms.  A leading '\n' flush is sent on the
 * first attempt to clear the ESP32 UART rx buffer post-boot.
 *
 * @return true  Dev board present and responsive.
 * @return false No board, no response after all attempts, or UART busy.
 *
 * Call radio_wifi_power_on() first; detect() assumes the board is already
 * powered and booted.  Blocks the calling thread (up to ~1500 ms worst case).
 */
bool radio_wifi_detect(void);

/*
 * Initialise the Wi-Fi Dev Board UART link.
 * Must only be called if radio_wifi_detect() returned true.
 * Must be followed by radio_wifi_deinit() before app exit.
 *
 * @return true  UART opened and board ready.
 * @return false UART open failed.
 */
bool radio_wifi_init(void);

/*
 * Release the Wi-Fi Dev Board UART link.
 * Safe to call if not initialised.
 */
void radio_wifi_deinit(void);

/*
 * Command the ESP32 to broadcast one Wi-Fi beacon frame.
 *
 * @param ssid   Null-terminated SSID string (1–32 bytes, printable UTF-8).
 * @param bssid  6-byte BSSID (globally-administered unicast, from chaff_generate_bssid).
 *
 * @return true  Command sent; ESP32 acknowledged.
 * @return false UART write failed, no ACK, or not initialised.
 */
bool radio_wifi_emit(const char* ssid, const uint8_t bssid[6]);
