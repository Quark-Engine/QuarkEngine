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

    // ui
    bool (*ui_begin)(const char* title);
    void (*ui_end)();
    void (*ui_text)(const char* text);
    bool (*ui_button)(const char* label);
    bool (*ui_checkbox)(const char* label, bool* value);
    bool (*ui_slider_float)(const char* label, float* value, float min, float max);
    bool (*ui_input_float)(const char* label, float* value);
    bool (*ui_color_edit3)(const char* label, float color[3]);
    void (*ui_separator)();
    void (*ui_same_line)();

    // entity read
    const char* (*entity_get_name)(int index);
    void        (*entity_get_position)(int index, float* x, float* y, float* z);
    void        (*entity_get_rotation)(int index, float* x, float* y, float* z);
    void        (*entity_get_scale)(int index, float* x, float* y, float* z);
    void        (*entity_get_color)(int index, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);

    // entity write
    void (*entity_set_position)(int index, float x, float y, float z);
    void (*entity_set_rotation)(int index, float x, float y, float z);
    void (*entity_set_scale)(int index, float x, float y, float z);
    void (*entity_set_color)(int index, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    void (*entity_set_name)(int index, const char* name);

    // scene
    void (*scene_save)();
    int  (*scene_spawn)(const char* asset_name);
    void (*scene_delete)(int index);
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