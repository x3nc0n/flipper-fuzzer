#include "../fluckflock.h"

// Disclaimer text from spec, broken to fit 128px screen at FontSecondary (~21 chars / line).
// Users navigate back to dismiss.

void fluckflock_scene_about_on_enter(void* context) {
    FluckFlockApp* app = context;

    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "FluckFlock v1.0");

    widget_add_string_element(
        app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary,
        "Wireless chaff broadcaster");
    widget_add_string_element(
        app->widget, 0, 23, AlignLeft, AlignTop, FontSecondary,
        "for anti-tracking research.");

    widget_add_string_element(
        app->widget, 0, 34, AlignLeft, AlignTop, FontSecondary,
        "For research use only.");
    widget_add_string_element(
        app->widget, 0, 43, AlignLeft, AlignTop, FontSecondary,
        "Comply with local radio");
    widget_add_string_element(
        app->widget, 0, 52, AlignLeft, AlignTop, FontSecondary,
        "emissions laws.");

    view_dispatcher_switch_to_view(app->view_dispatcher, FluckFlockViewWidget);
}

bool fluckflock_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fluckflock_scene_about_on_exit(void* context) {
    FluckFlockApp* app = context;
    widget_reset(app->widget);
}
