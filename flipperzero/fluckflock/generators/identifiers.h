#pragma once

#include <stdint.h>
#include <stddef.h>

/* ---- PRNG handle (defined in prng.h) ---- */

typedef struct ChaffRng ChaffRng;

/* ---- Generator functions ---- */

/*
 * Generate a plausible Wi-Fi SSID.
 * Writes into `buf` (up to `buf_len` bytes including null terminator).
 * Returns the length of the SSID (excluding null terminator).
 */
size_t chaff_generate_ssid(char* buf, size_t buf_len, ChaffRng* rng);

/*
 * Generate a plausible BSSID (6 bytes).
 * First 3 bytes: OUI from built-in vendor table.
 * Last 3 bytes: random.
 * Unicast bit set (bit 0 of out[0] = 0), locally-administered bit clear (bit 1 of out[0] = 0).
 */
void chaff_generate_bssid(uint8_t out[6], ChaffRng* rng);

/*
 * Generate a BLE random MAC address (6 bytes).
 * Locally-administered bit SET (bit 1 of out[0] = 1).
 * Unicast bit CLEAR (bit 0 of out[0] = 0).
 */
void chaff_generate_ble_mac(uint8_t out[6], ChaffRng* rng);

/*
 * Generate a plausible BLE advertisement name.
 * Writes into `buf` (up to `buf_len` bytes including null terminator).
 * Returns the length of the name (excluding null terminator).
 */
size_t chaff_generate_ble_name(char* buf, size_t buf_len, ChaffRng* rng);

/*
 * Generate a pseudo-random Sub-GHz payload.
 * Fills `buf` with `len` random bytes.
 */
void chaff_generate_subghz_payload(uint8_t* buf, size_t len, ChaffRng* rng);
