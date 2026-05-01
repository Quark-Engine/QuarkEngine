#include "plugin.h"
#include "imgui.h"

static PluginContext* g_ctx = nullptr;

void on_load(PluginContext* ctx)  { g_ctx = ctx; }
void on_unload()                  {}
void on_update(PluginContext* ctx) {}

void on_draw_ui(PluginContext* ctx) {
    ImGui::Begin("Huesos Plugin");
    ImGui::Text("Penis");
    ImGui::End();
}
static Plugin info {
    "Deadass Plugin", "14.88",
    on_load, on_unload, on_update, on_draw_ui
};

extern "C" Plugin* get_plugin() { return &info; }