/*
 * radio_ble.c — BLE advertisement radio implementation (Flipper Zero).
 *
 * Flipper-only. Uses furi_hal_bt_extra_beacon_* API.
 *
 * ASSUMPTIONS (document for firmware integration):
 *
 * 1. furi_hal_bt_extra_beacon_set_config() accepts a GapExtraBeaconConfig
 *    struct defined in furi_hal_bt.h (or gap.h).  Fields assumed:
 *      - min_adv_interval_ms / max_adv_interval_ms   (uint16_t, ms)
 *      - adv_channel_map                             (uint8_t, 0x07 = all)
 *      - adv_power_level                             (int8_t, dBm; use 0)
 *      - address_type                                (GapAddressType; BLE_GAP_ADDR_TYPE_RANDOM_STATIC = 1)
 *      - address[6]                                  (uint8_t MAC bytes)
 *
 * 2. furi_hal_bt_extra_beacon_set_data() accepts raw advertisement payload
 *    bytes (flags + name AD structures) and their length (uint8_t).
 *
 * 3. furi_hal_bt_extra_beacon_start() / _stop() / _is_active() exist as
 *    documented in the OFW (official firmware) BLE HAL.
 *
 * 4. The extra beacon runs independently of the main BLE stack (Companion app
 *    connection).  If the BLE stack is not started, furi_hal_bt_extra_beacon_*
 *    calls will return false / be no-ops — emit() returns false in that case.
 *
 * If the exact struct layout differs in the firmware version being compiled
 * against, adjust the GapExtraBeaconConfig initializer below.
 */

#include "radio_ble.h"

/* Flipper SDK — only compiled on-device */
#include <furi_hal_bt.h>

/* Advertisement interval: 100 ms min / 200 ms max.
 * BLE spec minimum is 20 ms; 100–200 ms is a common AP/IoT default and
 * keeps duty cycle low enough to coexist with the companion stack. */
#define BLE_ADV_INTERVAL_MIN_MS 100
#define BLE_ADV_INTERVAL_MAX_MS 200

static bool ble_initialised = false;

/* ---- Build a minimal advertisement payload ----
 *
 * Packet layout (AD structures, packed):
 *   [len=2][type=0x01 Flags][value=0x06]          — LE General Discoverable, no BR/EDR
 *   [len=1+namelen][type=0x09 Complete Local Name][name bytes...]
 *   [len=2][type=0x0A TX Power Level][value=0x00]
 *
 * Returns total byte count written into out[].
 * buf_size must be at least 31 (max BLE advertisement payload).
 */
static uint8_t build_adv_payload(uint8_t* out, size_t buf_size, const char* name) {
    size_t name_len = 0;
    while(name[name_len] && name_len < 20) name_len++; /* cap at 20 */

    size_t pos = 0;

    /* Flags */
    if(pos + 3 <= buf_size) {
        out[pos++] = 2;    /* length */
        out[pos++] = 0x01; /* type: Flags */
        out[pos++] = 0x06; /* LE General Discoverable | BR/EDR Not Supported */
    }

    /* Complete Local Name */
    if(name_len > 0 && pos + 2 + name_len <= buf_size) {
        out[pos++] = (uint8_t)(1 + name_len); /* length */
        out[pos++] = 0x09;                    /* type: Complete Local Name */
        for(size_t i = 0; i < name_len; i++) {
            out[pos++] = (uint8_t)name[i];
        }
    }

    /* TX Power Level */
    if(pos + 3 <= buf_size) {
        out[pos++] = 2;    /* length */
        out[pos++] = 0x0A; /* type: TX Power Level */
        out[pos++] = 0x00; /* 0 dBm */
    }

    return (uint8_t)pos;
}

void radio_ble_init(void) {
    /* Nothing to allocate — the extra beacon is a HAL singleton.
     * Just mark initialised so deinit is safe. */
    ble_initialised = true;
}

void radio_ble_deinit(void) {
    if(!ble_initialised) return;
    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_hal_bt_extra_beacon_stop();
    }
    ble_initialised = false;
}

bool radio_ble_emit(const uint8_t mac[6], const char* name) {
    if(!ble_initialised) return false;

    /* Stop previous beacon so we can reconfigure */
    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_hal_bt_extra_beacon_stop();
    }

    /* Build advertisement data */
    uint8_t adv_data[31];
    uint8_t adv_len = build_adv_payload(adv_data, sizeof(adv_data), name);

    /* Push payload */
    if(!furi_hal_bt_extra_beacon_set_data(adv_data, adv_len)) {
        return false;
    }

    /*
     * Configure beacon: address type + address + intervals.
     *
     * NOTE: GapExtraBeaconConfig field names are assumed from OFW source.
     * Adjust if the SDK version in your ufbt toolchain differs.
     */
    GapExtraBeaconConfig config = {
        .min_adv_interval_ms = BLE_ADV_INTERVAL_MIN_MS,
        .max_adv_interval_ms = BLE_ADV_INTERVAL_MAX_MS,
        .adv_channel_map = 0x07, /* all three advertising channels */
        .adv_power_level = 0,    /* 0 dBm */
        /* GapAddressTypeRandom = 1 — random static address per SDK 1.4.3 extra_beacon.h */
        .address_type = GapAddressTypeRandom,
    };
    /* Copy MAC in the byte order furi_hal_bt_extra_beacon expects (little-endian, reversed) */
    for(int i = 0; i < 6; i++) {
        config.address[i] = mac[5 - i];
    }

    if(!furi_hal_bt_extra_beacon_set_config(&config)) {
        return false;
    }

    return furi_hal_bt_extra_beacon_start();
}
