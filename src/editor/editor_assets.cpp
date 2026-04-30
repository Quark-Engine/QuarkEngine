#include "editor_assets.h"

#include "editor.h"
#include "editor_entity.h"
#include "editor_utils.h"
#include "editor_viewers.h"
#include "../headers/project.h"
#include "../headers/tex.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cctype>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define CloseWindow WinAPICloseWindow
#define ShowCursor WinAPIShowCursor
#define Rectangle WinAPIRectangle
#include <windows.h>
#include <shellapi.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#endif

namespace fs = std::filesystem;

namespace {

std::unordered_map<std::string, Texture> model_preview_cache;
std::unordered_map<std::string, RenderTexture2D> model_render_cache;
Texture icon_file_tex = {0};
Texture icon_folder_tex = {0};
Texture icon_full_folder_tex = {0};

constexpr float kIconSize = 64.0f;

}

ModelAsset* find_asset_by_name(const std::string& asset_name) {
    for (auto& asset : assets) {
        if (asset.name == asset_name) return &asset;
    }
    return nullptr;
}

ModelAsset* find_asset_by_path(const fs::path& full_path, const fs::path& project_path_value) {
    return find_asset_by_name(get_asset_name_for_path(project_path_value, full_path));
}

std::string build_resource_signature(const fs::path& resource_dir) {
    std::vector<std::string> entries;
    std::error_code ec;
    fs::recursive_directory_iterator iterator(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return {};

    for (const auto& entry : iterator) {
        std::error_code entry_ec;
        const fs::path relative = fs::relative(entry.path(), resource_dir, entry_ec);
        if (entry_ec) continue;

        std::string row = relative.generic_string();
        if (entry.is_regular_file(entry_ec) && !entry_ec) {
            std::error_code size_ec;
            std::error_code time_ec;
            row += "|f|";
            row += size_ec ? "0" : std::to_string(fs::file_size(entry.path(), size_ec));
            row += "|";
            row += time_ec ? "0" : std::to_string(static_cast<long long>(fs::last_write_time(entry.path(), time_ec).time_since_epoch().count()));
        } else {
            row += "|d";
        }

        entries.push_back(std::move(row));
    }

    std::sort(entries.begin(), entries.end());

    std::string signature;
    for (const auto& entry : entries) {
        signature += entry;
        signature.push_back('\n');
    }

    return signature;
}

Texture create_model_preview(const ModelAsset& asset, const std::string& cache_key, int preview_size) {
    Texture result = {0};

    Model preview_model = {0};
    if (!load_model_instance(asset, preview_model)) return result;
    if (!has_valid_model_data(preview_model)) {
        UnloadModel(preview_model);
        return result;
    }

    RenderTexture2D render_texture = LoadRenderTexture(preview_size, preview_size);
    if (render_texture.id == 0) {
        UnloadModel(preview_model);
        return result;
    }

    Vector3 min_bound = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vector3 max_bound = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool has_vertices = false;

    for (int mesh_index = 0; mesh_index < preview_model.meshCount; mesh_index++) {
        Mesh& mesh = preview_model.meshes[mesh_index];
        if (!mesh.vertices) continue;

        for (int vertex_index = 0; vertex_index < mesh.vertexCount; vertex_index++) {
            const float vx = mesh.vertices[vertex_index * 3 + 0];
            const float vy = mesh.vertices[vertex_index * 3 + 1];
            const float vz = mesh.vertices[vertex_index * 3 + 2];

            min_bound.x = fminf(min_bound.x, vx);
            min_bound.y = fminf(min_bound.y, vy);
            min_bound.z = fminf(min_bound.z, vz);
            max_bound.x = fmaxf(max_bound.x, vx);
            max_bound.y = fmaxf(max_bound.y, vy);
            max_bound.z = fmaxf(max_bound.z, vz);
            has_vertices = true;
        }
    }

    Vector3 center = {0, 0, 0};
    float distance = 3.0f;
    if (has_vertices) {
        center = {
            (min_bound.x + max_bound.x) * 0.5f,
            (min_bound.y + max_bound.y) * 0.5f,
            (min_bound.z + max_bound.z) * 0.5f
        };

        const Vector3 size = {
            max_bound.x - min_bound.x,
            max_bound.y - min_bound.y,
            max_bound.z - min_bound.z
        };

        float max_size = fmaxf(fmaxf(size.x, size.y), size.z);
        if (max_size < 0.1f) max_size = 1.0f;
        distance = max_size * 2.0f;
    }

    Camera3D camera = {};
    camera.position = { center.x + distance * 0.6f, center.y + distance * 0.5f, center.z + distance * 0.6f };
    camera.target = center;
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(render_texture);
    ClearBackground({ 32, 32, 40, 255 });
    BeginMode3D(camera);
    DrawModel(preview_model, { 0, 0, 0 }, 1.0f, WHITE);
    EndMode3D();
    EndTextureMode();

    result = render_texture.texture;
    model_render_cache[cache_key] = render_texture;
    UnloadModel(preview_model);
    return result;
}

bool import_path_to_resources(const fs::path& src, const fs::path& resource_dir) {
    std::error_code ec;

    if (!fs::exists(src, ec) || ec) {
        TraceLog(LOG_WARNING, "Dropped path does not exist: %s", src.string().c_str());
        return false;
    }

    if (fs::is_regular_file(src, ec)) {
        fs::copy_file(src, resource_dir / src.filename(), fs::copy_options::overwrite_existing, ec);
        if (ec) {
            TraceLog(LOG_WARNING, "Failed to import file: %s", src.string().c_str());
            return false;
        }
        return true;
    }

    if (fs::is_directory(src, ec)) {
        bool imported_any = false;
        const fs::path dst_root = resource_dir / src.filename();
        fs::create_directories(dst_root, ec);
        ec.clear();

        fs::recursive_directory_iterator iterator(src, fs::directory_options::skip_permission_denied, ec);
        if (ec) {
            TraceLog(LOG_WARNING, "Failed to open dropped directory: %s", src.string().c_str());
            return false;
        }

        for (const auto& entry : iterator) {
            if (!entry.is_regular_file(ec) || ec) {
                ec.clear();
                continue;
            }

            const fs::path relative = fs::relative(entry.path(), src, ec);
            if (ec) {
                ec.clear();
                continue;
            }

            const fs::path dst = dst_root / relative;
            fs::create_directories(dst.parent_path(), ec);
            if (ec) {
                ec.clear();
                continue;
            }

            fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                ec.clear();
                continue;
            }

            imported_any = true;
        }

        return imported_any;
    }

    TraceLog(LOG_WARNING, "Unsupported dropped path: %s", src.string().c_str());
    return false;
}

void draw_assets_ui(Editor& editor) {
    ImGui::SetNextWindowSize(ImVec2(1270, 165), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 550), ImGuiCond_Once);
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    if (icon_file_tex.id == 0) icon_file_tex = LoadTexture("assets/file.png");
    if (icon_folder_tex.id == 0) icon_folder_tex = LoadTexture("assets/folder.png");
    if (icon_full_folder_tex.id == 0) icon_full_folder_tex = LoadTexture("assets/full_folder.png");

    static ImVec2 selection_start = {};
    static ImVec2 selection_end = {};
    static bool selecting = false;
    static int rename_target = -1;

    if (editor.current_asset_path.empty() && !editor.project_path.empty()) {
        editor.current_asset_path = fs::path(editor.project_path) / "resources";
        std::error_code ec;
        fs::create_directories(editor.current_asset_path, ec);
    }

    const ImVec2 window_size = ImGui::GetWindowSize();
    const fs::path project_root = fs::path(editor.project_path);
    const fs::path relative_path = fs::relative(editor.current_asset_path, project_root.parent_path());

    std::vector<fs::path> crumbs;
    for (const auto& part : relative_path) crumbs.push_back(part);

    fs::path rebuilt = project_root.parent_path();
    for (int index = 0; index < static_cast<int>(crumbs.size()); index++) {
        rebuilt /= crumbs[index];
        const std::string label = crumbs[index].string() + "/";
        if (ImGui::SmallButton(label.c_str())) {
            editor.current_asset_path = rebuilt;
            editor.selected_asset_index = -1;
            editor_internal::tex_cache.clear();
            model_preview_cache.clear();
            for (auto& pair : model_render_cache) UnloadRenderTexture(pair.second);
            model_render_cache.clear();
        }
        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    std::vector<LocalEntry> directories;
    std::vector<LocalEntry> files;
    std::error_code dir_error;
    for (const auto& path : fs::directory_iterator(editor.current_asset_path, dir_error)) {
        LocalEntry entry = {};
        entry.filename = path.path().filename().string();
        entry.is_directory = path.is_directory();
        entry.is_image = is_image_file(path.path());
        entry.is_model = is_model_file(path.path());

        std::string ext = path.path().extension().string();
        if (!ext.empty() && ext[0] == '.') ext.erase(ext.begin());
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        entry.extension = ext;
        entry.is_material = entry.extension == "mtl";

        if (entry.is_directory) {
            directories.push_back(entry);
        } else {
            if (entry.is_image) {
                for (auto& asset_entry : asset_entries) {
                    if (asset_entry.filename == entry.filename && asset_entry.is_image) {
                        entry.texture = asset_entry.texture;
                        break;
                    }
                }
            }
            files.push_back(entry);
        }
    }

    std::vector<LocalEntry> entries;
    entries.insert(entries.end(), directories.begin(), directories.end());
    entries.insert(entries.end(), files.begin(), files.end());

    if (entries.empty()) {
        const char* text = "Empty folder";
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        ImGui::SetCursorPosX((window_size.x - text_size.x) * 0.5f);
        ImGui::SetCursorPosY((window_size.y - text_size.y) * 0.5f);
        ImGui::TextUnformatted(text);
        ImGui::End();
        return;
    }

    ImGui::BeginChild("AssetScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseClicked(0)) {
            selection_start = ImGui::GetMousePos();
            selection_end = selection_start;
            selecting = true;
        }
        if (ImGui::IsMouseDown(0) && selecting) selection_end = ImGui::GetMousePos();
        if (ImGui::IsMouseReleased(0)) selecting = false;
    }

    const float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        auto& entry = entries[i];
        ImGui::PushID(i);
        ImGui::BeginGroup();

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(kIconSize, kIconSize + 20.0f);
        ImGui::InvisibleButton("asset_btn", size);

        const bool item_active = ImGui::IsItemActive();
        const bool item_hovered = ImGui::IsItemHovered();
        if (item_hovered) ImGui::SetTooltip("%s", entry.filename.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0)) editor.selected_asset_index = i;

        if (entry.is_directory && item_hovered && ImGui::IsMouseDoubleClicked(0)) {
            editor.current_asset_path /= entry.filename;
            editor.selected_asset_index = -1;
            editor_internal::tex_cache.clear();
            model_preview_cache.clear();
            for (auto& pair : model_render_cache) UnloadRenderTexture(pair.second);
            model_render_cache.clear();
            ImGui::EndGroup();
            ImGui::PopID();
            break;
        }

        if (!entry.is_directory && item_hovered && ImGui::IsMouseDoubleClicked(0)) {
            const fs::path full_path = editor.current_asset_path / entry.filename;
            if (entry.is_model) {
                ModelAsset* asset = find_asset_by_path(full_path, editor.project_path);
                if (asset) open_model_viewer_for_asset(*asset);
            } else if (entry.is_material) {
                open_material_viewer_for_path(full_path, editor_internal::tex_cache);
            } else {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", full_path.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
            }
        }

        if (ImGui::BeginPopupContextItem("AssetContext")) {
            if (ImGui::MenuItem("Delete")) {
                editor.save_state();

                const fs::path target = editor.current_asset_path / entry.filename;
                std::error_code ec;
                if (entry.is_directory) fs::remove_all(target, ec);
                else fs::remove(target, ec);

                refresh_textures(&editor.scene, editor.project_path);
                refresh_assets(editor.project_path);
                refresh_models(editor.project_path, editor.scene);
                editor.selected_asset_index = -1;

                ImGui::EndPopup();
                ImGui::EndGroup();
                ImGui::PopID();
                break;
            }

            if (ImGui::MenuItem("Rename")) {
                rename_target = i;
                const size_t copied = entry.filename.copy(editor_internal::rename_buf, sizeof(editor_internal::rename_buf) - 1);
                editor_internal::rename_buf[copied] = '\0';
                ImGui::OpenPopup("RenameAsset");
            }

            ImGui::EndPopup();
        }

        bool selected = editor.selected_asset_index == i;
        if (selecting && (fabsf(selection_start.x - selection_end.x) > 5.0f || fabsf(selection_start.y - selection_end.y) > 5.0f)) {
            const ImVec2 min(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
            const ImVec2 max(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));
            if (!(pos.x + size.x < min.x || pos.x > max.x || pos.y + size.y < min.y || pos.y > max.y)) {
                selected = true;
            }
        }

        if (selected) {
            ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(80, 140, 255, 100));
        }

        ImGui::SetCursorScreenPos(pos);
        if (entry.is_directory) {
            const Texture texture = fs::is_empty(editor.current_asset_path / entry.filename) ? icon_folder_tex : icon_full_folder_tex;
            if (texture.id != 0) ImGui::Image((void*)(intptr_t)texture.id, ImVec2(kIconSize, kIconSize));
            else ImGui::Button("Folder", ImVec2(kIconSize, kIconSize));
        } else if (entry.is_image) {
            const std::string full = (editor.current_asset_path / entry.filename).string();
            if (!editor_internal::tex_cache.count(full)) {
                editor_internal::tex_cache[full] = LoadTexture(full.c_str());
            }
            ImGui::Image((void*)(intptr_t)editor_internal::tex_cache[full].id, ImVec2(kIconSize, kIconSize));
        } else if (entry.is_model) {
            const std::string full = (editor.current_asset_path / entry.filename).string();
            Texture preview_texture = {0};
            bool load_failed = false;

            if (model_preview_cache.count(full)) {
                preview_texture = model_preview_cache[full];
            } else {
                ModelAsset* asset = find_asset_by_path(editor.current_asset_path / entry.filename, editor.project_path);
                if (asset) {
                    preview_texture = create_model_preview(*asset, full);
                    if (preview_texture.id != 0) model_preview_cache[full] = preview_texture;
                    else load_failed = true;
                } else {
                    load_failed = true;
                }
            }

            if (preview_texture.id != 0) {
                ImGui::Image((void*)(intptr_t)preview_texture.id, ImVec2(kIconSize, kIconSize));
            } else {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize), load_failed ? IM_COL32(80, 80, 90, 255) : IM_COL32(100, 100, 120, 255));
                if (load_failed) {
                    draw_list->AddLine(ImVec2(pos.x + 10, pos.y + 10), ImVec2(pos.x + kIconSize - 10, pos.y + kIconSize - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                    draw_list->AddLine(ImVec2(pos.x + kIconSize - 10, pos.y + 10), ImVec2(pos.x + 10, pos.y + kIconSize - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                }
            }
        } else {
            if (icon_file_tex.id != 0) ImGui::Image((void*)(intptr_t)icon_file_tex.id, ImVec2(kIconSize, kIconSize));
            else ImGui::Button(entry.extension.empty() ? "file" : entry.extension.c_str(), ImVec2(kIconSize, kIconSize));
        }

        if (!entry.is_directory && !entry.extension.empty()) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            std::string ext_display = entry.extension;
            if (ext_display.size() > 3) ext_display.resize(3);
            const ImVec2 ext_text_size = ImGui::CalcTextSize(ext_display.c_str());
            const ImVec2 ext_pos(pos.x + kIconSize - ext_text_size.x - 2.0f, pos.y + kIconSize - ext_text_size.y - 2.0f);
            draw_list->AddRectFilled(ImVec2(ext_pos.x - 2.0f, ext_pos.y - 1.0f), ImVec2(pos.x + kIconSize, pos.y + kIconSize), IM_COL32(0, 0, 0, 180));
            draw_list->AddText(ext_pos, IM_COL32(255, 255, 255, 255), ext_display.c_str());
        }

        if (!entry.is_directory && is_model_file(fs::path(entry.filename))) {
            const std::string asset_name = get_asset_name_for_path(fs::path(editor.project_path), editor.current_asset_path / entry.filename);
            if (item_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                editor_internal::scene_asset_dragging = true;
                editor_internal::dragged_scene_asset_name = asset_name;
            }
            if (editor_internal::scene_asset_dragging && editor_internal::dragged_scene_asset_name == asset_name) {
                ImGui::SetTooltip("Spawn %s", editor_internal::dragged_scene_asset_name.c_str());
            }
        }

        std::string label = entry.filename;
        if (label.size() > 10) label = label.substr(0, 8) + "..";
        const ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
        ImGui::SetCursorScreenPos(ImVec2(pos.x + (kIconSize - label_size.x) * 0.5f, pos.y + kIconSize + 2.0f));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndGroup();

        const float next_x2 = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + kIconSize;
        if (i + 1 < static_cast<int>(entries.size()) && next_x2 < window_visible_x2) ImGui::SameLine();
        ImGui::PopID();
    }

    if (selecting && (fabsf(selection_start.x - selection_end.x) > 5.0f || fabsf(selection_start.y - selection_end.y) > 5.0f)) {
        const ImVec2 min(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
        const ImVec2 max(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));
        ImGui::GetForegroundDrawList()->AddRectFilled(min, max, IM_COL32(80, 140, 255, 40));
    }

    if (rename_target >= 0) {
        ImGui::OpenPopup("RenameAsset");
        rename_target = -2;
    }

    static std::string last_filename;
    if (rename_target == -2 && ImGui::BeginPopupModal("RenameAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename", editor_internal::rename_buf, IM_ARRAYSIZE(editor_internal::rename_buf));

        if (ImGui::Button("OK")) {
            const std::string new_filename = editor_internal::rename_buf;
            if (last_filename != new_filename) {
                editor.save_state();
                last_filename = new_filename;

                if (editor.selected_asset_index >= 0 && editor.selected_asset_index < static_cast<int>(entries.size())) {
                    const fs::path old_path = editor.current_asset_path / entries[editor.selected_asset_index].filename;
                    const fs::path new_path = editor.current_asset_path / editor_internal::rename_buf;

                    if (editor_internal::rename_buf[0] != '\0' && old_path != new_path && fs::exists(old_path)) {
                        try {
                            fs::rename(old_path, new_path);
                            refresh_textures(&editor.scene, editor.project_path);
                            refresh_assets(editor.project_path);
                            refresh_models(editor.project_path, editor.scene);
                            editor.selected_asset_index = -1;
                        } catch (...) {
                        }
                    }
                }
            }

            rename_target = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            rename_target = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
}

void Editor::draw_assets_ui() {
    ::draw_assets_ui(*this);
}

void cleanup_assets_ui() {
    for (auto& pair : model_render_cache) {
        UnloadRenderTexture(pair.second);
    }
    model_render_cache.clear();
    model_preview_cache.clear();
    editor_internal::tex_cache.clear();
}
