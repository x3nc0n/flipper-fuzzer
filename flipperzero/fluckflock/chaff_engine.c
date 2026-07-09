#include "chaff_engine.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <furi.h>
#include <furi_hal.h>

#include "generators/prng.h"
#include "generators/identifiers.h"
#include "radio/radio_ble.h"
#include "radio/radio_subghz.h"
#include "radio/radio_wifi.h"

/* ---- Sub-GHz defaults ---- */

// radio_subghz_emit() uses the driver's configured frequency internally.
// A future enhancement could add radio_subghz_set_frequency(); for now
// the driver picks a sensible region-legal default (see radio_subghz.h).
#define SUBGHZ_PAYLOAD_LEN 16u

/* ---- Engine struct (private) ---- */

struct ChaffEngine {
    FuriTimer*  timer;
    FuriMutex*  mutex;
    ChaffRng*   rng;
    bool        radio_enabled[ChaffRadioCount];
    ChaffStats  stats[ChaffRadioCount];
    bool        running;
};

/* ---- Timer callback ---- */

static void chaff_engine_timer_callback(void* ctx) {
    // Called from FuriTimer task context. Radio implementations must be non-blocking
    // or complete well within the configured interval (see decision file).
    chaff_engine_step((ChaffEngine*)ctx);
}

/* ---- Lifecycle ---- */

ChaffEngine* chaff_engine_alloc(void) {
    ChaffEngine* engine = malloc(sizeof(ChaffEngine));
    furi_assert(engine);
    memset(engine, 0, sizeof(ChaffEngine));

    // PRNG — seed from hardware entropy
    engine->rng = chaff_rng_alloc();
    furi_assert(engine->rng);
    uint64_t seed = 0;
    furi_hal_random_fill_buf((uint8_t*)&seed, sizeof(seed));
    chaff_rng_seed(engine->rng, seed);

    // Mutex protects stats[] from concurrent reads by UI thread and writes by timer task
    engine->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(engine->mutex);

    // Timer — not started until chaff_engine_start()
    engine->timer = furi_timer_alloc(chaff_engine_timer_callback, FuriTimerTypePeriodic, engine);
    furi_assert(engine->timer);

    // Default radio state: BLE + SubGhz enabled; WiFi off until app detects dev board
    engine->radio_enabled[ChaffRadioBLE]    = true;
    engine->radio_enabled[ChaffRadioSubGhz] = true;
    engine->radio_enabled[ChaffRadioWifi]   = false;

    engine->running = false;

    return engine;
}

void chaff_engine_free(ChaffEngine* engine) {
    furi_assert(engine);
    furi_assert(!engine->running);

    furi_timer_free(engine->timer);
    furi_mutex_free(engine->mutex);
    chaff_rng_free(engine->rng);
    free(engine);
}

void chaff_engine_start(ChaffEngine* engine, uint32_t interval_ms) {
    furi_assert(engine);
    furi_assert(!engine->running);

    // Initialise each enabled radio driver
    if(engine->radio_enabled[ChaffRadioBLE]) {
        radio_ble_init();
    }
    if(engine->radio_enabled[ChaffRadioSubGhz]) {
        radio_subghz_init();
    }
    if(engine->radio_enabled[ChaffRadioWifi]) {
        (void)radio_wifi_init(); // returns bool; failure is non-fatal — emit() will also fail safely
    }

    engine->running = true;
    furi_timer_start(engine->timer, interval_ms);
}

void chaff_engine_stop(ChaffEngine* engine) {
    furi_assert(engine);
    if(!engine->running) return;

    furi_timer_stop(engine->timer);
    engine->running = false;

    // Tear down radio drivers in reverse init order
    if(engine->radio_enabled[ChaffRadioWifi]) {
        radio_wifi_deinit();
    }
    if(engine->radio_enabled[ChaffRadioSubGhz]) {
        radio_subghz_deinit();
    }
    if(engine->radio_enabled[ChaffRadioBLE]) {
        radio_ble_deinit();
    }
}

bool chaff_engine_is_running(const ChaffEngine* engine) {
    furi_assert(engine);
    return engine->running;
}

/* ---- Radio enable/disable ---- */

void chaff_engine_set_radio_enabled(ChaffEngine* engine, ChaffRadioType radio, bool enabled) {
    furi_assert(engine);
    furi_assert((uint32_t)radio < ChaffRadioCount);
    engine->radio_enabled[radio] = enabled;
}

bool chaff_engine_get_radio_enabled(const ChaffEngine* engine, ChaffRadioType radio) {
    furi_assert(engine);
    furi_assert((uint32_t)radio < ChaffRadioCount);
    return engine->radio_enabled[radio];
}

/* ---- Stats ---- */

void chaff_engine_get_stats(const ChaffEngine* engine, ChaffRadioType radio, ChaffStats* out) {
    furi_assert(engine);
    furi_assert((uint32_t)radio < ChaffRadioCount);
    furi_assert(out);

    furi_mutex_acquire(engine->mutex, FuriWaitForever);
    *out = engine->stats[radio];
    furi_mutex_release(engine->mutex);
}

void chaff_engine_reset_stats(ChaffEngine* engine) {
    furi_assert(engine);

    furi_mutex_acquire(engine->mutex, FuriWaitForever);
    memset(engine->stats, 0, sizeof(engine->stats));
    furi_mutex_release(engine->mutex);
}

/* ---- Step ---- */

void chaff_engine_step(ChaffEngine* engine) {
    furi_assert(engine);

    // BLE: emit one advertisement with a fresh random MAC + plausible device name
    if(engine->radio_enabled[ChaffRadioBLE]) {
        uint8_t mac[6];
        char    name[21]; // BLE name ≤ 20 bytes + null

        chaff_generate_ble_mac(mac, engine->rng);
        chaff_generate_ble_name(name, sizeof(name), engine->rng);
        radio_ble_emit(mac, name); // McManus signature: (mac[6], name) — no name_len

        furi_mutex_acquire(engine->mutex, FuriWaitForever);
        engine->stats[ChaffRadioBLE].emitted_count++;
        snprintf(
            engine->stats[ChaffRadioBLE].last_ident,
            sizeof(engine->stats[ChaffRadioBLE].last_ident),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        furi_mutex_release(engine->mutex);
    }

    // Sub-GHz: emit a pseudo-random payload on the driver's configured ISM frequency
    if(engine->radio_enabled[ChaffRadioSubGhz]) {
        uint8_t payload[SUBGHZ_PAYLOAD_LEN];

        chaff_generate_subghz_payload(payload, sizeof(payload), engine->rng);
        radio_subghz_emit(payload, sizeof(payload)); // McManus signature: (payload, len)

        furi_mutex_acquire(engine->mutex, FuriWaitForever);
        engine->stats[ChaffRadioSubGhz].emitted_count++;
        // last_ident for SubGhz: show payload preview as hex
        snprintf(
            engine->stats[ChaffRadioSubGhz].last_ident,
            sizeof(engine->stats[ChaffRadioSubGhz].last_ident),
            "%02X%02X%02X%02X...",
            payload[0], payload[1], payload[2], payload[3]);
        furi_mutex_release(engine->mutex);
    }

    // WiFi: broadcast a beacon with a plausible SSID + vendor BSSID
    if(engine->radio_enabled[ChaffRadioWifi]) {
        uint8_t bssid[6];
        char    ssid[33]; // SSID ≤ 32 bytes + null

        chaff_generate_bssid(bssid, engine->rng);
        size_t ssid_len = chaff_generate_ssid(ssid, sizeof(ssid), engine->rng);
        radio_wifi_emit(ssid, bssid); // McManus signature: (ssid, bssid) — null-terminated, no ssid_len

        furi_mutex_acquire(engine->mutex, FuriWaitForever);
        engine->stats[ChaffRadioWifi].emitted_count++;
        size_t copy_len = ssid_len < 32u ? ssid_len : 32u;
        memcpy(engine->stats[ChaffRadioWifi].last_ident, ssid, copy_len);
        engine->stats[ChaffRadioWifi].last_ident[copy_len] = '\0';
        furi_mutex_release(engine->mutex);
    }
}
