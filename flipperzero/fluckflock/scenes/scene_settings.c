#include "../fluckflock.h"

/* ---- BLE toggle ---- */

static void ble_changed_callback(VariableItem* item) {
    FluckFlockApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
    app->settings.ble_enabled = (idx != 0);
    chaff_engine_set_radio_enabled(app->chaff_engine, ChaffRadioBLE, app->settings.ble_enabled);
}

/* ---- Sub-GHz toggle ---- */

static void subghz_changed_callback(VariableItem* item) {
    FluckFlockApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
    app->settings.subghz_enabled = (idx != 0);
    chaff_engine_set_radio_enabled(
        app->chaff_engine, ChaffRadioSubGhz, app->settings.subghz_enabled);
}

/* ---- WiFi toggle (only visible if dev board detected) ---- */

static void wifi_changed_callback(VariableItem* item) {
    FluckFlockApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
    app->settings.wifi_enabled = (idx != 0);
    chaff_engine_set_radio_enabled(
        app->chaff_engine, ChaffRadioWifi, app->settings.wifi_enabled);
}

/* ---- Interval selector ---- */

static void interval_changed_callback(VariableItem* item) {
    FluckFlockApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= FLUCKFLOCK_INTERVAL_COUNT) idx = FLUCKFLOCK_DEFAULT_INTERVAL_IDX;
    variable_item_set_current_value_text(item, fluckflock_interval_names[idx]);
    app->settings.interval_idx = idx;
}

/* ---- Scene handlers ---- */

void fluckflock_scene_settings_on_enter(void* context) {
    FluckFlockApp* app = context;

    variable_item_list_reset(app->variable_item_list);

    VariableItem* item;

    // BLE
    item = variable_item_list_add(
        app->variable_item_list, "BLE", 2, ble_changed_callback, app);
    variable_item_set_current_value_index(item, app->settings.ble_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, app->settings.ble_enabled ? "ON" : "OFF");

    // Sub-GHz
    item = variable_item_list_add(
        app->variable_item_list, "Sub-GHz", 2, subghz_changed_callback, app);
    variable_item_set_current_value_index(item, app->settings.subghz_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, app->settings.subghz_enabled ? "ON" : "OFF");

    // WiFi (only when dev board is present)
    if(app->wifi_detected) {
        item = variable_item_list_add(
            app->variable_item_list, "Wi-Fi", 2, wifi_changed_callback, app);
        variable_item_set_current_value_index(item, app->settings.wifi_enabled ? 1 : 0);
        variable_item_set_current_value_text(item, app->settings.wifi_enabled ? "ON" : "OFF");
    }

    // Interval
    item = variable_item_list_add(
        app->variable_item_list,
        "Interval",
        FLUCKFLOCK_INTERVAL_COUNT,
        interval_changed_callback,
        app);
    variable_item_set_current_value_index(item, app->settings.interval_idx);
    variable_item_set_current_value_text(
        item, fluckflock_interval_names[app->settings.interval_idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, FluckFlockViewVariableItemList);
}

bool fluckflock_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fluckflock_scene_settings_on_exit(void* context) {
    FluckFlockApp* app = context;
    // Settings already updated live via callbacks; persist now
    fluckflock_settings_save(app);
    variable_item_list_reset(app->variable_item_list);
}
