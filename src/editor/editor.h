#pragma once

#include "../headers/editor.h"
#include "imgui.h"
#include "../headers/ImGuizmo.h"
#include <string>
#include <unordered_map>

namespace editor_internal {
extern ImGuizmo::OPERATION gizmo_mode;
extern int renaming_index;
extern char rename_buf[128];
extern bool scene_asset_dragging;
extern std::string dragged_scene_asset_name;
extern std::unordered_map<std::string, Texture> tex_cache;
extern bool show_about_window;
extern bool has_clipboard;
extern Entity clipboard_data;

void restore_scene_entity_models(Scene& scene);

}
