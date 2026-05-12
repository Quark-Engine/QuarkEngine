#include "plugin.h"
#include <cstdio>

void on_load(PluginContext* ctx) {}
void on_unload() {}
void on_update(PluginContext* ctx) {}

void on_draw_ui(PluginContext* ctx) {
    if (ctx->ui_begin("My Plugin")) {
        int sel = *ctx->selected;
        if (sel >= 0) {
            ctx->ui_text(ctx->entity_get_name(sel));
            ctx->ui_separator();

            float x, y, z;
            ctx->entity_get_position(sel, &x, &y, &z);
            if (ctx->ui_slider_float("Y Position", &y, -50.0f, 50.0f))
                ctx->entity_set_position(sel, x, y, z);

            float color[3] = {1, 0, 0};
            if (ctx->ui_color_edit3("Color", color))
                ctx->entity_set_color(sel,
                    (unsigned char)(color[0]*255),
                    (unsigned char)(color[1]*255),
                    (unsigned char)(color[2]*255), 255);
        }

        if (ctx->ui_button("Spawn Cube"))
            ctx->scene_spawn("Cube");

        if (ctx->ui_button("Save Scene"))
            ctx->scene_save();
    }
    ctx->ui_end();
}

static Plugin info {
    "MyPlugin", "0.1",
    on_load, on_unload, on_update, on_draw_ui
};

PLUGIN_EXPORT Plugin* get_plugin() { return &info; }