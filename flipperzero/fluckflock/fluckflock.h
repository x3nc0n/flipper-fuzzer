#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "chaff_engine.h"
#include "scenes/scenes.h"

/* ---- Build-time constants ---- */

#define FLUCKFLOCK_DEFAULT_INTERVAL_IDX 1u           // index into fluckflock_interval_values[]
#define FLUCKFLOCK_SETTINGS_PATH "/ext/apps_data/fluckflock/settings.dat"
#define FLUCKFLOCK_SETTINGS_MAGIC   0xF1F2u
#define FLUCKFLOCK_SETTINGS_VERSION 1u
#define FLUCKFLOCK_STATUS_REFRESH_MS 500u

/* ---- Interval preset table (shared with settings scene) ---- */

#define FLUCKFLOCK_INTERVAL_COUNT 5u

static const uint32_t fluckflock_interval_values[FLUCKFLOCK_INTERVAL_COUNT] = {
    500, 1000, 2000, 3000, 5000,
};
static const char* const fluckflock_interval_names[FLUCKFLOCK_INTERVAL_COUNT] = {
    "500ms", "1s", "2s", "3s", "5s",
};

/* ---- View IDs ---- */

typedef enum {
    FluckFlockViewSubmenu,
    FluckFlockViewVariableItemList,
    FluckFlockViewWidget,
} FluckFlockView;

/* ---- Custom events (sent via view_dispatcher_send_custom_event) ---- */

typedef enum {
    /* Main menu submenu indices — must match submenu add order */
    FluckFlockCustomEventMainMenuStart    = 0,
    FluckFlockCustomEventMainMenuSettings = 1,
    FluckFlockCustomEventMainMenuAbout    = 2,
    /* Running scene periodic UI refresh */
    FluckFlockCustomEventRunningTick      = 100,
} FluckFlockCustomEvent;

/* ---- Persistent settings ---- */

typedef struct {
    bool    ble_enabled;
    bool    subghz_enabled;
    bool    wifi_enabled;
    uint8_t interval_idx;   // index into fluckflock_interval_values[]
} FluckFlockSettings;

/* Settings file layout written to flash */
typedef struct {
    uint16_t         magic;
    uint8_t          version;
    FluckFlockSettings data;
} FluckFlockSettingsFile;

/* ---- App struct ---- */

typedef struct {
    Gui*                gui;
    ViewDispatcher*     view_dispatcher;
    SceneManager*       scene_manager;
    Submenu*            submenu;
    VariableItemList*   variable_item_list;
    Widget*             widget;
    NotificationApp*    notifications;

    ChaffEngine*        chaff_engine;
    FluckFlockSettings  settings;

    bool                wifi_detected;
    FuriTimer*          status_refresh_timer;  // fires while running scene is active
} FluckFlockApp;

/* ---- Entry point ---- */

int32_t fluckflock_app(void* p);

/* ---- Settings helpers (implemented in fluckflock.c, used by settings scene) ---- */

void fluckflock_settings_save(FluckFlockApp* app);
