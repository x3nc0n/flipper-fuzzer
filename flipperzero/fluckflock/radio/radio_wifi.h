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
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Probe UART for the official Flipper Wi-Fi Dev Board (ESP32).
 *
 * Sends a detection ping and waits up to ~500 ms for a valid response.
 *
 * @return true  Dev board present and responsive.
 * @return false No board, no response, or UART busy.
 *
 * NOTE: Currently stubbed to return false — see TODO in radio_wifi.c.
 * Blocks the calling thread up to ~500 ms. Call once at app startup.
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
