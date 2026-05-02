#include "plugin.h"
#include <cstdio>

void on_load(PluginContext* ctx) {}
void on_unload() {}
void on_update(PluginContext* ctx) {}

void on_draw_ui(PluginContext* ctx) {
    if (ctx->ui_begin("MyPlugin")) {
        ctx->ui_text("Hello World");
        char buf[64];
        sprintf(buf, "Entities: %d", ctx->entity_count);
        ctx->ui_text(buf);
    }
    ctx->ui_end();
}

static Plugin info {
    "MyPlugin", "0.1",
    on_load, on_unload, on_update, on_draw_ui
};

extern "C" __declspec(dllexport) Plugin* get_plugin() { return &info; }