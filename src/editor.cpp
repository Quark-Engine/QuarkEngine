#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include "headers/editor.h"
#include "headers/lighting.h"
#include "headers/entity.h"
#include "headers/ImGuizmo.h"
#include "headers/project.h"
#include <cstdlib>
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
#include <algorithm>
#include <cfloat>

namespace fs = std::filesystem;

static ImGuizmo::OPERATION gizmo_mode = ImGuizmo::TRANSLATE;
static int renaming_index = -1;
static char rename_buf[128] = "";
static bool scene_asset_dragging = false;
static std::string dragged_scene_asset_name;
static std::unordered_map<std::string, Texture> tex_cache;
static std::unordered_map<std::string, Texture> model_preview_cache;
static std::unordered_map<std::string, RenderTexture2D> model_render_cache;

static bool show_model_viewer = false;
static Model viewer_model = { 0 };
static RenderTexture2D viewer_rt = { 0 };
static Vector3 viewer_target = { 0, 0, 0 };
static Vector3 viewer_model_center = { 0, 0, 0 };
static Vector3 viewer_model_rotation = { 0, 0, 0 };
static float viewer_phi = 20.0f, viewer_theta = 45.0f, viewer_radius = 5.0f;

const float icon_size = 64.0f;
const float padding = 10.0f;
const float cell_size = icon_size + padding;

struct LocalEntry {
    std::string filename;
    bool is_directory;
    bool is_image;
    bool is_model;
    Texture texture;
    std::string extension;
};

static void draw_model_viewer_window() {
    if (!show_model_viewer) {
        if (viewer_model.meshCount > 0) {
            UnloadModel(viewer_model);
            viewer_model = { 0 };
        }
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Model Preview", &show_model_viewer)) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1) size.x = 1;
        if (size.y < 1) size.y = 1;

        if (viewer_rt.id == 0 || viewer_rt.texture.width != (int)size.x || viewer_rt.texture.height != (int)size.y) {
            if (viewer_rt.id != 0) UnloadRenderTexture(viewer_rt);
            viewer_rt = LoadRenderTexture((int)size.x, (int)size.y);
        }

        ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("ModelViewport", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        if (is_hovered) {
            viewer_radius -= ImGui::GetIO().MouseWheel * 1.5f;
            if (viewer_radius < 0.1f) viewer_radius = 0.1f;
        }

        if (is_active) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                viewer_model_rotation.y += delta.x * 0.5f;
                viewer_model_rotation.x += delta.y * 0.5f;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                viewer_theta -= delta.x * 0.5f; 
                viewer_phi -= delta.y * 0.5f;
            }


            if (viewer_phi > 89.0f) viewer_phi = 89.0f;
            if (viewer_phi < -89.0f) viewer_phi = -89.0f;
        }

        Camera3D cam = { 0 };
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        cam.target = viewer_target;
        cam.up = { 0, 1, 0 };
        cam.position.x = viewer_target.x + viewer_radius * cosf(viewer_phi * DEG2RAD) * sinf(viewer_theta * DEG2RAD);
        cam.position.y = viewer_target.y + viewer_radius * sinf(viewer_phi * DEG2RAD);
        cam.position.z = viewer_target.z + viewer_radius * cosf(viewer_phi * DEG2RAD) * cosf(viewer_theta * DEG2RAD);

        BeginTextureMode(viewer_rt);
        ClearBackground({ 40, 40, 45, 255 });
        BeginMode3D(cam);
        if (viewer_model.meshCount > 0) {
            Matrix matCenter = MatrixTranslate(-viewer_model_center.x, -viewer_model_center.y, -viewer_model_center.z);
            Matrix matRotation = MatrixRotateXYZ({viewer_model_rotation.x * DEG2RAD, viewer_model_rotation.y * DEG2RAD, 0});
            viewer_model.transform = MatrixMultiply(matCenter, matRotation);

            DrawModel(viewer_model, { 0, 0, 0 }, 1.0f, WHITE);
            DrawModelWires(viewer_model, { 0, 0, 0 }, 1.0f, DARKGRAY);
        }
        DrawGrid(10, 1.0f);
        EndMode3D();
        EndTextureMode();

        ImGui::SetCursorScreenPos(viewport_pos);
        Rectangle src = { 0, 0, (float)viewer_rt.texture.width, -(float)viewer_rt.texture.height };
        rlImGuiImageRect(&viewer_rt.texture, (int)size.x, (int)size.y, src);
    }
    ImGui::End();
}

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

static std::string get_asset_name_for_path(const fs::path& project_path_value, const fs::path& asset_path) {
    std::error_code ec;
    const fs::path resource_dir = project_path_value / "resources";
    const fs::path relative = fs::relative(asset_path, resource_dir, ec);
    if (!ec) return relative.generic_string();
    return asset_path.filename().generic_string();
}

static ModelAsset* find_asset_by_path(const fs::path& full_path, const fs::path& project_path_value) {
    std::string asset_name = get_asset_name_for_path(project_path_value, full_path);
    return find_asset_by_name(asset_name);
}

static std::string build_resource_signature(const fs::path& resource_dir) {
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

static Texture create_model_preview(const ModelAsset& asset, const std::string& cache_key, int preview_size = 64) {
    Texture result = {0};
    
    Model preview_model = {0};
    if (!load_model_instance(asset, preview_model)) {
        return result;
    }
    
    if (!has_valid_model_data(preview_model)) {
        UnloadModel(preview_model);
        return result;
    }
    
    RenderTexture2D render_texture = LoadRenderTexture(preview_size, preview_size);
    if (render_texture.id == 0) {
        UnloadModel(preview_model);
        return result;
    }
    
    Vector3 min_bound = {FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 max_bound = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    bool has_vertices = false;
    
    for (int m = 0; m < preview_model.meshCount; m++) {
        Mesh& mesh = preview_model.meshes[m];
        if (!mesh.vertices) continue;
        
        for (int v = 0; v < mesh.vertexCount; v++) {
            float vx = mesh.vertices[v * 3];
            float vy = mesh.vertices[v * 3 + 1];
            float vz = mesh.vertices[v * 3 + 2];
            
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
        
        Vector3 size = {
            max_bound.x - min_bound.x,
            max_bound.y - min_bound.y,
            max_bound.z - min_bound.z
        };
        
        float max_size = fmaxf(fmaxf(size.x, size.y), size.z);
        if (max_size < 0.1f) max_size = 1.0f;
        distance = max_size * 2.0f;
    }
    Camera3D preview_camera = {0};
    preview_camera.position = {center.x + distance * 0.6f, center.y + distance * 0.5f, center.z + distance * 0.6f};
    preview_camera.target = center;
    preview_camera.up = {0.0f, 1.0f, 0.0f};
    preview_camera.fovy = 45.0f;
    preview_camera.projection = CAMERA_PERSPECTIVE;
    
    BeginTextureMode(render_texture);
    {
        ClearBackground({32, 32, 40, 255});
        
        BeginMode3D(preview_camera);
        {
            DrawModel(preview_model, {0, 0, 0}, 1.0f, WHITE);
        }
        EndMode3D();
    }
    EndTextureMode();
    
    result = render_texture.texture;
    
    model_render_cache[cache_key] = render_texture;
    
    UnloadModel(preview_model);
    
    return result;
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
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);
        entity.texture_source = TEXTURE_NONE;
        entity.texture_name.clear();
    } 
    
    else {
        if (!load_model_instance(asset, entity.model)) {
            entity.asset = nullptr;
            entity.asset_name.clear();
            entity.model = {0};
            return entity;
        }

        entity.owns_model_instance = true;
        clear_mesh_overrides(entity);
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
            apply_mesh_overrides(e);
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
            apply_mesh_overrides(e);
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
            apply_mesh_overrides(e);
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
            apply_mesh_overrides(e);
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
        bool imported_any = false;

        if (current_asset_path.empty())
            current_asset_path = fs::path(project_path);

        std::error_code ec;
        fs::create_directories(current_asset_path, ec);

        for (unsigned int i = 0; i < dropped.count; i++) {
            fs::path src(dropped.paths[i]);
            imported_any = import_path_to_resources(src, current_asset_path) || imported_any;
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
        } 
        
        else if (current_time - undo_hold_start > 0.5f) {
            if (current_time - last_undo_time > 0.15f) {
                undo();
                last_undo_time = current_time;
            }
        }
    } 
    
    else {
        undo_key_was_pressed = false;
        undo_hold_start = 0;
    }
    
    if (ctrl && IsKeyDown(KEY_Y)) {
        if (!redo_key_was_pressed) {
            redo();
            redo_key_was_pressed = true;
            redo_hold_start = current_time;
            last_redo_time = current_time;
        } 
        
        else if (current_time - redo_hold_start > 0.5f) {
            if (current_time - last_redo_time > 0.15f) {
                redo();
                last_redo_time = current_time;
            }
        }
    } 
    
    else {
        redo_key_was_pressed = false;
        redo_hold_start = 0;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) project_save(project_path, scene);

    static double last_asset_poll = 0;
    double current_time_d = GetTime();

    if (current_time_d - last_asset_poll > 2.0) {
        last_asset_poll = current_time_d;

        fs::path resource_dir = fs::path(project_path) / "resources";
        if (fs::exists(resource_dir)) {
            static std::string last_resource_signature;
            const std::string current_signature = build_resource_signature(resource_dir);

            if (current_signature != last_resource_signature) {
                last_resource_signature = current_signature;
                refresh_textures(&scene, project_path);
                refresh_assets(project_path);
                refresh_models(project_path, scene);
            }
        }
    }
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
                    
                    clear_mesh_overrides(*e);
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
                clear_mesh_overrides(*e);

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

        if (has_valid_model_data(e->model)) {
            static int selected_mesh_index = 0;
            static int selected_triangle_index = 0;

            if (selected_mesh_index >= e->model.meshCount) selected_mesh_index = 0;
            if (selected_mesh_index < 0) selected_mesh_index = 0;

            if (e->model.meshCount > 1) {
                ImGui::SliderInt("Editable Mesh", &selected_mesh_index, 0, e->model.meshCount - 1);
            } else {
                ImGui::Text("Editable Mesh: 0");
            }

            Mesh& editable_mesh = e->model.meshes[selected_mesh_index];
            if (editable_mesh.triangleCount > 0) {
                if (selected_triangle_index >= editable_mesh.triangleCount) selected_triangle_index = editable_mesh.triangleCount - 1;
                if (selected_triangle_index < 0) selected_triangle_index = 0;

                ImGui::SliderInt("Triangle", &selected_triangle_index, 0, editable_mesh.triangleCount - 1);
                ImGui::Text("Vertices: %d  Triangles: %d", editable_mesh.vertexCount, editable_mesh.triangleCount);

                int triangle_vertices[3] = {};
                if (get_mesh_triangle_vertex_indices(editable_mesh, selected_triangle_index, triangle_vertices)) {
                    auto edit_triangle_vertex = [&](const char* label, int vertex_index) {
                        float vertex[3] = {
                            editable_mesh.vertices[vertex_index * 3 + 0],
                            editable_mesh.vertices[vertex_index * 3 + 1],
                            editable_mesh.vertices[vertex_index * 3 + 2]
                        };

                        if (ImGui::DragFloat3(label, vertex, 0.01f)) {
                            save_state();
                            if (!entity_has_mesh_overrides(*e)) {
                                if (!e->mesh_triangles_detached) {
                                    detach_mesh_triangles(*e);
                                }
                                capture_mesh_overrides_from_model(*e);
                            }

                            std::vector<float>& mesh_override = e->mesh_vertex_overrides[selected_mesh_index];
                            mesh_override[vertex_index * 3 + 0] = vertex[0];
                            mesh_override[vertex_index * 3 + 1] = vertex[1];
                            mesh_override[vertex_index * 3 + 2] = vertex[2];
                            apply_mesh_overrides(*e);
                        }
                    };

                    edit_triangle_vertex("Vertex A", triangle_vertices[0]);
                    edit_triangle_vertex("Vertex B", triangle_vertices[1]);
                    edit_triangle_vertex("Vertex C", triangle_vertices[2]);

                    if (!e->mesh_triangles_detached) {
                        ImGui::Text("Triangles are still shared until first edit.");
                    } else {
                        ImGui::Text("Triangle sculpt mode is active.");
                    }

                    if (entity_has_mesh_overrides(*e) && ImGui::Button("Reset Mesh Edits")) {
                        save_state();
                        clear_mesh_overrides(*e);

                        if (e->asset && e->asset->is_procedural) {
                            update_model(e);
                        } else if (e->asset) {
                            const bool owns_current_model = entity_owns_model(*e);
                            if (owns_current_model && e->model.meshCount > 0) {
                                UnloadModel(e->model);
                            }

                            e->model = {0};
                            if (!load_model_instance(*e->asset, e->model)) {
                                e->asset = nullptr;
                                e->asset_name.clear();
                                e->owns_model_instance = false;
                            } else {
                                e->owns_model_instance = true;
                            }
                        }

                        if (e->asset) {
                            store_uv(e);
                            store_material_textures(e);
                            e->shader_assigned = false;
                        }
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
    draw_model_viewer_window();
}

void Editor::draw_assets_ui() {
    ImGui::SetNextWindowSize(ImVec2(1270, 165), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 550), ImGuiCond_Once);
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

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
        e.is_directory = p.is_directory();
        e.is_image = is_image_file(p.path());
        e.is_model = is_model_file(p.path());

        fs::path fp = p.path();
        std::string ext = fp.extension().string();
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        e.extension = ext;

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
            if (entry.is_directory) ImGui::Button("Folder", ImVec2(icon_size, icon_size));
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
                        } else {
                            load_failed = true;
                        }
                    } else {
                        load_failed = true;
                    }
                }
                
                if (preview_tex.id != 0) {
                    ImGui::Image((void*)(intptr_t)preview_tex.id, ImVec2(icon_size, icon_size));
                } else {
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
                std::string ext = entry.extension;
                if (ext.empty()) ext = "file";
                ImGui::Button(ext.c_str(), ImVec2(icon_size, icon_size));
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
