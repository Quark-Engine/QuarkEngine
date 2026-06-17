#pragma once

#include "../editor.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include <string>
#include <unordered_map>

namespace editor_internal {

extern ImGuizmo::OPERATION gizmo_mode;
extern int renaming_index;
extern char rename_buf[128];
extern bool scene_asset_dragging;

extern std::string dragged_scene_asset_name;
extern bool file_dragging;
extern int dragged_file_index;
extern int dragged_target_folder_index;

extern std::unordered_map<std::string, Texture> tex_cache;
extern bool show_about_window;
extern bool has_clipboard;
extern Entity clipboard_data;

void restore_scene_entity_models(Scene& scene);

}
