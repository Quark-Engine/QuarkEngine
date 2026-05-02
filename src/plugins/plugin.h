#pragma once

#ifdef _WIN32
    #define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
    #define PLUGIN_EXPORT extern "C"
#endif

struct PluginContext {
    float delta_time;
    int entity_count;
    int* selected;

    bool (*ui_begin)(const char* title);
    void (*ui_end)();
    void (*ui_text)(const char* text);
    bool (*ui_button)(const char* label);
};

struct Plugin {
    const char* name;
    const char* version;
    void (*on_load)(PluginContext* ctx);
    void (*on_unload)();
    void (*on_update)(PluginContext* ctx);
    void (*on_draw_ui)(PluginContext* ctx);
};

PLUGIN_EXPORT Plugin* get_plugin();