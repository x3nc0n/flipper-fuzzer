#pragma once

#include <gui/scene_manager.h>

/* ---- Scene ID enum (generated from scene_config.h) ---- */

typedef enum {
#define ADD_SCENE(prefix, name) FluckFlockScene##name,
#include "scene_config.h"
#undef ADD_SCENE
    FluckFlockSceneCount,
} FluckFlockScene;

/* ---- Handler table (defined in scenes.c) ---- */

extern const SceneManagerHandlers fluckflock_scene_handlers;

/* ---- Per-scene handler forward declarations ---- */

#define ADD_SCENE(prefix, name)                                                          \
    void fluckflock_scene_##prefix##_on_enter(void* context);                           \
    bool fluckflock_scene_##prefix##_on_event(void* context, SceneManagerEvent event);  \
    void fluckflock_scene_##prefix##_on_exit(void* context);
#include "scene_config.h"
#undef ADD_SCENE
