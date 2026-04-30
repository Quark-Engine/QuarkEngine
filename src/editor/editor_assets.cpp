#include "editor_assets.h"
#include "editor_utils.h"
#include "editor_entity.h"
#include "editor_viewers.h"
#include "editor.h"
#include "tex.h"
#include "models.h"
#include <vector>
#include <algorithm>
#include <imgui.h>
#include <fstream>
#include <unordered_map>

namespace fs = std::filesystem;

static Texture icon_file_tex = {0};
static Texture icon_folder_tex = {0};
static Texture icon_full_folder_tex = {0};

static std::unordered_map<std::string, Texture> tex_cache;
static std::unordered_map<std::string, Texture> model_preview_cache;
static std::unordered_map<std::string, RenderTexture2D> model_render_cache;

const float icon_size = 64.0f;
const float padding = 10.0f;
const float cell_size = icon_size + padding;

static int renaming_index = -1;
static char rename_buf[128] = "";

static bool scene_asset_dragging = false;
static std::string dragged_scene_asset_name;


ModelAsset* find_asset_by_path(const fs::path& full_path, const fs::path& project_path_value) {
    std::string asset_name = get_asset_name_for_path(project_path_value, full_path);
    return find_asset_by_name(asset_name);
}

bool import_path_to_resources(const fs::path& src, const fs::path& resource_dir) {
    std::error_code ec;

    if (!fs::exists(src, ec) || ec) {
        TraceLog(LOG_WARNING, "Dropped path does not exist: %s", src.string().c_str());
        return false;
    }

    if (fs::is_regular_file(src, ec)) {
        fs::path dst = resource_dir / src.filename();
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
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

        fs::recursive_directory_iterator it(
            src,
            fs::directory_options::skip_permission_denied,
            ec
        );
        if (ec) {
            TraceLog(LOG_WARNING, "Failed to open dropped directory: %s", src.string().c_str());
            return false;
        }

        for (const auto& entry : it) {
            if (!entry.is_regular_file(ec) || ec) {
                ec.clear();
                continue;
            }

            fs::path relative = fs::relative(entry.path(), src, ec);
            if (ec) {
                TraceLog(LOG_WARNING, "Failed to compute relative path: %s", entry.path().string().c_str());
                ec.clear();
                continue;
            }

            fs::path dst = dst_root / relative;
            fs::create_directories(dst.parent_path(), ec);
            if (ec) {
                TraceLog(LOG_WARNING, "Failed to create directory for import: %s", dst.parent_path().string().c_str());
                ec.clear();
                continue;
            }

            fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                TraceLog(LOG_WARNING, "Failed to import file from directory: %s", entry.path().string().c_str());
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

std::string build_resource_signature(const fs::path& resource_dir) {
    std::vector<std::string> entries;
    std::error_code ec;
    fs::recursive_directory_iterator it(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return {};

    for (const auto& entry : it) {
        std::error_code entry_ec;
        const fs::path relative = fs::relative(entry.path(), resource_dir, entry_ec);
        if (entry_ec) continue;

        std::string row = relative.generic_string();
        if (entry.is_regular_file(entry_ec) && !entry_ec) {
            std::error_code size_ec;
            std::error_code time_ec;
            const auto size = fs::file_size(entry.path(), size_ec);
            const auto time = fs::last_write_time(entry.path(), time_ec);

            row += "|f|";
            row += size_ec ? "0" : std::to_string(size);
            row += "|";
            row += time_ec ? "0" : std::to_string(time.time_since_epoch().count());
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

void draw_assets_ui() {
    ImGui::SetNextWindowSize(ImVec2(1270, 165), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 550), ImGuiCond_Once);
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (icon_file_tex.id == 0) icon_file_tex = LoadTexture("assets/file.png");
    if (icon_folder_tex.id == 0) icon_folder_tex = LoadTexture("assets/folder.png");
    if (icon_full_folder_tex.id == 0) icon_full_folder_tex = LoadTexture("assets/full_folder.png");

    static ImVec2 selection_start;
    static ImVec2 selection_end;
    static bool selecting = false;
    static int rename_target = -1;

    if (current_asset_path.empty())
        current_asset_path = fs::path(project_path);

    ImVec2 window_size = ImGui::GetWindowSize();

    fs::path project_root = fs::path(project_path);
    fs::path relative_path = fs::relative(current_asset_path, project_root.parent_path());

    std::vector<fs::path> crumbs;
    for (auto& part : relative_path) {
        crumbs.push_back(part);
    }

    fs::path rebuilt = project_root.parent_path();

    for (int c = 0; c < (int)crumbs.size(); c++) {
        rebuilt /= crumbs[c];
        std::string label = crumbs[c].string() + "/";

        if (ImGui::SmallButton(label.c_str())) {
            current_asset_path = rebuilt;
            selected_asset_index = -1;
            tex_cache.clear();
            model_preview_cache.clear();
            for (auto& rt : model_render_cache) {
                UnloadRenderTexture(rt.second);
            }
            model_render_cache.clear();
        }

        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    std::vector<LocalEntry> dirs_list;
    std::vector<LocalEntry> files_list;

    std::error_code ec_dir;

    for (auto& p : fs::directory_iterator(current_asset_path, ec_dir)) {
        LocalEntry e;
        e.filename = p.path().filename().string();

        fs::path fp = p.path();
        std::string ext = fp.extension().string();
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        e.extension = ext;

        e.is_directory = p.is_directory();
        e.is_image = is_image_file(p.path());
        e.is_model = is_model_file(p.path());
        e.is_material = (e.extension == "mtl");

        if (e.is_directory) {
            dirs_list.push_back(e);
        } 
        
        else {
            if (e.is_image) {
                for (auto& ae : asset_entries) {
                    if (ae.filename == e.filename && ae.is_image) {
                        e.texture = ae.texture;
                        break;
                    }
                }
            }

            files_list.push_back(e);
        }
    }

    std::vector<LocalEntry> entries;
    entries.insert(entries.end(), dirs_list.begin(), dirs_list.end());
    entries.insert(entries.end(), files_list.begin(), files_list.end());

    if (entries.empty()) {
        const char* text = "Empty folder";
        ImVec2 text_size = ImGui::CalcTextSize(text);

        ImGui::SetCursorPosX((window_size.x - text_size.x) * 0.5f);
        ImGui::SetCursorPosY((window_size.y - text_size.y) * 0.5f);
        ImGui::Text("%s", text);
    }

    else {
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

        float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        for (int i = 0; i < (int)entries.size(); i++) {
            auto& entry = entries[i];

            ImGui::PushID(i);
            ImGui::BeginGroup();

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size(icon_size, icon_size + 20);

            ImGui::InvisibleButton("asset_btn", size);
            
            const bool item_active = ImGui::IsItemActive();
            const bool item_hovered = ImGui::IsItemHovered();

            if (item_hovered) {
                ImGui::SetTooltip("%s", entry.filename.c_str());
            }

            if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0)) {
                selected_asset_index = i;
            }

            if (entry.is_directory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                current_asset_path /= entry.filename;
                selected_asset_index = -1;

                tex_cache.clear();
                model_preview_cache.clear();
                for (auto& rt : model_render_cache) {
                    UnloadRenderTexture(rt.second);
                }
                model_render_cache.clear();
                ImGui::EndGroup();
                ImGui::PopID();
                break;
            }

            if (!entry.is_directory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                fs::path full_path = current_asset_path / entry.filename;
                
                if (entry.is_model) {
                    ModelAsset* asset = find_asset_by_path(full_path, project_path);
                    if (asset) {
                        if (viewer_model.meshCount > 0) UnloadModel(viewer_model);
                        if (load_model_instance(*asset, viewer_model)) {
                            show_model_viewer = true;
                            viewer_radius = 5.0f;
                            viewer_phi = 20.0f;
                            viewer_theta = 45.0f;
                            viewer_target = { 0, 0, 0 };
                            viewer_model_rotation = { 0, 0, 0 };

                            BoundingBox bb = GetModelBoundingBox(viewer_model);
                            viewer_model_center = {
                                (bb.min.x + bb.max.x) * 0.5f,
                                (bb.min.y + bb.max.y) * 0.5f,
                                (bb.min.z + bb.max.z) * 0.5f
                            };
                        }
                    }
                }
                else if (entry.is_material) {
                    fs::path full_path = current_asset_path / entry.filename;
                    std::ifstream mtl_file(full_path);

                    if (mtl_file.is_open()) {
                        if (viewer_mat_sphere.meshCount > 0) UnloadModel(viewer_mat_sphere);
                        viewer_mat_sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));
                        
                        Material& mat = viewer_mat_sphere.materials[0];
                        material_albedo = WHITE;
                        material_albedo_f[0] = 1.0f; material_albedo_f[1] = 1.0f; 
                        material_albedo_f[2] = 1.0f; material_albedo_f[3] = 1.0f;
                        material_brightness = 1.0f;
                        material_texture = { 0 };

                        std::string mtl_line;
                        while (std::getline(mtl_file, mtl_line)) {
                            if (mtl_line.empty() || mtl_line[0] == '#') continue;
                            
                            std::istringstream iss(mtl_line);
                            std::string type;
                            iss >> type;
                            
                            if (type == "Kd") {
                                float r, g, b;
                                if (iss >> r >> g >> b) {
                                    mat.maps[MATERIAL_MAP_DIFFUSE].color = { 
                                        (unsigned char)(r * 255), 
                                        (unsigned char)(g * 255), 
                                        (unsigned char)(b * 255), 255 
                                    };
                                    material_albedo = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
                                    material_albedo_f[0] = r;
                                    material_albedo_f[1] = g;
                                    material_albedo_f[2] = b;
                                    material_albedo_f[3] = 1.0f;
                                }
                            } 
                            else if (type == "map_Kd") {
                                std::string tex_name;
                                if (iss >> tex_name) {
                                    fs::path tex_path = full_path.parent_path() / tex_name;
                                    std::string tex_full_str = tex_path.string();
                                    
                                    if (tex_cache.find(tex_full_str) == tex_cache.end()) {
                                        if (fs::exists(tex_path)) {
                                            tex_cache[tex_full_str] = LoadTexture(tex_full_str.c_str());
                                        }
                                    }
                                    if (tex_cache.count(tex_full_str)) {
                                        mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex_cache[tex_full_str];
                                        material_texture = tex_cache[tex_full_str];
                                    }
                                }
                            }
                        }
                        rebuild_material_preview_mesh();
                        show_material_viewer = true;
                        viewer_radius = 2.5f;
                        viewer_phi = 20.0f;
                        viewer_theta = 45.0f;
                        viewer_target = { 0, 0, 0 };
                        viewer_model_rotation = { 0, 0, 0 };
                    }
                }
                else {
#ifdef _WIN32
                    ShellExecuteA(NULL, "open", full_path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
                    std::string command = "open \"" + full_path.string() + "\"";
                    system(command.c_str());
#elif __linux__
                    std::string command = "xdg-open \"" + full_path.string() + "\"";
                    system(command.c_str());
#endif
                }
            }
            
            if (ImGui::BeginPopupContextItem("AssetContext")) {
                if (ImGui::MenuItem("Delete")) {
                    save_state();

                    fs::path target = current_asset_path / entry.filename;
                    std::error_code ec;

                    if (entry.is_directory) fs::remove_all(target, ec);
                    else fs::remove(target, ec);

                    refresh_textures(&scene, project_path);
                    refresh_assets(project_path);
                    refresh_models(project_path, scene);

                    selected_asset_index = -1;

                    ImGui::EndPopup();
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (ImGui::MenuItem("Rename")) {
                    rename_target = i;
                    size_t copied = entry.filename.copy(rename_buf, sizeof(rename_buf) - 1);
                    rename_buf[copied] = '\0';
                    
                    ImGui::OpenPopup("RenameAsset");
                }

                ImGui::EndPopup();
            }

            bool selected = (selected_asset_index == i);

            if (selecting && (fabs(selection_start.x - selection_end.x) > 5.0f || fabs(selection_start.y - selection_end.y) > 5.0f)) {
                ImVec2 min(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
                ImVec2 max(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));

                if (!(pos.x + size.x < min.x || pos.x > max.x || pos.y + size.y < min.y || pos.y > max.y)) {
                    selected = true;
                }
            }

            if (selected) {
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(80, 140, 255, 100));
            }

            ImGui::SetCursorScreenPos(pos);
            if (entry.is_directory) {
                if (fs::is_empty(current_asset_path / entry.filename)) {
                    if (icon_folder_tex.id != 0) ImGui::Image((void*)(intptr_t)icon_folder_tex.id, ImVec2(icon_size, icon_size));
                    else ImGui::Button("Folder", ImVec2(icon_size, icon_size));
                }

                else {
                    if (icon_full_folder_tex.id != 0) ImGui::Image((void*)(intptr_t)icon_full_folder_tex.id, ImVec2(icon_size, icon_size));
                    else ImGui::Button("Folder", ImVec2(icon_size, icon_size));
                }
            }
            else if (entry.is_image) {
                std::string full = (current_asset_path / entry.filename).string();

                if (tex_cache.find(full) == tex_cache.end())
                    tex_cache[full] = LoadTexture(full.c_str());
                ImGui::Image((void*)(intptr_t)tex_cache[full].id, ImVec2(icon_size, icon_size));
            }
            
            else if (entry.is_model) {
                std::string full = (current_asset_path / entry.filename).string();
                
                Texture preview_tex = {0};
                bool load_failed = false;
                
                if (model_preview_cache.find(full) != model_preview_cache.end()) {
                    preview_tex = model_preview_cache[full];
                } else {
                    ModelAsset* asset = find_asset_by_path(current_asset_path / entry.filename, project_path);
                    if (asset) {
                        preview_tex = create_model_preview(*asset, full);
                        if (preview_tex.id != 0) {
                            model_preview_cache[full] = preview_tex;
                        } 
                        
                        else {
                            load_failed = true;
                        }
                    } 
                    
                    else {
                        load_failed = true;
                    }
                }
                
                if (preview_tex.id != 0) {
                    ImGui::Image((void*)(intptr_t)preview_tex.id, ImVec2(icon_size, icon_size));
                } 
                
                else {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImU32 bg_color = load_failed ? IM_COL32(80, 80, 90, 255) : IM_COL32(100, 100, 120, 255);
                    draw_list->AddRectFilled(pos, ImVec2(pos.x + icon_size, pos.y + icon_size), bg_color);
                    
                    if (load_failed) {
                        draw_list->AddLine(
                            ImVec2(pos.x + 10, pos.y + 10),
                            ImVec2(pos.x + icon_size - 10, pos.y + icon_size - 10),
                            IM_COL32(255, 100, 100, 200), 2.0f
                        );
                        draw_list->AddLine(
                            ImVec2(pos.x + icon_size - 10, pos.y + 10),
                            ImVec2(pos.x + 10, pos.y + icon_size - 10),
                            IM_COL32(255, 100, 100, 200), 2.0f
                        );
                    }
                }
            }
        
            else {
                if (icon_file_tex.id != 0)
                    ImGui::Image((void*)(intptr_t)icon_file_tex.id, ImVec2(icon_size, icon_size));
                else {
                    std::string ext = entry.extension;
                    if (ext.empty()) ext = "file";
                    ImGui::Button(ext.c_str(), ImVec2(icon_size, icon_size));
                }
            }

            if (!entry.is_directory && !entry.extension.empty()) {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                
                std::string ext_display = entry.extension;
                if (ext_display.size() > 3) ext_display = ext_display.substr(0, 3);
                
                ImVec2 ext_text_size = ImGui::CalcTextSize(ext_display.c_str());
                ImVec2 ext_pos = ImVec2(
                    pos.x + icon_size - ext_text_size.x - 2,
                    pos.y + icon_size - ext_text_size.y - 2
                );
                
                draw_list->AddRectFilled(
                    ImVec2(ext_pos.x - 2, ext_pos.y - 1),
                    ImVec2(pos.x + icon_size, pos.y + icon_size),
                    IM_COL32(0, 0, 0, 180)
                );
                
                draw_list->AddText(ext_pos, IM_COL32(255, 255, 255, 255), ext_display.c_str());
            }

            if (!entry.is_directory && is_model_file(fs::path(entry.filename))) {
                const std::string asset_name = get_asset_name_for_path(fs::path(project_path), current_asset_path / entry.filename);

                if (item_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    scene_asset_dragging = true;
                    dragged_scene_asset_name = asset_name;
                }

                if (scene_asset_dragging && dragged_scene_asset_name == asset_name) {
                    ImGui::SetTooltip("Spawn %s", dragged_scene_asset_name.c_str());
                }
            }

            std::string label = entry.filename;
            if (label.size() > 10) label = label.substr(0, 8) + "..";

            ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
            ImGui::SetCursorScreenPos(ImVec2(pos.x + (icon_size - label_size.x) * 0.5f, pos.y + icon_size + 2.0f));
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndGroup();

            float last_x2 = ImGui::GetItemRectMax().x;
            float next_x2 = last_x2 + ImGui::GetStyle().ItemSpacing.x + icon_size;

            if (i + 1 < (int)entries.size() && next_x2 < window_visible_x2) {
                ImGui::SameLine();
            }

            ImGui::PopID();
        }

        if (selecting && (fabs(selection_start.x - selection_end.x) > 5.0f || fabs(selection_start.y - selection_end.y) > 5.0f)) {
            ImVec2 min(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
            ImVec2 max(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));

            ImGui::GetForegroundDrawList()->AddRectFilled(min, max, IM_COL32(80, 140, 255, 40));
        }

        if (rename_target >= 0) {
            ImGui::OpenPopup("RenameAsset");
            rename_target = -2;
        }

        static std::string last_filename;
        if (rename_target == -2 && ImGui::BeginPopupModal("RenameAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("##rename", rename_buf, IM_ARRAYSIZE(rename_buf));

            if (ImGui::Button("OK")) {
                std::string new_filename = rename_buf;
                if (last_filename != new_filename) {
                    save_state();
                    last_filename = new_filename;

                    if (selected_asset_index >= 0 && selected_asset_index < (int)entries.size()) {
                        fs::path old_path = current_asset_path / entries[selected_asset_index].filename;
                        fs::path new_path = current_asset_path / rename_buf;

                        if (rename_buf[0] != '\0' && old_path != new_path && fs::exists(old_path)) {
                            try {
                                fs::rename(old_path, new_path);
                                refresh_textures(&scene, project_path);
                                refresh_assets(project_path);
                                refresh_models(project_path, scene);
                                selected_asset_index = -1;
                            } catch (...) {}
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
    }
    ImGui::End();
}
