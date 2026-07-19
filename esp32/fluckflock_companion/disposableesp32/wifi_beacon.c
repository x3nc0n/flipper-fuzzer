#include "wifi_beacon.h"
#include "config.h"
#include "identity_gen.h"
#include "dedupe.h"
#include "status_led.h"

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ff_wifi";

#define BEACON_BUF_MAX 128

static dedupe_ring_t s_dedupe;

// Builds one 802.11 beacon management frame (24-byte MAC header + fixed
// params + SSID/Rates/DS-Parameter information elements) for `ssid` from
// `mac`, on `channel`. Intentionally beacon-only -- no deauth/disassoc
// frames are ever constructed, per spec.md section 3 non-goals. Returns the
// frame length written to `out` (caller-provided buffer, >= BEACON_BUF_MAX).
static size_t beacon_build(uint8_t *out, const uint8_t mac[6],
                           const char *ssid, size_t ssid_len, uint8_t channel) {
    size_t i = 0;

    // --- 802.11 MAC header (24 bytes) ---
    out[i++] = 0x80; out[i++] = 0x00;   // Frame Control: Beacon (type=mgmt, subtype=8)
    out[i++] = 0x00; out[i++] = 0x00;   // Duration
    memset(&out[i], 0xFF, 6); i += 6;   // Destination: broadcast
    memcpy(&out[i], mac, 6); i += 6;    // Source address
    memcpy(&out[i], mac, 6); i += 6;    // BSSID
    out[i++] = 0x00; out[i++] = 0x00;   // Seq-ctl (auto-filled by driver, en_sys_seq=true)

    // --- Frame body ---
    memset(&out[i], 0x00, 8); i += 8;   // Timestamp (filled in by radio on TX)
    out[i++] = 0x64; out[i++] = 0x00;   // Beacon interval: 100 TU (~102.4ms)

    uint16_t cap = 0x0001;                 // ESS
    if (esp_random() % 2) cap |= 0x0010;   // randomly flag "Privacy" for realism
    out[i++] = (uint8_t)(cap & 0xFF);
    out[i++] = (uint8_t)(cap >> 8);

    // SSID IE
    out[i++] = 0x00; out[i++] = (uint8_t)ssid_len;
    memcpy(&out[i], ssid, ssid_len); i += ssid_len;

    // Supported Rates IE (1,2,5.5,11 basic; 18,24,36,54 not)
    static const uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
    out[i++] = 0x01; out[i++] = sizeof(rates);
    memcpy(&out[i], rates, sizeof(rates)); i += sizeof(rates);

    // DS Parameter Set IE (channel)
    out[i++] = 0x03; out[i++] = 0x01;
    out[i++] = channel;

    return i;
}

esp_err_t wifi_beacon_init(void) {
    esp_err_t err;

    // esp_netif_init()/event loop are required by the Wi-Fi driver internally
    // (it posts WIFI_EVENT_* to the default loop) even though we never
    // create an actual netif/IP interface -- we only ever use raw-frame TX.
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;

    err = esp_wifi_start();
    if (err != ESP_OK) return err;

    err = esp_wifi_set_channel(g_ff_config.wifi_channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_channel failed: %s", esp_err_to_name(err));
    }

    dedupe_init(&s_dedupe, g_ff_config.no_repeat_window);
    ESP_LOGI(TAG, "Wi-Fi beacon injection ready on channel %d", g_ff_config.wifi_channel);
    return ESP_OK;
}

static void wifi_beacon_task(void *arg) {
    (void)arg;
    esp_task_wdt_add(NULL);
    uint8_t frame[BEACON_BUF_MAX];

    while (1) {
        uint32_t jitter = g_ff_config.jitter_ms ? (esp_random() % g_ff_config.jitter_ms) : 0;
        uint32_t tick_budget_ms = g_ff_config.rotation_interval_ms + jitter;
        uint8_t n = g_ff_config.wifi_beacons_per_tick ? g_ff_config.wifi_beacons_per_tick : 1;
        uint32_t per_frame_delay_ms = tick_budget_ms / (n + 1); // leave headroom

        for (uint8_t k = 0; k < n; k++) {
            uint8_t mac[6];
            char ssid[FF_SSID_MAX_LEN + 1];
            size_t ssid_len = 0;

            // Retry a handful of times if we happen to roll a repeat within
            // the no-repeat window (spec.md section 5.1).
            for (int attempt = 0; attempt < 8; attempt++) {
                ff_random_mac(mac);
                ssid_len = ff_generate_ssid(ssid, sizeof(ssid));

                uint8_t hbuf[6 + FF_SSID_MAX_LEN];
                memcpy(hbuf, mac, 6);
                memcpy(hbuf + 6, ssid, ssid_len);
                uint32_t h = dedupe_hash(hbuf, 6 + ssid_len);
                if (dedupe_check_and_add(&s_dedupe, h)) break;
            }

            size_t len = beacon_build(frame, mac, ssid, ssid_len, g_ff_config.wifi_channel);
            esp_err_t tx_err = esp_wifi_80211_tx(WIFI_IF_STA, frame, len, true);
            if (tx_err != ESP_OK) {
                ESP_LOGW(TAG, "80211_tx failed: %s", esp_err_to_name(tx_err));
            } else if (g_ff_config.verbose_logging) {
                ESP_LOGI(TAG, "beacon SSID=\"%s\" MAC=%02x:%02x:%02x:%02x:%02x:%02x",
                         ssid, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }

            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(per_frame_delay_ms > 0 ? per_frame_delay_ms : 1));
        }

        status_led_notify_broadcast_pulse();
    }
}

void wifi_beacon_start_task(void) {
    xTaskCreate(wifi_beacon_task, "ff_wifi_beacon", 4096, NULL, 5, NULL);
}
