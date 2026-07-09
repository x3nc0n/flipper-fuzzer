/*
 * radio_ble.h — BLE advertisement radio interface.
 *
 * Flipper-only: depends on furi_hal_bt / extra beacon API.
 * Do NOT include in host test builds.
 *
 * Fenster (chaff_engine.c) calls these functions; signatures are stable.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Initialise BLE advertisement radio.
 * Must be called once before radio_ble_emit().
 * Safe to call repeatedly (idempotent after first call).
 */
void radio_ble_init(void);

/*
 * Tear down BLE advertisement radio and stop any active beacon.
 * Safe to call if not initialised.
 */
void radio_ble_deinit(void);

/*
 * Emit one BLE advertisement using the given MAC and advertisement name.
 *
 * @param mac   6-byte BLE random static address (as produced by chaff_generate_ble_mac).
 * @param name  Null-terminated BLE device name (≤ 20 bytes of payload; caller ensures this).
 *
 * @return true  Advertisement successfully configured and beacon started/refreshed.
 * @return false Failure (BLE stack not ready, HAL call rejected, etc.).
 *
 * Each call stops the previous beacon, reconfigures address + AD data,
 * and restarts it.  The beacon runs until the next call or radio_ble_deinit().
 */
bool radio_ble_emit(const uint8_t mac[6], const char* name);
