/*
 * radio_subghz.h — Sub-GHz (CC1101) radio interface.
 *
 * Flipper-only: depends on furi_hal_subghz.
 * Do NOT include in host test builds.
 *
 * Fenster (chaff_engine.c) calls these functions; signatures are stable.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Initialise Sub-GHz radio (acquires CC1101, loads default preset).
 * Must be called before radio_subghz_emit().
 * Safe to call repeatedly (idempotent after first call).
 */
void radio_subghz_init(void);

/*
 * Release Sub-GHz radio back to idle/sleep state.
 * Safe to call if not initialised.
 */
void radio_subghz_deinit(void);

/*
 * Transmit `len` bytes of payload on the configured ISM frequency.
 *
 * Default frequency: 433.92 MHz (EU/AU/most of world ISM band).
 *
 * REGION CAVEAT: 433.92 MHz is legal in most regions but not all (e.g. the US
 * 433 MHz band has restrictions; 315 MHz or 915 MHz may be more appropriate
 * there). This driver uses the Flipper's built-in region configuration and will
 * refuse to transmit on an out-of-region frequency.  If you need a different
 * default, change SUBGHZ_DEFAULT_FREQ below and recompile.
 *
 * @param payload  Byte array to transmit (e.g. output of chaff_generate_subghz_payload).
 * @param len      Number of bytes. Capped internally at 60 bytes (CC1101 FIFO limit minus
 *                 1-byte length field).
 *
 * @return true  Transmission dispatched successfully.
 * @return false Failure (radio busy, frequency out of region, HAL error, etc.).
 */
bool radio_subghz_emit(const uint8_t* payload, size_t len);
