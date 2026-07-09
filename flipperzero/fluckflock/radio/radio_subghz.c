/*
 * radio_subghz.c — Sub-GHz (CC1101) radio implementation (Flipper Zero).
 *
 * Flipper-only. Uses furi_hal_subghz_* HAL (SDK 1.4.3).
 *
 * SDK 1.4.3 NOTES:
 *
 * 1. furi_hal_subghz_acquire() / furi_hal_subghz_release() do NOT exist in
 *    this SDK version.  Exclusive CC1101 access is ensured by calling
 *    furi_hal_subghz_reset() + furi_hal_subghz_idle() before configuration
 *    and furi_hal_subghz_idle() + furi_hal_subghz_sleep() after use.
 *
 * 2. furi_hal_subghz_load_preset() and FuriHalSubGhzPreset DO NOT EXIST in
 *    SDK 1.4.3.  Use furi_hal_subghz_load_custom_preset(preset_data) with a
 *    register table from <lib/subghz/devices/cc1101_configs.h>.
 *
 * 3. furi_hal_subghz_set_frequency_and_path(uint32_t) selects the antenna
 *    path automatically and returns the actual PLL frequency (0 = rejected).
 *
 * 4. furi_hal_subghz_write_packet(data, size) writes to the TX FIFO.
 *    furi_hal_subghz_tx() strobes TX mode (returns false if out-of-region).
 *
 * REGION CAVEAT:
 *   Default frequency is 433.92 MHz. The Flipper region lock enforces legal
 *   transmit restrictions.  Do not bypass furi_hal_subghz_set_frequency_and_path()
 *   with raw register writes — that would circumvent the region check.
 */

#include "radio_subghz.h"

/* Flipper SDK — only compiled on-device */
#include <furi_hal_subghz.h>
/* OOK 270 kHz async preset register table (SDK 1.4.3) */
#include <lib/subghz/devices/cc1101_configs.h>

/* Default ISM frequency (Hz).  Common in EU/AU/most non-US regions.
 * For North America, consider 315000000 or 915000000. */
#define SUBGHZ_DEFAULT_FREQ 433920000UL

/* CC1101 FIFO is 64 bytes; reserve 1 byte for the length field. */
#define SUBGHZ_MAX_PAYLOAD 60

static bool subghz_initialised = false;

void radio_subghz_init(void) {
    if(subghz_initialised) return;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();

    /* SDK 1.4.3: load OOK 270 kHz async preset via register table. */
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_270khz_async_regs);

    subghz_initialised = true;
}

void radio_subghz_deinit(void) {
    if(!subghz_initialised) return;
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    subghz_initialised = false;
}

bool radio_subghz_emit(const uint8_t* payload, size_t len) {
    if(!subghz_initialised) return false;
    if(!payload || len == 0) return false;

    /* Clamp to FIFO limit */
    if(len > SUBGHZ_MAX_PAYLOAD) len = SUBGHZ_MAX_PAYLOAD;

    /* Set frequency; the HAL enforces region restrictions internally.
     * A return value of 0 means the frequency was rejected. */
    uint32_t actual_freq = furi_hal_subghz_set_frequency_and_path(SUBGHZ_DEFAULT_FREQ);
    if(actual_freq == 0) {
        /* Out of region or HAL rejected — do not transmit */
        return false;
    }

    /* Write packet to CC1101 TX FIFO, then strobe TX.
     * furi_hal_subghz_tx() returns false if region lock blocks transmission. */
    furi_hal_subghz_write_packet(payload, (uint8_t)len);
    bool ok = furi_hal_subghz_tx();

    /* Return chip to idle after burst to respect duty cycle */
    furi_hal_subghz_idle();

    return ok;
}
