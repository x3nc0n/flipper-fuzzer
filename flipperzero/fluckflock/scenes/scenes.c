#include "scenes.h"

/* SDK 1.4.3: SceneManagerHandlers holds three separate callback arrays,
 * not an array of per-scene structs. Build one array per callback type. */

static const AppSceneOnEnterCallback on_enter_handlers[] = {
#define ADD_SCENE(prefix, name) fluckflock_scene_##prefix##_on_enter,
#include "scene_config.h"
#undef ADD_SCENE
};

static const AppSceneOnEventCallback on_event_handlers[] = {
#define ADD_SCENE(prefix, name) fluckflock_scene_##prefix##_on_event,
#include "scene_config.h"
#undef ADD_SCENE
};

static const AppSceneOnExitCallback on_exit_handlers[] = {
#define ADD_SCENE(prefix, name) fluckflock_scene_##prefix##_on_exit,
#include "scene_config.h"
#undef ADD_SCENE
};

const SceneManagerHandlers fluckflock_scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers  = on_exit_handlers,
    .scene_num         = FluckFlockSceneCount,
};
