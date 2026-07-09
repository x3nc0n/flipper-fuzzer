#include "../fluckflock.h"
#include <stdio.h>

/* ---- Widget layout helpers ---- */

// Flipper screen: 128 x 64 px. FontSecondary: ~7px tall. FontPrimary: ~10px tall.
#define ROW_TITLE  2
#define ROW_BLE    16
#define ROW_BLE2   25
#define ROW_SG     36
#define ROW_SG2    45
#define ROW_WIFI   56

static void running_rebuild_widget(FluckFlockApp* app) {
    widget_reset(app->widget);

    ChaffStats stats;
    char line[48];

    // Title / status bar
    widget_add_string_element(
        app->widget, 64, ROW_TITLE, AlignCenter, AlignTop, FontPrimary, "RUNNING");

    // BLE
    if(chaff_engine_get_radio_enabled(app->chaff_engine, ChaffRadioBLE)) {
        chaff_engine_get_stats(app->chaff_engine, ChaffRadioBLE, &stats);
        snprintf(line, sizeof(line), "BLE %lu", (unsigned long)stats.emitted_count);
        widget_add_string_element(
            app->widget, 0, ROW_BLE, AlignLeft, AlignTop, FontSecondary, line);
        // Truncate last_ident to fit screen width (~21 chars in FontSecondary)
        widget_add_string_element(
            app->widget, 0, ROW_BLE2, AlignLeft, AlignTop, FontSecondary, stats.last_ident);
    }

    // Sub-GHz
    if(chaff_engine_get_radio_enabled(app->chaff_engine, ChaffRadioSubGhz)) {
        chaff_engine_get_stats(app->chaff_engine, ChaffRadioSubGhz, &stats);
        snprintf(line, sizeof(line), "SG %lu", (unsigned long)stats.emitted_count);
        widget_add_string_element(
            app->widget, 64, ROW_BLE, AlignLeft, AlignTop, FontSecondary, line);
        widget_add_string_element(
            app->widget, 64, ROW_BLE2, AlignLeft, AlignTop, FontSecondary, stats.last_ident);
    }

    // WiFi (if detected and enabled)
    if(app->wifi_detected &&
       chaff_engine_get_radio_enabled(app->chaff_engine, ChaffRadioWifi)) {
        chaff_engine_get_stats(app->chaff_engine, ChaffRadioWifi, &stats);
        snprintf(line, sizeof(line), "WiFi %lu", (unsigned long)stats.emitted_count);
        widget_add_string_element(
            app->widget, 0, ROW_WIFI, AlignLeft, AlignTop, FontSecondary, line);
        // Show truncated last SSID
        widget_add_string_element(
            app->widget, 50, ROW_WIFI, AlignLeft, AlignTop, FontSecondary, stats.last_ident);
    } else if(!app->wifi_detected ||
              !chaff_engine_get_radio_enabled(app->chaff_engine, ChaffRadioWifi)) {
        // Show hint at bottom
        widget_add_string_element(
            app->widget, 64, 63, AlignCenter, AlignBottom, FontSecondary, "Back=Stop");
    }
}

static void status_timer_callback(void* context) {
    FluckFlockApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, FluckFlockCustomEventRunningTick);
}

void fluckflock_scene_running_on_enter(void* context) {
    FluckFlockApp* app = context;

    // Populate widget with initial stats (engine is already started by main_menu scene)
    running_rebuild_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FluckFlockViewWidget);

    // Start periodic UI refresh timer
    furi_assert(app->status_refresh_timer == NULL);
    app->status_refresh_timer =
        furi_timer_alloc(status_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->status_refresh_timer, FLUCKFLOCK_STATUS_REFRESH_MS);
}

bool fluckflock_scene_running_on_event(void* context, SceneManagerEvent event) {
    FluckFlockApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FluckFlockCustomEventRunningTick) {
            running_rebuild_widget(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back = stop chaff and return to main menu
        chaff_engine_stop(app->chaff_engine);
        // Return false — let SceneManager pop this scene automatically
        consumed = false;
    }

    return consumed;
}

void fluckflock_scene_running_on_exit(void* context) {
    FluckFlockApp* app = context;

    // Stop and free the UI refresh timer
    if(app->status_refresh_timer) {
        furi_timer_stop(app->status_refresh_timer);
        furi_timer_free(app->status_refresh_timer);
        app->status_refresh_timer = NULL;
    }

    widget_reset(app->widget);
}
