#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include "headers/editor.h"
#include "headers/lighting.h"
#include "headers/entity.h"
#include "headers/ImGuizmo.h"
#include "headers/project.h"

namespace fs = std::filesystem;

static ImGuizmo::OPERATION gizmo_mode = ImGuizmo::TRANSLATE;
static int renaming_index = -1;
static char rename_buf[128] = "";
static bool scene_asset_dragging = false;
static std::string dragged_scene_asset_name;

const float icon_size = 64.0f;
const float padding = 10.0f;
const float cell_size = icon_size + padding;

static void assign_entity_name(Entity& entity, const char* new_name) {
    if (!new_name || new_name[0] == '\0') return;
    entity.name = new_name;
}

static ModelAsset* find_asset_by_name(const std::string& asset_name) {
    for (auto& asset : assets) {
        if (asset.name == asset_name) return &asset;
    }
    return nullptr;
}

static bool has_valid_model_data(const Model& model) {
    return model.meshCount > 0 && model.meshes != nullptr;
}

static Entity make_entity_from_asset(Scene& scene, ModelAsset& asset) {
    Entity entity;
    entity.id = static_cast<int>(scene.entities.size());
    entity.type = asset.type;
    entity.asset = &asset;
    entity.asset_name = asset.name;
    entity.segments = 16;
    const std::string base_name = asset.is_procedural ? object_type_name(asset.type) : fs::path(asset.name).stem().string();
    entity.name = scene.make_unique_name(base_name.empty() ? "Model" : base_name);

    if (asset.is_procedural) {
        entity.model = asset.generator(entity.segments);
        entity.owns_model_instance = true;
        store_uv(&entity);
        store_material_textures(&entity);
        entity.texture_source = TEXTURE_NONE;
        entity.texture_name.clear();
    } else {
        if (!load_model_instance(asset, entity.model)) {
            entity.asset = nullptr;
            entity.asset_name.clear();
            entity.model = {0};
            return entity;
        }

        entity.owns_model_instance = true;
        store_uv(&entity);
        store_material_textures(&entity);

        bool has_embedded = false;
        for (int i = 0; i < entity.model.materialCount; i++) {
            if (entity.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                has_embedded = true;
                break;
            }
        }

        if (has_embedded) entity.texture_source = TEXTURE_MODEL;
        else entity.texture_source = TEXTURE_NONE;
    }

    entity.texture = {0};
    return entity;
}

static Vector3 get_scene_drop_position(Camera3D camera) {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    const float epsilon = 0.0001f;

    if (fabsf(ray.direction.y) > epsilon) {
        const float t = -ray.position.y / ray.direction.y;
        if (t >= 0.0f) {
            return {
                ray.position.x + ray.direction.x * t,
                0.0f,
                ray.position.z + ray.direction.z * t
            };
        }
    }

    return {
        camera.position.x + camera.target.x,
        0.0f,
        camera.position.z + camera.target.z
    };
}

static bool import_path_to_resources(const fs::path& src, const fs::path& resource_dir) {
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

void Editor::save_state() {
    SceneState state;
    state.selected = scene.selected;

    for (auto e : scene.entities) {
        e.model.materials = nullptr;
        e.model.meshMaterial = nullptr;
        e.owns_materials = false;
        state.entities.push_back(e);
    }

    undo_stack.push(state);
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::undo() {
    if (undo_stack.empty()) return;

    SceneState current;
    current.selected = scene.selected;
    for (auto e : scene.entities) {
        e.model.materials = nullptr;
        e.model.meshMaterial = nullptr;
        e.owns_materials = false;
        current.entities.push_back(e);
    }

    redo_stack.push(current);

    SceneState prev = undo_stack.top();
    undo_stack.pop();
    scene.entities = prev.entities;
    scene.selected = prev.selected;

    for (auto& e : scene.entities) {
        if (!e.asset) continue;

        if (e.asset->is_procedural) {
            e.model = e.asset->generator(e.segments);
            e.owns_model_instance = true;
            store_uv(&e);
            store_material_textures(&e);
        } 
        
        else {
            if (!load_model_instance(*e.asset, e.model)) {
                e.asset = nullptr;
                e.asset_name.clear();
                e.model = {0};
                e.owns_model_instance = false;
                continue;
            }
            e.owns_model_instance = true;
            store_uv(&e);
            store_material_textures(&e);
        }

        e.shader_assigned = false;
    }
}

void Editor::redo() {
    if (redo_stack.empty()) return;

    SceneState current;
    current.selected = scene.selected;
    for (auto e : scene.entities) {
        e.model.materials = nullptr;
        e.model.meshMaterial = nullptr;
        e.owns_materials = false;
        current.entities.push_back(e);
    }
    
    undo_stack.push(current);

    SceneState next = redo_stack.top();
    redo_stack.pop();
    scene.entities = next.entities;
    scene.selected = next.selected;

    for (auto& e : scene.entities) {
        if (!e.asset) continue;
        if (e.asset->is_procedural) {
            e.model = e.asset->generator(e.segments);
            e.owns_model_instance = true;
            store_uv(&e);
            store_material_textures(&e);
        } 
        
        else {
            if (!load_model_instance(*e.asset, e.model)) {
                e.asset = nullptr;
                e.asset_name.clear();
                e.model = {0};
                e.owns_model_instance = false;
                continue;
            }
            e.owns_model_instance = true;
            store_uv(&e);
            store_material_textures(&e);
        }
        e.shader_assigned = false;
    }
}

void Editor::handle_input() {
    float speed = 0.1f;
    Entity* e = scene.get_selected();

    if (IsKeyPressed(KEY_P)) gizmo_mode = ImGuizmo::TRANSLATE;
    if (IsKeyPressed(KEY_R)) gizmo_mode = ImGuizmo::ROTATE;
    if (IsKeyPressed(KEY_S)) gizmo_mode = ImGuizmo::SCALE;

    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        fs::path resource_dir = fs::path(project_path) / "resources";
        std::error_code ec;
        fs::create_directories(resource_dir, ec);

        bool imported_any = false;
        for (unsigned int i = 0; i < dropped.count; i++) {
            fs::path src(dropped.paths[i]);
            imported_any = import_path_to_resources(src, resource_dir) || imported_any;
        }

        UnloadDroppedFiles(dropped);

        if (imported_any) {
            save_state();
            refresh_textures(&scene, project_path);
            refresh_assets(project_path);
            refresh_models(project_path, scene);
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (!io.WantCaptureKeyboard && ctrl && IsKeyPressed(KEY_S)) {
        project_save(project_path, scene);
    }

    static float last_undo_time = 0;
    static float last_redo_time = 0;
    static bool undo_key_was_pressed = false;
    static bool redo_key_was_pressed = false;
    static float undo_hold_start = 0;
    static float redo_hold_start = 0;
    float current_time = GetTime();
    
    if (ctrl && IsKeyDown(KEY_Z)) {
        if (!undo_key_was_pressed) {
            undo();
            undo_key_was_pressed = true;
            undo_hold_start = current_time;
            last_undo_time = current_time;
        } else if (current_time - undo_hold_start > 0.5f) {
            if (current_time - last_undo_time > 0.15f) {
                undo();
                last_undo_time = current_time;
            }
        }
    } else {
        undo_key_was_pressed = false;
        undo_hold_start = 0;
    }
    
    if (ctrl && IsKeyDown(KEY_Y)) {
        if (!redo_key_was_pressed) {
            redo();
            redo_key_was_pressed = true;
            redo_hold_start = current_time;
            last_redo_time = current_time;
        } else if (current_time - redo_hold_start > 0.5f) {
            if (current_time - last_redo_time > 0.15f) {
                redo();
                last_redo_time = current_time;
            }
        }
    } else {
        redo_key_was_pressed = false;
        redo_hold_start = 0;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) project_save(project_path, scene);
}

void Editor::draw_gizmo(Camera3D camera) {
    Entity* e = scene.get_selected();
    if (!e) return;

    ImGuizmo::BeginFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    Matrix view = GetCameraMatrix(camera);
    Matrix proj = MatrixPerspective(camera.fovy * DEG2RAD, io.DisplaySize.x / io.DisplaySize.y, 0.1f, 1000.0f);

    view = MatrixTranspose(view);
    proj = MatrixTranspose(proj);

    float view_mat[16], proj_mat[16];

    memcpy(view_mat, &view, sizeof(view_mat));
    memcpy(proj_mat, &proj, sizeof(proj_mat));

    float t[3] = { e->position.x, e->position.y, e->position.z };
    float r[3] = { e->rotation.x, e->rotation.y, e->rotation.z };
    float s[3] = { e->scale.x, e->scale.y, e->scale.z };
    float matrix[16];

    ImGuizmo::RecomposeMatrixFromComponents(t, r, s, matrix);
    ImGuizmo::Manipulate(view_mat, proj_mat, gizmo_mode, ImGuizmo::WORLD, matrix);

    static bool was_using = false;

    if (ImGuizmo::IsUsing() && !was_using) {
        save_state();
    }

    if (ImGuizmo::IsUsing()) {
        float nt[3], nr[3], ns[3];
        ImGuizmo::DecomposeMatrixToComponents(matrix, nt, nr, ns);
        e->position = { nt[0], nt[1], nt[2] };
        e->rotation = { nr[0], nr[1], nr[2] };
        e->scale    = { ns[0], ns[1], ns[2] };
    }

    was_using = ImGuizmo::IsUsing();
}

void Editor::handle_scene_asset_drop(Camera3D camera) {
    if (!scene_asset_dragging) return;

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return;

    const std::string asset_name = dragged_scene_asset_name;
    scene_asset_dragging = false;
    dragged_scene_asset_name.clear();

    if (ImGuizmo::IsUsing()) return;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) return;

    ModelAsset* asset = find_asset_by_name(asset_name);
    if (!asset) return;

    Entity entity = make_entity_from_asset(scene, *asset);
    if (!has_valid_model_data(entity.model)) return;

    save_state();
    entity.position = get_scene_drop_position(camera);

    scene.entities.push_back(entity);
    scene.selected = static_cast<int>(scene.entities.size()) - 1;
}

void Editor::draw_ui(Shader shader) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                project_save(project_path, scene);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit")) {
                CloseWindow();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    float menu_bar_height = ImGui::GetFrameHeight();

    ImGui::SetNextWindowSize(ImVec2(150, 520), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 5 + menu_bar_height), ImGuiCond_Once);
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    for (int i = 0; i < scene.entities.size(); i++) {
        Entity& ent = scene.entities[i];

        ImGui::PushID(i);

        bool selected = (scene.selected == i);
        if (ImGui::Selectable(ent.name.c_str(), selected))
            scene.selected = i;

        if (ImGui::BeginPopupContextItem(TextFormat("context_%d", ent.id))) {
            if (ImGui::MenuItem("Delete")) {
                save_state();

                Entity& ent = scene.entities[i];

                if (ent.has_light && ent.light_created) {
                    ent.light.enabled = false;
                    if (ent.light.id != -1) update_lighting(shader, ent.light);
                    free_light_id(ent.light.id);
                }

                scene.entities.erase(scene.entities.begin() + i);

                if (scene.selected == i) scene.selected = -1;
                else if (scene.selected > i) scene.selected--;

                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }

            if (ImGui::MenuItem("Rename")) {
                save_state();

                renaming_index = i;
                const size_t copied = ent.name.copy(rename_buf, sizeof(rename_buf) - 1);
                rename_buf[copied] = '\0';
            }

            if (ImGui::MenuItem("Duplicate")) {
                save_state();

                Entity ent_copy = ent;
                ent_copy.id = static_cast<int>(scene.entities.size());
                ent_copy.name = scene.make_default_name_for(ent_copy);

                if (ent_copy.has_light) {
                    ent_copy.light_created = false;
                    ent_copy.light.id = -1;
                    ent_copy.light.light = {0};
                    ent_copy.light.intensity = ent.light.intensity;
                    ent_copy.light.range = ent.light.range;
                }

                scene.entities.push_back(ent_copy);
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("Create")) {
            save_state();

            for (int asset_index = 0; asset_index < static_cast<int>(assets.size()); asset_index++) {
                auto& a = assets[asset_index];
                const std::string menu_label = a.name + "##create_" + std::to_string(asset_index);
                if (ImGui::MenuItem(menu_label.c_str())) {
                    Entity e = make_entity_from_asset(scene, a);
                    if (!has_valid_model_data(e.model)) continue;
                    scene.entities.push_back(e);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::End();

    if (renaming_index != -1) { 
        ImGui::OpenPopup("Rename"); 
    }
    
    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename", rename_buf, IM_ARRAYSIZE(rename_buf));
        if (ImGui::Button("OK")) {
            if (renaming_index >= 0 && renaming_index < static_cast<int>(scene.entities.size())) {
                assign_entity_name(scene.entities[renaming_index], rename_buf);
            }
            renaming_index = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            renaming_index = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(225, 520), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(1050, 5 + menu_bar_height), ImGuiCond_Once);

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::Text("Mode"); ImGui::SameLine();

    if (ImGui::Button("P")) gizmo_mode = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::Button("R")) gizmo_mode = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::Button("S")) gizmo_mode = ImGuizmo::SCALE;

    Entity* e = scene.get_selected();
    if (e) {
        ImGui::Separator();
        char inspector_name[128] = {};
        const size_t copied = e->name.copy(inspector_name, sizeof(inspector_name) - 1);
        inspector_name[copied] = '\0';
        
        static std::string last_name;
        if (ImGui::InputText("Name", inspector_name, IM_ARRAYSIZE(inspector_name))) {
            if (last_name != inspector_name) {
                save_state();
                assign_entity_name(*e, inspector_name);
                last_name = inspector_name;
            }
        } else {
            last_name = inspector_name;
        }

        ImGui::Separator();
        ImGui::Text("Transform");
        float pos[3] = { e->position.x, e->position.y, e->position.z };
        float rot[3] = { e->rotation.x, e->rotation.y, e->rotation.z };
        float scl[3] = { e->scale.x, e->scale.y, e->scale.z };

        static Vector3 last_pos, last_rot, last_scl;
        
        if (ImGui::DragFloat3("Position", pos, 0.1f)) {
            if (last_pos.x != pos[0] || last_pos.y != pos[1] || last_pos.z != pos[2]) {
                save_state();
                last_pos = {pos[0], pos[1], pos[2]};
            }
            e->position = { pos[0], pos[1], pos[2] };
        }
        
        if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
            if (last_rot.x != rot[0] || last_rot.y != rot[1] || last_rot.z != rot[2]) {
                save_state();
                last_rot = {rot[0], rot[1], rot[2]};
            }
            e->rotation = { rot[0], rot[1], rot[2] };
        }
        
        if (ImGui::DragFloat3("Scale", scl, 0.1f)) {
            if (last_scl.x != scl[0] || last_scl.y != scl[1] || last_scl.z != scl[2]) {
                save_state();
                last_scl = {scl[0], scl[1], scl[2]};
            }
            e->scale = { scl[0], scl[1], scl[2] };
        }

        ImGui::Separator();
        ImGui::Text("Mesh");

        if (e->asset && e->asset->is_procedural) {
            int max_seg = 125;
            if (e->type == SPHERE || e->type == HEMISPHERE) max_seg = 100;

            static int last_segments = 0;
            if (ImGui::DragInt("Segments", &e->segments, 1, 3, max_seg)) {
                if (last_segments != e->segments) {
                    save_state();
                    last_segments = e->segments;
                    
                    update_model(e);
                    store_uv(e);
                    e->shader_assigned = false;
                }
            }
        }
        

        static int current_model_index = 0;
        std::vector<const char*> model_names;
        model_names.reserve(assets.size());
        for (int i = 0; i < static_cast<int>(assets.size()); i++) {
            model_names.push_back(assets[i].name.c_str());
            model_names[i] = assets[i].name.c_str();
            if (e->asset_name == assets[i].name) current_model_index = i;
        }

        static int last_model_index = -1;
        if (!model_names.empty() && ImGui::Combo("Model", &current_model_index, model_names.data(), static_cast<int>(model_names.size()))) {
            if (last_model_index != current_model_index) {
                save_state();
                last_model_index = current_model_index;

                const bool owns_current_model = entity_owns_model(*e);
                if (owns_current_model && e->model.meshCount > 0) {
                    UnloadModel(e->model);
                }
                e->model = {0};

                e->original_texcoords.clear();
                e->original_material_textures.clear();

                e->asset = &assets[current_model_index];
                e->asset_name = e->asset->name;
                e->type  = e->asset->type;

                e->shader_assigned = false;

                if (e->asset->is_procedural) { 
                    e->segments = 16; 
                    update_model(e); 
                    e->owns_model_instance = true;
                    store_uv(e);
                    store_material_textures(e);
                    e->texture_source = TEXTURE_NONE;
                    e->texture_name.clear();
                } 
                
                else {
                    if (!load_model_instance(*e->asset, e->model)) {
                        e->asset = nullptr;
                        e->asset_name.clear();
                        e->model = {0};
                        e->owns_model_instance = false;
                        e->texture_source = TEXTURE_NONE;
                        e->texture_name.clear();
                        return;
                    }

                    e->owns_model_instance = true;
                    store_uv(e);
                    store_material_textures(e);

                    bool has_embedded = false;
                    for (int i = 0; i < e->model.materialCount; i++) {
                        if (e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                            has_embedded = true;
                            break;
                        }
                    }

                    if (has_embedded) {
                        e->texture_source = TEXTURE_MODEL;
                        e->texture_name.clear();
                    } else {
                        e->texture_source = TEXTURE_NONE;
                        e->texture_name.clear();
                    }
                }
            }
        }

        int current_texture_index = 0;
        std::vector<const char*> texture_names;
        std::vector<TextureSource> texture_types;
        std::vector<std::string> texture_sources;

        texture_names.push_back("None");
        texture_types.push_back(TEXTURE_NONE);
        texture_sources.push_back("");

        for (int i = 1; i < static_cast<int>(texture_options.size()); i++) {
            texture_names.push_back(texture_options[i].name.c_str());
            texture_types.push_back(TEXTURE_EXTERNAL);
            texture_sources.push_back(texture_options[i].name);
        }

        bool has_model_texture = false;
        static std::string model_texture_label;
        if (e->asset && !e->asset->is_procedural) {
            for (const auto& t : e->original_material_textures) {
                if (t.id != 0 && t.id != 1) {
                    has_model_texture = true;
                    break;
                }
            }
        }

        if (has_model_texture) {
            model_texture_label = TextFormat("%s [texture]", e->asset_name.c_str());
            texture_names.push_back(model_texture_label.c_str());
            texture_types.push_back(TEXTURE_MODEL);
            texture_sources.push_back("__model__");
        }

        for (int i = 0; i < static_cast<int>(texture_types.size()); i++) {
            if (texture_types[i] == e->texture_source) {
                if (e->texture_source == TEXTURE_EXTERNAL && e->texture_name == texture_sources[i]) {
                    current_texture_index = i;
                    break;
                }
                if (e->texture_source != TEXTURE_EXTERNAL) {
                    current_texture_index = i;
                    break;
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Material");

        if (current_texture_index >= static_cast<int>(texture_names.size()))
            current_texture_index = 0;

        if (ImGui::Combo("Texture", &current_texture_index, texture_names.data(), static_cast<int>(texture_names.size()))) {
            save_state();

            TextureSource selected_type = texture_types[current_texture_index];
            if (selected_type == TEXTURE_NONE) {
                e->texture_source = TEXTURE_NONE;
                e->texture_name.clear();
                e->texture = {0};
                clear_material_textures(e);
            } 
            
            else if (selected_type == TEXTURE_EXTERNAL) {
                e->texture_source = TEXTURE_EXTERNAL;
                e->texture_name = texture_sources[current_texture_index];
                e->texture = {0};

                for (int i = 1; i < static_cast<int>(texture_options.size()); i++) {
                    if (texture_options[i].name == e->texture_name) {
                        e->texture = texture_options[i].texture;
                        break;
                    }
                }

                for (int i = 0; i < e->model.materialCount; i++) {
                    e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e->texture;
                }
            } 
            
            else if (selected_type == TEXTURE_MODEL) {
                e->texture_source = TEXTURE_MODEL;
                e->texture_name = "";
                e->texture = {0};
                restore_model_textures(e);
            }
        }

        static bool last_stretch_texture = false;
        if (ImGui::Checkbox("Stretch Texture", &e->texture_stretch)) {
            if (last_stretch_texture != e->texture_stretch) {
                save_state();
                last_stretch_texture = e->texture_stretch;
            }
        }

        if (!e->texture_stretch) {
            static bool last_auto_uv = false;
            if (ImGui::Checkbox("Auto UV", &e->auto_uv)) {
                if (last_auto_uv != e->auto_uv) {
                    save_state();
                    last_auto_uv = e->auto_uv;
                }
            }

            if (e->auto_uv) {
                static Vector2 last_uv_scale = {0, 0};
                if (ImGui::InputFloat("Scale X", &e->uv_scale_vec.x, 0.1f, 1.0f, "%.2f")) {
                    if (last_uv_scale.x != e->uv_scale_vec.x) {
                        save_state();
                        last_uv_scale.x = e->uv_scale_vec.x;
                    }
                    if (e->uv_scale_vec.x < 0.01f) e->uv_scale_vec.x = 0.01f;
                }
                
                if (ImGui::InputFloat("Scale Y", &e->uv_scale_vec.y, 0.1f, 1.0f, "%.2f")) {
                    if (last_uv_scale.y != e->uv_scale_vec.y) {
                        save_state();
                        last_uv_scale.y = e->uv_scale_vec.y;
                    }
                    if (e->uv_scale_vec.y < 0.01f) e->uv_scale_vec.y = 0.01f;
                }
            }

            else {
                static float last_repeat_u = 0, last_repeat_v = 0;
                if (ImGui::InputFloat("Repeat U", &e->texture_repeat_u, 0.1f, 1.0f, "%.2f")) {
                    if (last_repeat_u != e->texture_repeat_u) {
                        save_state();
                        last_repeat_u = e->texture_repeat_u;
                    }
                    if (e->texture_repeat_u < 0.01f) e->texture_repeat_u = 0.01f;
                }
                
                if (ImGui::InputFloat("Repeat V", &e->texture_repeat_v, 0.1f, 1.0f, "%.2f")) {
                    if (last_repeat_v != e->texture_repeat_v) {
                        save_state();
                        last_repeat_v = e->texture_repeat_v;
                    }
                    if (e->texture_repeat_v < 0.01f) e->texture_repeat_v = 0.01f;
                }
            }
        }

        float color[4] = { e->color.r / 255.f, e->color.g / 255.f, e->color.b / 255.f, e->color.a / 255.f };
        static Color last_color;
        if (ImGui::ColorEdit4("Color", color)) {
            Color new_color = {
                (unsigned char)(color[0]*255),
                (unsigned char)(color[1]*255),
                (unsigned char)(color[2]*255),
                (unsigned char)(color[3]*255)
            };
            if (last_color.r != new_color.r || last_color.g != new_color.g || 
                last_color.b != new_color.b || last_color.a != new_color.a) {
                save_state();
                last_color = new_color;
                e->color = new_color;
            }
        }

        float outline[4] = { e->outline_color.r / 255.f, e->outline_color.g / 255.f, e->outline_color.b / 255.f, e->outline_color.a / 255.f };
        static Color last_outline;
        if (ImGui::ColorEdit4("Outline", outline)) {
            Color new_outline = {
                (unsigned char)(outline[0]*255),
                (unsigned char)(outline[1]*255),
                (unsigned char)(outline[2]*255),
                (unsigned char)(outline[3]*255)
            };
            if (last_outline.r != new_outline.r || last_outline.g != new_outline.g ||
                last_outline.b != new_outline.b || last_outline.a != new_outline.a) {
                save_state();
                last_outline = new_outline;
                e->outline_color = new_outline;
            }
        } 

        ImGui::Separator();
        ImGui::Text("Lighting");

        static bool last_has_light = false;
        if (ImGui::Checkbox("Has Light", &e->has_light)) {
            if (last_has_light != e->has_light) {
                save_state();
                last_has_light = e->has_light;
            }
        }

        if (e->has_light)
        {
            float light_color[4] = {
                e->light.color.r / 255.f,
                e->light.color.g / 255.f,
                e->light.color.b / 255.f,
                e->light.color.a / 255.f
            };

            static Color last_light_color;
            if (ImGui::ColorEdit4("Light Color", light_color))
            {
                Color new_light_color = {
                    (unsigned char)(light_color[0]*255),
                    (unsigned char)(light_color[1]*255),
                    (unsigned char)(light_color[2]*255),
                    (unsigned char)(light_color[3]*255)
                };
                if (last_light_color.r != new_light_color.r || last_light_color.g != new_light_color.g ||
                    last_light_color.b != new_light_color.b || last_light_color.a != new_light_color.a) {
                    save_state();
                    last_light_color = new_light_color;
                    e->light.color = new_light_color;
                }
            }

            static float last_intensity = 0, last_range = 0;
            if (ImGui::InputFloat("Intensity", &e->light.intensity, 0.1f, 1.0f, "%.2f")) {
                if (last_intensity != e->light.intensity) {
                    save_state();
                    last_intensity = e->light.intensity;
                }
            }
            
            if (ImGui::InputFloat("Range", &e->light.range, 0.1f, 1.0f, "%.2f")) {
                if (last_range != e->light.range) {
                    save_state();
                    last_range = e->light.range;
                }
            }

            if (!e->light_created)
            {
                int new_id = allocate_light_id();
                if (new_id == -1) { e->has_light = false; }
                else
                {
                    e->light = create_lighting(e->position, e->light.color);
                    e->light.id = new_id;

                    e->light.light = create_light_at_slot(new_id, LIGHT_POINT, e->position, Vector3Zero(), e->light.color, shader);
                    e->light_created = true;
                }
            }
        }

        else if (e->light_created)
        {
            e->light.enabled = false;
            if (e->light.id != -1)
            {
                update_lighting(shader, e->light);
            }
        }
    }

    ImGui::End();

    draw_assets_ui();
}

void Editor::draw_assets_ui() {
    ImGui::SetNextWindowSize(ImVec2(1270, 165), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 550), ImGuiCond_Once);
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    static ImVec2 selection_start;
    static ImVec2 selection_end;
    static bool selecting = false;
    static int rename_target = -1;

    ImVec2 window_size = ImGui::GetWindowSize();

    if (asset_entries.empty()) {
        const char* text = "Drag files here";
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

        for (int i = 0; i < asset_entries.size(); i++) {
            ImGui::PushID(i);
            ImGui::BeginGroup();

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size(icon_size, icon_size + 20.0f);

            ImGui::InvisibleButton("asset_btn", size);
            const bool asset_item_active = ImGui::IsItemActive();
            const bool asset_item_hovered = ImGui::IsItemHovered();

            if (asset_item_hovered) ImGui::SetTooltip("%s", asset_entries[i].filename.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0)) selected_asset_index = i;

            if (ImGui::BeginPopupContextItem("AssetContext")) {
                if (ImGui::MenuItem("Delete")) {
                    save_state();

                    fs::path target = fs::path(project_path) / "resources" / asset_entries[i].filename;
                    std::error_code ec;
                    if (asset_entries[i].is_directory) fs::remove_all(target, ec);
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
                    size_t copied = asset_entries[i].filename.copy(rename_buf, sizeof(rename_buf) - 1);
                    rename_buf[copied] = '\0';

                    ImGui::OpenPopup("RenameAsset");
                }

                ImGui::EndPopup();
            }

            bool selected = (selected_asset_index == i);

            if (selecting && (fabs(selection_start.x - selection_end.x) > 5.0f || fabs(selection_start.y - selection_end.y) > 5.0f)) {
                ImVec2 min = ImVec2(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
                ImVec2 max = ImVec2(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));

                if (!(pos.x + size.x < min.x || pos.x > max.x || pos.y + size.y < min.y || pos.y > max.y)) selected = true;
            }

            if (selected) {
                ImVec2 frame_max = ImVec2(pos.x + size.x, pos.y + size.y);
                ImGui::GetWindowDrawList()->AddRectFilled(pos, frame_max, IM_COL32(80, 140, 255, 150));
            }

            ImGui::SetCursorScreenPos(pos);
            if (asset_entries[i].is_image) ImGui::Image((void*)(intptr_t)asset_entries[i].texture.id, ImVec2(icon_size, icon_size));
            else if (asset_entries[i].is_directory) ImGui::Button("Folder", ImVec2(icon_size, icon_size));
            else ImGui::Button("File", ImVec2(icon_size, icon_size));

            if (!asset_entries[i].is_directory && is_model_file(fs::path(asset_entries[i].filename))) {
                if (asset_item_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    scene_asset_dragging = true;
                    dragged_scene_asset_name = asset_entries[i].filename;
                }

                if (scene_asset_dragging && dragged_scene_asset_name == asset_entries[i].filename) {
                    ImGui::SetTooltip("Spawn %s", dragged_scene_asset_name.c_str());
                }
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + icon_size));
            std::string label = asset_entries[i].filename;

            if (label.size() > 10) label = label.substr(0, 8) + "...";

            ImGui::TextWrapped("%s", label.c_str());
            ImGui::EndGroup();

            float last_x2 = ImGui::GetItemRectMax().x;
            float next_x2 = last_x2 + ImGui::GetStyle().ItemSpacing.x + icon_size;

            if (i + 1 < asset_entries.size() && next_x2 < window_visible_x2) ImGui::SameLine();

            ImGui::PopID();
        }

        if (selecting && (fabs(selection_start.x - selection_end.x) > 5.0f || fabs(selection_start.y - selection_end.y) > 5.0f)) {
            ImVec2 min = ImVec2(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
            ImVec2 max = ImVec2(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));

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
                    
                    if (selected_asset_index >= 0 && selected_asset_index < asset_entries.size()) {
                        fs::path resource_dir = fs::path(project_path) / "resources";
                        fs::path old_path = resource_dir / asset_entries[selected_asset_index].filename;
                        fs::path new_path = resource_dir / rename_buf;

                        if (rename_buf[0] != '\0' && old_path != new_path && fs::exists(old_path)) {
                            try {
                                fs::rename(old_path, new_path);
                                refresh_textures(&scene, project_path);
                                refresh_assets(project_path);
                                refresh_models(project_path, scene);
                                selected_asset_index = -1;
                            } 
                            
                            catch (...) {}
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
