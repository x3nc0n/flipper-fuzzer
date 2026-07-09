/*
 * wifi_proto.h — FluckFlock Wi-Fi companion UART line protocol (host-testable).
 *
 * Pure C — NO Flipper SDK includes.  Only <stdint.h>, <stddef.h>, <string.h>,
 * <stdio.h>.  Can be compiled and unit-tested on any host (Hockney's domain).
 *
 * ============================================================================
 * PROTOCOL SUMMARY (ASCII, newline-terminated, 115200 8N1)
 *
 *  Flipper → ESP32          ESP32 → Flipper    Purpose
 *  ────────────────────     ─────────────────  ──────────────────────────────
 *  FF?\n                    FF!v1\n            Handshake / board detection
 *  FFON\n                   OK\n               Enter 802.11 raw-injection mode
 *  FFOFF\n                  OK\n               Exit injection mode (power-save)
 *  FFB <bssid12> <ssid>\n   (none)             Inject one 802.11 beacon frame
 *
 * Field details:
 *   bssid12 — 12 lowercase hex chars, no separators (e.g. "aabbccddeeff")
 *   ssid    — raw SSID string, 1–32 printable ASCII/UTF-8 chars, no newlines
 *
 * The Flipper is always the host (initiator).  Beacon commands are
 * fire-and-forget; no ACK is expected from the companion for FFB.
 * ============================================================================
 *
 * Owner: McManus (Wireless / RF Dev)
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Protocol string constants ---- */

/** Flipper→ESP32: detect/handshake ping */
#define WIFI_PROTO_HANDSHAKE_CMD          "FF?\n"

/** ESP32→Flipper: handshake acknowledgement prefix (followed by version) */
#define WIFI_PROTO_HANDSHAKE_REPLY_PREFIX "FF!"

/** Full expected handshake reply from companion v1 firmware */
#define WIFI_PROTO_HANDSHAKE_REPLY_V1     "FF!v1\n"

/** Flipper→ESP32: enable 802.11 raw-injection mode */
#define WIFI_PROTO_ON_CMD                 "FFON\n"

/** Flipper→ESP32: disable injection mode */
#define WIFI_PROTO_OFF_CMD                "FFOFF\n"

/** ESP32→Flipper: generic success reply for ON/OFF commands */
#define WIFI_PROTO_OK_REPLY               "OK\n"

/** Maximum length (bytes, including '\n', excluding '\0') of a beacon command.
 *  "FFB " (4) + 12 hex (12) + " " (1) + max SSID 32 (32) + "\n" (1) = 50. */
#define WIFI_PROTO_BEACON_CMD_MAX_LEN     50u

/* ---- Protocol frame builders ---- */

/**
 * Write the handshake ping command into buf.
 *
 * @param buf      Destination buffer (≥5 bytes for "FF?\n\0").
 * @param buf_len  Size of buf in bytes.
 * @return         Number of bytes written (excluding '\0'), or 0 on error.
 */
size_t wifi_proto_build_handshake(char* buf, size_t buf_len);

/**
 * Write the injection-enable command into buf.
 *
 * @param buf      Destination buffer (≥6 bytes for "FFON\n\0").
 * @param buf_len  Size of buf in bytes.
 * @return         Number of bytes written (excluding '\0'), or 0 on error.
 */
size_t wifi_proto_build_on(char* buf, size_t buf_len);

/**
 * Write the injection-disable command into buf.
 *
 * @param buf      Destination buffer (≥7 bytes for "FFOFF\n\0").
 * @param buf_len  Size of buf in bytes.
 * @return         Number of bytes written (excluding '\0'), or 0 on error.
 */
size_t wifi_proto_build_off(char* buf, size_t buf_len);

/**
 * Build a beacon injection command: "FFB <bssid12> <ssid>\n"
 *
 * bssid is formatted as 12 lowercase hex chars with no separators.
 * ssid must be 1–32 bytes (null-terminated), printable, no newlines.
 *
 * @param buf      Destination buffer (WIFI_PROTO_BEACON_CMD_MAX_LEN + 1 recommended).
 * @param buf_len  Size of buf in bytes.
 * @param ssid     Null-terminated SSID string.
 * @param bssid    6-byte BSSID array (big-endian / network byte order).
 * @return         Number of bytes written (excluding '\0'), or 0 on error.
 */
size_t wifi_proto_build_beacon(
    char*          buf,
    size_t         buf_len,
    const char*    ssid,
    const uint8_t  bssid[6]);

#ifdef __cplusplus
}
#endif
