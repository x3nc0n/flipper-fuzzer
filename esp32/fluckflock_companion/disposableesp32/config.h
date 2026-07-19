#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Runtime-tunable configuration. Seeded from Kconfig defaults at boot, then
// overridden by any values already persisted in NVS (namespace "ffcfg").
// Can be changed live (and re-persisted) in the field via the `ffcfg` serial
// console command implemented in console_cfg.c -- no reflash required.
//
// Mirrors the shared config surface described in spec.md section 5.3.
typedef struct {
    uint32_t rotation_interval_ms;
    uint32_t jitter_ms;
    uint8_t  wifi_beacons_per_tick;
    uint8_t  wifi_channel;
    uint32_t no_repeat_window;
    bool     enable_wifi;
    bool     enable_ble;
    bool     verbose_logging;
} fluckflock_config_t;

extern fluckflock_config_t g_ff_config;

// Loads g_ff_config from Kconfig defaults, then applies any NVS overrides
// found under the "ffcfg" namespace. Safe/cheap to call once at boot.
void fluckflock_config_load(void);

// Persists the current in-memory g_ff_config to NVS so changes survive
// reboot without a reflash.
esp_err_t fluckflock_config_save(void);
