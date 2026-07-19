#pragma once

#include "esp_err.h"

// Initializes the NimBLE host+controller and starts both the NimBLE host
// FreeRTOS task and the FluckFlock advertisement-rotation task. Actual
// advertising rotation begins once the host reports sync (see ble_adv.c
// ble_app_on_sync) -- this function returns before that happens.
esp_err_t ble_adv_init(void);
