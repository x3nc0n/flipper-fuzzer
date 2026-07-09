#include "../fluckflock.h"

typedef enum {
    FluckFlockMainMenuItemStart    = 0,
    FluckFlockMainMenuItemSettings = 1,
    FluckFlockMainMenuItemAbout    = 2,
} FluckFlockMainMenuItem;

static void main_menu_callback(void* context, uint32_t index) {
    FluckFlockApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void fluckflock_scene_main_menu_on_enter(void* context) {
    FluckFlockApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "FluckFlock");
    submenu_add_item(
        app->submenu, "Start Chaff", FluckFlockMainMenuItemStart,
        main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", FluckFlockMainMenuItemSettings,
        main_menu_callback, app);
    submenu_add_item(
        app->submenu, "About", FluckFlockMainMenuItemAbout,
        main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FluckFlockViewSubmenu);
}

bool fluckflock_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    FluckFlockApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch((FluckFlockMainMenuItem)event.event) {
        case FluckFlockMainMenuItemStart:
            chaff_engine_reset_stats(app->chaff_engine);
            chaff_engine_start(
                app->chaff_engine,
                fluckflock_interval_values[app->settings.interval_idx]);
            scene_manager_next_scene(app->scene_manager, FluckFlockSceneRunning);
            consumed = true;
            break;
        case FluckFlockMainMenuItemSettings:
            scene_manager_next_scene(app->scene_manager, FluckFlockSceneSettings);
            consumed = true;
            break;
        case FluckFlockMainMenuItemAbout:
            scene_manager_next_scene(app->scene_manager, FluckFlockSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void fluckflock_scene_main_menu_on_exit(void* context) {
    FluckFlockApp* app = context;
    submenu_reset(app->submenu);
}
