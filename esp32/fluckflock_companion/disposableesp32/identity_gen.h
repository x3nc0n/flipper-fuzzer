#pragma once

#include <stdint.h>
#include <stddef.h>

#define FF_SSID_MAX_LEN 32      // 802.11 SSID hard limit (spec.md 5.2.1)
// Legacy BLE advertising payload is capped at 31 bytes total. ble_adv.c packs
// flags (3 bytes) + name AD (2 + name_len) + 8-byte mfg_data AD (2 + 8 = 10
// bytes): 3 + 10 + 2 + name_len <= 31  =>  name_len <= 16. Found via on-device
// testing: 20 intermittently overflowed and NimBLE's ble_gap_adv_set_fields()
// rejected it with BLE_HS_EMSGSIZE (rc=4). Use 14 for a safety margin.
#define FF_BLE_NAME_MAX_LEN 14

// Fills `mac` with a random locally-administered, unicast 6-byte address for
// Wi-Fi use: bit 1 of the first octet set (U/L bit), bit 0 cleared (I/G bit),
// per spec.md section 5.2.
void ff_random_mac(uint8_t mac[6]);

// Fills `addr` with a random 6-byte BLE "static random device address".
// NOTE: this intentionally differs from ff_random_mac()'s IEEE 802
// locally-administered-bit convention -- the Bluetooth Core Spec requires a
// static random address to have its two most-significant bits set to `11`
// (Vol 6, Part B, section 1.3.2.1), otherwise NimBLE's ble_hs_id_set_rnd()
// rejects it. See esp32/README.md "Deviations from spec.md" for details.
void ff_random_ble_static_addr(uint8_t addr[6]);

// Fills `out` with `len` random bytes (used for BLE manufacturer data).
void ff_random_bytes(uint8_t *out, size_t len);

// Generates a random SSID into `out` (buffer must be >= FF_SSID_MAX_LEN + 1
// bytes) following the word-list/template/weighting rules in spec.md
// section 5.2.1. Returns the string length (not including NUL).
size_t ff_generate_ssid(char *out, size_t out_cap);

// Generates a random short BLE device name (reuses the SSID generator for
// thematic consistency, truncated to FF_BLE_NAME_MAX_LEN) into `out` (buffer
// must be >= FF_BLE_NAME_MAX_LEN + 1 bytes).
size_t ff_generate_ble_name(char *out, size_t out_cap);
