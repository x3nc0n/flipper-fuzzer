#include "fluckflock.h"
#include "radio/radio_wifi.h"   // radio_wifi_power_on/off, radio_wifi_detect() — called at startup/teardown

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <string.h>

/* ---- Settings persistence ---- */

static void fluckflock_settings_defaults(FluckFlockApp* app) {
    app->settings.ble_enabled    = true;
    app->settings.subghz_enabled = true;
    app->settings.wifi_enabled   = false;
    app->settings.interval_idx   = FLUCKFLOCK_DEFAULT_INTERVAL_IDX;
}

static void fluckflock_settings_load(FluckFlockApp* app) {
    fluckflock_settings_defaults(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FLUCKFLOCK_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FluckFlockSettingsFile sf;
        if(storage_file_read(file, &sf, sizeof(sf)) == sizeof(sf)) {
            if(sf.magic == FLUCKFLOCK_SETTINGS_MAGIC &&
               sf.version == FLUCKFLOCK_SETTINGS_VERSION) {
                app->settings = sf.data;
                // Clamp interval_idx to valid range
                if(app->settings.interval_idx >= FLUCKFLOCK_INTERVAL_COUNT) {
                    app->settings.interval_idx = FLUCKFLOCK_DEFAULT_INTERVAL_IDX;
                }
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void fluckflock_settings_save(FluckFlockApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, "/ext/apps_data/fluckflock");

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FLUCKFLOCK_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FluckFlockSettingsFile sf = {
            .magic   = FLUCKFLOCK_SETTINGS_MAGIC,
            .version = FLUCKFLOCK_SETTINGS_VERSION,
            .data    = app->settings,
        };
        storage_file_write(file, &sf, sizeof(sf));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ---- ViewDispatcher callbacks ---- */

static bool fluckflock_custom_event_callback(void* context, uint32_t event) {
    FluckFlockApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool fluckflock_navigation_event_callback(void* context) {
    FluckFlockApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ---- Alloc / free ---- */

static FluckFlockApp* fluckflock_app_alloc(void) {
    FluckFlockApp* app = malloc(sizeof(FluckFlockApp));
    furi_assert(app);
    memset(app, 0, sizeof(FluckFlockApp));

    // Open GUI + notification records
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // ViewDispatcher + SceneManager
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager   = scene_manager_alloc(&fluckflock_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fluckflock_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fluckflock_navigation_event_callback);

    // UI modules
    app->submenu             = submenu_alloc();
    app->variable_item_list  = variable_item_list_alloc();
    app->widget              = widget_alloc();

    // Register views
    view_dispatcher_add_view(
        app->view_dispatcher, FluckFlockViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher,
        FluckFlockViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    view_dispatcher_add_view(
        app->view_dispatcher, FluckFlockViewWidget, widget_get_view(app->widget));

    // Attach to GUI
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Chaff engine
    app->chaff_engine = chaff_engine_alloc();

    // Power the Wi-Fi Dev Board (5V OTG), wait for ESP boot, then detect.
    // Blocks ~1.5–2 s total. Board stays powered for the entire app session
    // so injection works during Start Chaff. Powered off in fluckflock_app_free().
    radio_wifi_power_on();
    app->wifi_detected = radio_wifi_detect();

    // Status refresh timer is owned by scene_running; initialised to NULL here.
    // scene_running_on_enter allocates it; scene_running_on_exit frees it.
    app->status_refresh_timer = NULL;

    // Load persisted settings and apply to engine
    fluckflock_settings_load(app);
    chaff_engine_set_radio_enabled(app->chaff_engine, ChaffRadioBLE, app->settings.ble_enabled);
    chaff_engine_set_radio_enabled(
        app->chaff_engine, ChaffRadioSubGhz, app->settings.subghz_enabled);
    if(app->wifi_detected) {
        chaff_engine_set_radio_enabled(
            app->chaff_engine, ChaffRadioWifi, app->settings.wifi_enabled);
    }

    return app;
}

static void fluckflock_app_free(FluckFlockApp* app) {
    furi_assert(app);

    // Stop engine if still running (safety)
    if(chaff_engine_is_running(app->chaff_engine)) {
        chaff_engine_stop(app->chaff_engine);
    }

    // Stop and free the status refresh timer if it was allocated
    // (scene_running_on_exit normally frees it; this guards early teardown)
    if(app->status_refresh_timer) {
        furi_timer_stop(app->status_refresh_timer);
        furi_timer_free(app->status_refresh_timer);
        app->status_refresh_timer = NULL;
    }

    chaff_engine_free(app->chaff_engine);

    // Power off the Wi-Fi Dev Board (5V OTG). Idempotent — safe even if never detected.
    radio_wifi_power_off();

    // Remove views before freeing modules
    view_dispatcher_remove_view(app->view_dispatcher, FluckFlockViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FluckFlockViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, FluckFlockViewWidget);

    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    widget_free(app->widget);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

/* ---- Entry point ---- */

int32_t fluckflock_app(void* p) {
    UNUSED(p);

    FluckFlockApp* app = fluckflock_app_alloc();

    // Start at main menu
    scene_manager_next_scene(app->scene_manager, FluckFlockSceneMainMenu);

    // Run the event loop (blocks until view_dispatcher_stop() is called)
    view_dispatcher_run(app->view_dispatcher);

    // Save settings on clean exit
    fluckflock_settings_save(app);

    fluckflock_app_free(app);

    return 0;
}
