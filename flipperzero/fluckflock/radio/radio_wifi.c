/*
 * radio_wifi.c — Wi-Fi Dev Board (ESP32 via UART) implementation (Flipper Zero).
 *
 * Flipper-only.  Uses furi_hal_serial_* for UART communication with the
 * official Flipper Wi-Fi Dev Board.
 *
 * STUB STATUS:
 *   radio_wifi_detect() → always returns false (see TODO below).
 *   radio_wifi_init()   → no-op stub.
 *   radio_wifi_deinit() → no-op stub.
 *   radio_wifi_emit()   → no-op stub, returns false.
 *
 * This is intentional: the Wi-Fi radio path is optional.  When detect() returns
 * false the chaff engine never enables ChaffRadioWifi and these stubs are never
 * called.  Implement the TODOs when the ESP32 UART protocol is finalised.
 *
 * ============================================================================
 * UART PROTOCOL (PROPOSED — finalise with Keaton before implementing):
 *
 * Physical:
 *   - Port:     FuriHalSerialIdUsart (expansion header USART)
 *   - Baud:     115200 8N1
 *   - Connector: GPIO pins 13 (TX) / 14 (RX) on the Flipper
 *
 * Framing (assumed Marauder-compatible ASCII command protocol):
 *   Detection:  Send "help\n" → expect any response containing "Marauder" or
 *               "FluckFlock" within 500 ms.
 *   Beacon cmd: Send "beacon -s <SSID> -b <XX:XX:XX:XX:XX:XX>\n"
 *               → expect "ACK\n" or empty (fire-and-forget).
 *
 * If the ESP32 runs a custom sketch instead of Marauder, replace the framing
 * with whatever protocol that sketch implements.
 * ============================================================================
 *
 * ASSUMPTIONS:
 *   furi_hal_serial_control_acquire(FuriHalSerialIdUsart) returns a handle.
 *   furi_hal_serial_init(handle, baud) opens the port.
 *   furi_hal_serial_tx(handle, data, size) is fire-and-forget (async).
 *   furi_hal_serial_deinit(handle) + furi_hal_serial_control_release(handle) close it.
 */

#include "radio_wifi.h"

/* Flipper SDK — only compiled on-device */
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <string.h>
#include <stdio.h>

/* UART baud rate for the dev board */
#define WIFI_UART_BAUD 115200

static FuriHalSerialHandle* wifi_serial = NULL;

/* ---- Helpers ---- */

/* Format a BSSID into "XX:XX:XX:XX:XX:XX\0" (18 bytes) */
static void bssid_to_str(char* out, const uint8_t bssid[6]) {
    static const char hex[] = "0123456789ABCDEF";
    for(int i = 0; i < 6; i++) {
        out[i * 3]     = hex[(bssid[i] >> 4) & 0xF];
        out[i * 3 + 1] = hex[bssid[i] & 0xF];
        out[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
}

/* ---- Public API ---- */

bool radio_wifi_detect(void) {
    /*
     * TODO: implement actual detection.
     *
     * Sketch:
     *   1. Acquire USART handle.
     *   2. Open at WIFI_UART_BAUD.
     *   3. Send "help\n".
     *   4. Read response with a 500 ms timeout.
     *   5. Return true if response contains "Marauder" or known marker.
     *   6. Close UART and release handle before returning.
     *
     * For now, always return false so the engine disables Wi-Fi cleanly.
     */
    return false;
}

bool radio_wifi_init(void) {
    if(wifi_serial) return true; /* already open */

    /*
     * TODO: open UART to dev board.
     *
     * wifi_serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
     * if(!wifi_serial) return false;
     * furi_hal_serial_init(wifi_serial, WIFI_UART_BAUD);
     */

    return false; /* stub: no board support yet */
}

void radio_wifi_deinit(void) {
    if(!wifi_serial) return;

    /*
     * TODO: close UART.
     *
     * furi_hal_serial_deinit(wifi_serial);
     * furi_hal_serial_control_release(wifi_serial);
     * wifi_serial = NULL;
     */
}

bool radio_wifi_emit(const char* ssid, const uint8_t bssid[6]) {
    if(!wifi_serial) return false;
    if(!ssid || ssid[0] == '\0') return false;

    /*
     * TODO: send beacon command to ESP32.
     *
     * char bssid_str[18];
     * bssid_to_str(bssid_str, bssid);
     *
     * char cmd[80];
     * int len = snprintf(cmd, sizeof(cmd), "beacon -s %s -b %s\n", ssid, bssid_str);
     * if(len < 0 || (size_t)len >= sizeof(cmd)) return false;
     *
     * furi_hal_serial_tx(wifi_serial, (const uint8_t*)cmd, (size_t)len);
     *
     * Optionally wait for "ACK\n" with a short timeout (~100 ms).
     * For fire-and-forget, just return true after tx.
     */

    (void)bssid;        /* intentionally stubbed — board not yet supported */
    (void)bssid_to_str; /* suppress unused warning while stubbed */
    return false;       /* stub: not implemented */
}
