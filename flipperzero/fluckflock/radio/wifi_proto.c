/*
 * wifi_proto.c — FluckFlock Wi-Fi companion UART line protocol implementation.
 *
 * Pure C — NO Flipper SDK includes.  Only <stdint.h>, <stddef.h>, <string.h>.
 * Host-testable (Hockney can compile and test this with plain gcc/clang).
 *
 * Owner: McManus (Wireless / RF Dev)
 */

#include "wifi_proto.h"

#include <string.h>

/* Lowercase hex nibble table */
static const char WIFI_PROTO_HEX[] = "0123456789abcdef";

/* ---- Internal helpers ---- */

/**
 * Copy a literal command string into buf, return byte count (excl. '\0').
 * Returns 0 if buf_len is too small (needs len+1 bytes for the '\0').
 */
static size_t proto_copy_literal(char* buf, size_t buf_len, const char* s) {
    size_t len = strlen(s);
    if(!buf || buf_len < len + 1u) return 0u;
    memcpy(buf, s, len + 1u); /* include '\0' */
    return len;
}

/* ---- Public API ---- */

size_t wifi_proto_build_handshake(char* buf, size_t buf_len) {
    return proto_copy_literal(buf, buf_len, WIFI_PROTO_HANDSHAKE_CMD);
}

size_t wifi_proto_build_on(char* buf, size_t buf_len) {
    return proto_copy_literal(buf, buf_len, WIFI_PROTO_ON_CMD);
}

size_t wifi_proto_build_off(char* buf, size_t buf_len) {
    return proto_copy_literal(buf, buf_len, WIFI_PROTO_OFF_CMD);
}

size_t wifi_proto_build_beacon(
    char*         buf,
    size_t        buf_len,
    const char*   ssid,
    const uint8_t bssid[6]) {

    if(!buf || !ssid || !bssid) return 0u;

    size_t ssid_len = strlen(ssid);
    if(ssid_len == 0u || ssid_len > 32u) return 0u;

    /*
     * Frame layout: "FFB " (4) + 12 hex (12) + " " (1) + ssid + "\n" (1) + '\0' (1)
     *              = 19 + ssid_len bytes of buffer required.
     */
    size_t needed = 4u + 12u + 1u + ssid_len + 1u + 1u; /* +1 for '\0' */
    if(buf_len < needed) return 0u;

    size_t pos = 0u;

    buf[pos++] = 'F';
    buf[pos++] = 'F';
    buf[pos++] = 'B';
    buf[pos++] = ' ';

    for(int i = 0; i < 6; i++) {
        buf[pos++] = WIFI_PROTO_HEX[(bssid[i] >> 4) & 0xFu];
        buf[pos++] = WIFI_PROTO_HEX[bssid[i] & 0xFu];
    }

    buf[pos++] = ' ';
    memcpy(buf + pos, ssid, ssid_len);
    pos += ssid_len;
    buf[pos++] = '\n';
    buf[pos]   = '\0';

    return pos; /* bytes written, excluding '\0' */
}
