#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ---- Radio types ---- */

typedef enum {
    ChaffRadioBLE    = 0,
    ChaffRadioSubGhz = 1,
    ChaffRadioWifi   = 2,
    ChaffRadioCount  = 3, // sentinel, always last
} ChaffRadioType;

/* ---- Stats ---- */

typedef struct {
    uint32_t emitted_count;   // total identifiers emitted since last start
    char     last_ident[33];  // last identifier string emitted (null-terminated, for display)
} ChaffStats;

/* ---- Engine (opaque) ---- */

typedef struct ChaffEngine ChaffEngine;

/* Allocate engine. Does not start emission. */
ChaffEngine* chaff_engine_alloc(void);

/* Free engine. Must be stopped first. */
void chaff_engine_free(ChaffEngine* engine);

/* Start chaff emission. Launches internal timer at `interval_ms`. */
void chaff_engine_start(ChaffEngine* engine, uint32_t interval_ms);

/* Stop chaff emission. Timer stops, counters preserved. */
void chaff_engine_stop(ChaffEngine* engine);

/* Returns true if engine is currently emitting. */
bool chaff_engine_is_running(const ChaffEngine* engine);

/* Enable or disable a specific radio. Can be called while running. */
void chaff_engine_set_radio_enabled(ChaffEngine* engine, ChaffRadioType radio, bool enabled);

/* Query whether a radio is enabled. */
bool chaff_engine_get_radio_enabled(const ChaffEngine* engine, ChaffRadioType radio);

/* Get stats for a radio. Caller provides pointer; function fills it. */
void chaff_engine_get_stats(const ChaffEngine* engine, ChaffRadioType radio, ChaffStats* out);

/* Reset all counters to zero. */
void chaff_engine_reset_stats(ChaffEngine* engine);

/*
 * Single step — generate + transmit one round for all enabled radios.
 * Called internally by the timer. Exposed for testing / manual advance.
 */
void chaff_engine_step(ChaffEngine* engine);
