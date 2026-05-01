#pragma once
#include "raylib.h"
#include "scene.h"
#include "entity.h"

#define PLUGIN_API extern "C"

struct PluginContext {
    Scene* scene;
    int* selected;
    float delta_time;
};

struct Plugin {
    const char* name;
    const char* version;
    void (*on_load)(PluginContext* ctx);
    void (*on_unload)();
    void (*on_update)(PluginContext* ctx);
    void (*on_draw_ui)(PluginContext* ctx);
};

PLUGIN_API Plugin* get_plugin();