#pragma once

#include "esp_err.h"

// Initializes Wi-Fi in STA mode (never connects to anything) purely so
// esp_wifi_80211_tx() raw-frame injection works. Does not run a real access
// point -- see spec.md section 5 / task-esp32.md "Wi-Fi identifier
// rotation".
esp_err_t wifi_beacon_init(void);

// Starts the FreeRTOS task that builds+broadcasts a batch of fake beacon
// frames every rotation tick. Call only after a successful wifi_beacon_init().
void wifi_beacon_start_task(void);
