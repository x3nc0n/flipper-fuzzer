#include "config.h"

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "ff_config";
#define NVS_NS "ffcfg"

fluckflock_config_t g_ff_config;

static uint32_t nvs_get_u32_or(nvs_handle_t h, const char *key, uint32_t def) {
    uint32_t v;
    if (nvs_get_u32(h, key, &v) == ESP_OK) return v;
    return def;
}

static uint8_t nvs_get_u8_or(nvs_handle_t h, const char *key, uint8_t def) {
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK) return v;
    return def;
}

void fluckflock_config_load(void) {
    // Kconfig-derived defaults first.
    g_ff_config.rotation_interval_ms  = CONFIG_FLUCKFLOCK_ROTATION_INTERVAL_MS;
    g_ff_config.jitter_ms             = CONFIG_FLUCKFLOCK_JITTER_MS;
    g_ff_config.wifi_beacons_per_tick = CONFIG_FLUCKFLOCK_WIFI_BEACONS_PER_TICK;
    g_ff_config.wifi_channel          = CONFIG_FLUCKFLOCK_WIFI_CHANNEL;
    g_ff_config.no_repeat_window      = CONFIG_FLUCKFLOCK_NO_REPEAT_WINDOW;
#ifdef CONFIG_FLUCKFLOCK_ENABLE_WIFI
    g_ff_config.enable_wifi           = true;
#else
    g_ff_config.enable_wifi           = false;
#endif
#ifdef CONFIG_FLUCKFLOCK_ENABLE_BLE
    g_ff_config.enable_ble            = true;
#else
    g_ff_config.enable_ble            = false;
#endif
#ifdef CONFIG_FLUCKFLOCK_VERBOSE_LOGGING
    g_ff_config.verbose_logging       = true;
#else
    g_ff_config.verbose_logging       = false;
#endif

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved NVS overrides, using Kconfig defaults");
        return;
    }

    g_ff_config.rotation_interval_ms  = nvs_get_u32_or(h, "rot_ms", g_ff_config.rotation_interval_ms);
    g_ff_config.jitter_ms             = nvs_get_u32_or(h, "jit_ms", g_ff_config.jitter_ms);
    g_ff_config.wifi_beacons_per_tick = nvs_get_u8_or(h, "wifi_n", g_ff_config.wifi_beacons_per_tick);
    g_ff_config.wifi_channel          = nvs_get_u8_or(h, "wifi_ch", g_ff_config.wifi_channel);
    g_ff_config.no_repeat_window      = nvs_get_u32_or(h, "no_rep", g_ff_config.no_repeat_window);
    g_ff_config.enable_wifi           = nvs_get_u8_or(h, "en_wifi", g_ff_config.enable_wifi) != 0;
    g_ff_config.enable_ble            = nvs_get_u8_or(h, "en_ble", g_ff_config.enable_ble) != 0;
    g_ff_config.verbose_logging       = nvs_get_u8_or(h, "verbose", g_ff_config.verbose_logging) != 0;

    nvs_close(h);
    ESP_LOGI(TAG, "config loaded (NVS overrides applied where present)");
}

esp_err_t fluckflock_config_save(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    nvs_set_u32(h, "rot_ms", g_ff_config.rotation_interval_ms);
    nvs_set_u32(h, "jit_ms", g_ff_config.jitter_ms);
    nvs_set_u8(h, "wifi_n", g_ff_config.wifi_beacons_per_tick);
    nvs_set_u8(h, "wifi_ch", g_ff_config.wifi_channel);
    nvs_set_u32(h, "no_rep", g_ff_config.no_repeat_window);
    nvs_set_u8(h, "en_wifi", g_ff_config.enable_wifi ? 1 : 0);
    nvs_set_u8(h, "en_ble", g_ff_config.enable_ble ? 1 : 0);
    nvs_set_u8(h, "verbose", g_ff_config.verbose_logging ? 1 : 0);

    err = nvs_commit(h);
    nvs_close(h);
    return err;
}
