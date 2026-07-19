#include "ble_adv.h"
#include "config.h"
#include "identity_gen.h"
#include "dedupe.h"
#include "status_led.h"

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ff_ble";

static SemaphoreHandle_t s_synced_sem;
static dedupe_ring_t s_dedupe;

// NimBLE requires a GAP event callback pointer even for non-connectable
// advertising; we don't expect connection events since we never advertise
// connectable, but log anything unexpected for debugging.
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
    (void)arg;
    if (event->type != BLE_GAP_EVENT_ADV_COMPLETE) {
        ESP_LOGD(TAG, "unexpected gap event type=%d", event->type);
    }
    return 0;
}

// Rotates BLE identity per spec.md section 5.2: new random address, new
// advertised name, new manufacturer-data payload, every tick.
static void ble_adv_rotate(void) {
    ble_gap_adv_stop(); // ignore rc -- fine if advertising wasn't running yet

    uint8_t addr[6];
    char name[FF_BLE_NAME_MAX_LEN + 1];
    uint8_t mfg[8];

    for (int attempt = 0; attempt < 8; attempt++) {
        ff_random_ble_static_addr(addr);
        ff_generate_ble_name(name, sizeof(name));
        ff_random_bytes(mfg, sizeof(mfg));

        uint8_t hbuf[6 + FF_BLE_NAME_MAX_LEN];
        size_t name_len = strlen(name);
        memcpy(hbuf, addr, 6);
        memcpy(hbuf + 6, name, name_len);
        uint32_t h = dedupe_hash(hbuf, 6 + name_len);
        if (dedupe_check_and_add(&s_dedupe, h)) break;
    }

    int rc = ble_hs_id_set_rnd(addr);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_hs_id_set_rnd failed rc=%d", rc);
        return;
    }

    ble_svc_gap_device_name_set(name);

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    fields.mfg_data = mfg;
    fields.mfg_data_len = sizeof(mfg);

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_adv_set_fields failed rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params,
                           ble_gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_adv_start failed rc=%d", rc);
        return;
    }

    if (g_ff_config.verbose_logging) {
        ESP_LOGI(TAG, "BLE adv name=\"%s\" addr=%02x:%02x:%02x:%02x:%02x:%02x",
                 name, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    }
}

static void ble_app_on_sync(void) {
    xSemaphoreGive(s_synced_sem);
}

static void ble_host_task(void *param) {
    (void)param;
    nimble_port_run(); // blocks until nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

static void ble_rotation_task(void *arg) {
    (void)arg;
    xSemaphoreTake(s_synced_sem, portMAX_DELAY);
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "NimBLE synced, starting advertisement rotation");

    while (1) {
        ble_adv_rotate();
        status_led_notify_broadcast_pulse();

        uint32_t jitter = g_ff_config.jitter_ms ? (esp_random() % g_ff_config.jitter_ms) : 0;
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(g_ff_config.rotation_interval_ms + jitter));
    }
}

esp_err_t ble_adv_init(void) {
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_svc_gap_init();
    ble_svc_gap_device_name_set("fluckflock");
    ble_hs_cfg.sync_cb = ble_app_on_sync;

    dedupe_init(&s_dedupe, g_ff_config.no_repeat_window);
    s_synced_sem = xSemaphoreCreateBinary();
    if (s_synced_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nimble_port_freertos_init(ble_host_task);
    xTaskCreate(ble_rotation_task, "ff_ble_rot", 4096, NULL, 5, NULL);

    return ESP_OK;
}
