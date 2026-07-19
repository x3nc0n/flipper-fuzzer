#include <stdbool.h>

#include "config.h"
#include "status_led.h"
#include "wifi_beacon.h"
#include "ble_adv.h"
#include "console_cfg.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"

static const char *TAG = "fluckflock";

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    fluckflock_config_load();

    // Task watchdog: a stuck radio call reboots rather than hangs forever
    // (task-esp32.md "Shared infra" / spec.md 5.4 graceful-degradation
    // intent). CONFIG_ESP_TASK_WDT_INIT=n in sdkconfig.defaults disables the
    // automatic startup init so this is the only place it's configured.
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    err = esp_task_wdt_init(&wdt_config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "task wdt init failed: %s", esp_err_to_name(err));
    }

    status_led_init();
    status_led_set_state(FF_LED_BOOTING);

    console_cfg_start();

    bool wifi_ok = false;
    bool ble_ok = false;

    if (g_ff_config.enable_wifi) {
        if (wifi_beacon_init() == ESP_OK) {
            wifi_beacon_start_task();
            wifi_ok = true;
            ESP_LOGI(TAG, "Wi-Fi beacon chaff active");
        } else {
            ESP_LOGE(TAG, "Wi-Fi beacon init failed -- continuing without it");
        }
    }

    if (g_ff_config.enable_ble) {
        if (ble_adv_init() == ESP_OK) {
            ble_ok = true;
            ESP_LOGI(TAG, "BLE advertisement chaff active");
        } else {
            ESP_LOGE(TAG, "BLE init failed -- continuing without it");
        }
    }

    // Graceful degradation per spec.md 5.4: if one radio fails, keep
    // broadcasting on the other rather than halting.
    if (!wifi_ok && !ble_ok) {
        status_led_set_state(FF_LED_ERROR_BOTH);
    } else if (!wifi_ok) {
        status_led_set_state(FF_LED_ERROR_WIFI);
    } else if (!ble_ok) {
        status_led_set_state(FF_LED_ERROR_BLE);
    } else {
        status_led_set_state(FF_LED_BROADCASTING);
    }
}
