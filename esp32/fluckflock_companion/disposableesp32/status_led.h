#pragma once

// LED heartbeat/status states -- see spec.md section 5.4 ("Optional status
// indication ... for 'alive + broadcasting' vs. error states").
typedef enum {
    FF_LED_BOOTING,
    FF_LED_BROADCASTING,
    FF_LED_ERROR_WIFI,   // BLE only active
    FF_LED_ERROR_BLE,    // Wi-Fi only active
    FF_LED_ERROR_BOTH,   // neither radio came up
} ff_led_state_t;

// Configures the status GPIO and starts the blink-pattern task. Call once
// from app_main() before initializing the radios.
void status_led_init(void);

void status_led_set_state(ff_led_state_t state);

// Called by the Wi-Fi/BLE rotation tasks once they've completed at least one
// successful tick, to flip the LED into the "broadcasting" heartbeat.
void status_led_notify_broadcast_pulse(void);
