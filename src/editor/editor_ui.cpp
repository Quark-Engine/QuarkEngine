#include "editor_ui.h"

#include "editor.h"
#include "editor_assets.h"
#include "editor_entity.h"
#include "editor_utils.h"
#include "editor_viewers.h"
#include "../headers/ImGuizmo.h"
#include "rlImGui.h"
#include "../headers/lighting.h"
#include "../headers/project.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "raymath.h"
#include "rlgl.h"
#include <cstring>
#include <cmath>
#include <vector>

extern RenderTexture2D scene_rt;
bool show_hierarchy = true;
bool show_inspector = true;
bool show_assets = true;
bool show_scene = true;

bool g_is_scene_hovered = false;
bool g_is_scene_active = false;

namespace {

ImVec2 g_scene_window_pos = { 0, 0 };
ImVec2 g_scene_window_size = { 0, 0 };

struct MeshEditState {
    bool enabled = false;
    int entity_index = -1;
    int mesh_index = 0;
    int triangle_index = 0;
    int vertex_corner = 0;
    bool was_using_gizmo = false;
};

MeshEditState g_mesh_edit_state;

Matrix compose_entity_transform_matrix(const Entity& entity) {
    Matrix transform = MatrixIdentity();
    transform = MatrixMultiply(transform, MatrixTranslate(entity.position.x, entity.position.y, entity.position.z));
    transform = MatrixMultiply(transform, MatrixRotateX(entity.rotation.x * DEG2RAD));
    transform = MatrixMultiply(transform, MatrixRotateY(entity.rotation.y * DEG2RAD));
    transform = MatrixMultiply(transform, MatrixRotateZ(entity.rotation.z * DEG2RAD));
    transform = MatrixMultiply(transform, MatrixScale(entity.scale.x, entity.scale.y, entity.scale.z));
    return transform;
}

void sync_mesh_edit_state(const Editor& editor) {
    if (editor.scene.selected != g_mesh_edit_state.entity_index) {
        g_mesh_edit_state.entity_index = editor.scene.selected;
        g_mesh_edit_state.mesh_index = 0;
        g_mesh_edit_state.triangle_index = 0;
        g_mesh_edit_state.vertex_corner = 0;
        g_mesh_edit_state.was_using_gizmo = false;
    }
}

bool get_selected_triangle_vertices(const Entity& entity, int mesh_index, int triangle_index, int out_indices[3]) {
    if (!has_valid_model_data(entity.model)) return false;
    if (mesh_index < 0 || mesh_index >= entity.model.meshCount) return false;

    const Mesh& mesh = entity.model.meshes[mesh_index];
    if (triangle_index < 0 || triangle_index >= mesh.triangleCount) return false;
    return get_mesh_triangle_vertex_indices(mesh, triangle_index, out_indices);
}

bool get_selected_vertex_index(const Entity& entity, int mesh_index, int triangle_index, int vertex_corner, int& out_vertex_index) {
    int triangle_vertices[3] = {};
    if (!get_selected_triangle_vertices(entity, mesh_index, triangle_index, triangle_vertices)) return false;
    if (vertex_corner < 0 || vertex_corner > 2) return false;
    out_vertex_index = triangle_vertices[vertex_corner];
    return true;
}

Vector3 get_mesh_vertex_local_position(const Entity& entity, int mesh_index, int vertex_index) {
    const Mesh& mesh = entity.model.meshes[mesh_index];
    return {
        mesh.vertices[vertex_index * 3 + 0],
        mesh.vertices[vertex_index * 3 + 1],
        mesh.vertices[vertex_index * 3 + 2]
    };
}

Vector3 get_mesh_vertex_world_position(const Entity& entity, int mesh_index, int vertex_index) {
    const Matrix transform = compose_entity_transform_matrix(entity);
    return Vector3Transform(get_mesh_vertex_local_position(entity, mesh_index, vertex_index), transform);
}

bool ensure_mesh_edit_ready(Entity& entity) {
    if (entity_has_mesh_overrides(entity)) return true;
    if (!entity.mesh_triangles_detached) detach_mesh_triangles(entity);
    capture_mesh_overrides_from_model(entity);
    return entity_has_mesh_overrides(entity);
}

bool set_mesh_vertex_local_position(Entity& entity, int mesh_index, int vertex_index, const Vector3& local_position) {
    if (!has_valid_model_data(entity.model)) return false;
    if (mesh_index < 0 || mesh_index >= entity.model.meshCount) return false;

    Mesh& mesh = entity.model.meshes[mesh_index];
    if (!mesh.vertices || vertex_index < 0 || vertex_index >= mesh.vertexCount) return false;
    if (!ensure_mesh_edit_ready(entity)) return false;
    if (mesh_index >= static_cast<int>(entity.mesh_vertex_overrides.size())) return false;

    std::vector<float>& mesh_override = entity.mesh_vertex_overrides[mesh_index];
    if (mesh_override.size() != static_cast<size_t>(mesh.vertexCount * 3)) return false;

    mesh_override[vertex_index * 3 + 0] = local_position.x;
    mesh_override[vertex_index * 3 + 1] = local_position.y;
    mesh_override[vertex_index * 3 + 2] = local_position.z;

    const bool applied = apply_mesh_overrides(entity);
    if (applied) {
        mark_entity_bounds_dirty(&entity);
        mark_entity_uv_dirty(&entity);
    }
    return applied;
}

bool set_mesh_vertex_world_position(Entity& entity, int mesh_index, int vertex_index, const Vector3& world_position) {
    const Matrix inverse_transform = MatrixInvert(compose_entity_transform_matrix(entity));
    const Vector3 local_position = Vector3Transform(world_position, inverse_transform);
    return set_mesh_vertex_local_position(entity, mesh_index, vertex_index, local_position);
}

bool pick_mesh_triangle(
    const Entity& entity,
    int mesh_index,
    Ray ray,
    int& out_triangle_index,
    int& out_vertex_corner
) {
    if (!has_valid_model_data(entity.model)) return false;
    if (mesh_index < 0 || mesh_index >= entity.model.meshCount) return false;

    const Mesh& mesh = entity.model.meshes[mesh_index];
    if (!mesh.vertices || mesh.triangleCount <= 0) return false;

    const Matrix transform = compose_entity_transform_matrix(entity);
    float best_distance = FLT_MAX;
    int best_triangle = -1;
    int best_corner = 0;

    for (int triangle_index = 0; triangle_index < mesh.triangleCount; triangle_index++) {
        int indices[3] = {};
        if (!get_mesh_triangle_vertex_indices(mesh, triangle_index, indices)) continue;

        Vector3 vertices[3] = {};
        for (int i = 0; i < 3; i++) {
            vertices[i] = Vector3Transform({
                mesh.vertices[indices[i] * 3 + 0],
                mesh.vertices[indices[i] * 3 + 1],
                mesh.vertices[indices[i] * 3 + 2]
            }, transform);
        }

        const RayCollision hit = GetRayCollisionTriangle(ray, vertices[0], vertices[1], vertices[2]);
        if (!hit.hit || hit.distance >= best_distance) continue;

        best_distance = hit.distance;
        best_triangle = triangle_index;

        float closest_corner_distance = FLT_MAX;
        for (int i = 0; i < 3; i++) {
            const float distance_to_corner = Vector3Distance(hit.point, vertices[i]);
            if (distance_to_corner < closest_corner_distance) {
                closest_corner_distance = distance_to_corner;
                best_corner = i;
            }
        }
    }

    if (best_triangle < 0) return false;

    out_triangle_index = best_triangle;
    out_vertex_corner = best_corner;
    return true;
}

void reset_mesh_edit_model(Entity& entity) {
    clear_mesh_overrides(entity);

    if (entity.asset && entity.asset->is_procedural) {
        update_model(&entity);
    } else if (entity.asset) {
        if (entity_owns_model(entity) && entity.model.meshCount > 0) {
            UnloadModel(entity.model);
        }

        entity.model = {0};
        if (!load_model_instance(*entity.asset, entity.model)) {
            entity.asset = nullptr;
            entity.asset_name.clear();
            entity.owns_model_instance = false;
        } else {
            entity.owns_model_instance = true;
        }
    }

    if (entity.asset) {
        store_uv(&entity);
        store_material_textures(&entity);
        entity.shader_assigned = false;
    }
}

void reset_editor_layout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

    ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
    ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
    ImGui::DockBuilderDockWindow("Assets", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Scene", dock_main_id);
    
    ImGui::DockBuilderFinish(dockspace_id);
    
    show_hierarchy = show_inspector = show_assets = show_scene = true;
}

void draw_mesh_vertex_overlay(Editor& editor, Camera3D camera) {
    sync_mesh_edit_state(editor);
    if (!g_mesh_edit_state.enabled) return;

    Entity* entity = editor.scene.get_selected();
    if (!entity || !has_valid_model_data(entity->model)) return;
    if (g_mesh_edit_state.mesh_index >= entity->model.meshCount) g_mesh_edit_state.mesh_index = 0;

    int triangle_vertices[3] = {};
    if (!get_selected_triangle_vertices(*entity, g_mesh_edit_state.mesh_index, g_mesh_edit_state.triangle_index, triangle_vertices)) return;

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    Vector2 screen_points[3] = {};
    Vector3 world_points[3] = {};

    for (int i = 0; i < 3; i++) {
        world_points[i] = get_mesh_vertex_world_position(*entity, g_mesh_edit_state.mesh_index, triangle_vertices[i]);
        screen_points[i] = GetWorldToScreen(world_points[i], camera);
    }

    for (int i = 0; i < 3; i++) {
        const int next = (i + 1) % 3;
        draw_list->AddLine(
            ImVec2(screen_points[i].x, screen_points[i].y),
            ImVec2(screen_points[next].x, screen_points[next].y),
            IM_COL32(255, 210, 120, 220),
            2.0f
        );
    }

    for (int i = 0; i < 3; i++) {
        const bool selected = g_mesh_edit_state.vertex_corner == i;
        const ImU32 fill = selected ? IM_COL32(255, 170, 64, 255) : IM_COL32(70, 180, 255, 240);
        const float radius = selected ? 8.0f : 6.0f;
        draw_list->AddCircleFilled(ImVec2(screen_points[i].x, screen_points[i].y), radius, fill);
        draw_list->AddCircle(ImVec2(screen_points[i].x, screen_points[i].y), radius, IM_COL32(20, 20, 20, 255), 0, 2.0f);
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    const Vector2 mouse = GetMousePosition();
    float best_distance = 18.0f;
    int best_corner = -1;

    for (int i = 0; i < 3; i++) {
        const float dx = mouse.x - screen_points[i].x;
        const float dy = mouse.y - screen_points[i].y;
        const float distance = sqrtf(dx * dx + dy * dy);
        if (distance < best_distance) {
            best_distance = distance;
            best_corner = i;
        }
    }

    if (best_corner >= 0) {
        g_mesh_edit_state.vertex_corner = best_corner;
        return;
    }

    int picked_triangle = -1;
    int picked_corner = 0;
    if (pick_mesh_triangle(
            *entity,
            g_mesh_edit_state.mesh_index,
            GetScreenToWorldRay(mouse, camera),
            picked_triangle,
            picked_corner)) {
        g_mesh_edit_state.triangle_index = picked_triangle;
        g_mesh_edit_state.vertex_corner = picked_corner;
    }
}

}

void draw_gizmo(Editor& editor, Camera3D camera) {
    sync_mesh_edit_state(editor);

    Entity* entity = editor.scene.get_selected();
    if (!entity) return;

    ImGuizmo::SetDrawlist();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(g_scene_window_pos.x, g_scene_window_pos.y, g_scene_window_size.x, g_scene_window_size.y);

    Matrix view = MatrixTranspose(GetCameraMatrix(camera));
    float aspect = (g_scene_window_size.y > 0) ? (g_scene_window_size.x / g_scene_window_size.y) : 1.0f;
    Matrix projection = MatrixTranspose(MatrixPerspective(
        camera.fovy * DEG2RAD,
        aspect,
        0.1f,
        1000.0f
    ));

    float view_matrix[16] = {};
    float projection_matrix[16] = {};
    float transform_matrix[16] = {};
    float translation[3] = { entity->position.x, entity->position.y, entity->position.z };
    float rotation[3] = { entity->rotation.x, entity->rotation.y, entity->rotation.z };
    float scale[3] = { entity->scale.x, entity->scale.y, entity->scale.z };

    memcpy(view_matrix, &view, sizeof(view_matrix));
    memcpy(projection_matrix, &projection, sizeof(projection_matrix));

    draw_mesh_vertex_overlay(editor, camera);

    if (g_mesh_edit_state.enabled && has_valid_model_data(entity->model)) {
        if (g_mesh_edit_state.mesh_index >= entity->model.meshCount) g_mesh_edit_state.mesh_index = 0;

        int vertex_index = -1;
        if (get_selected_vertex_index(*entity, g_mesh_edit_state.mesh_index, g_mesh_edit_state.triangle_index, g_mesh_edit_state.vertex_corner, vertex_index)) {
            const Vector3 vertex_world = get_mesh_vertex_world_position(*entity, g_mesh_edit_state.mesh_index, vertex_index);
            float vertex_translation[3] = { vertex_world.x, vertex_world.y, vertex_world.z };
            float vertex_rotation[3] = { 0.0f, 0.0f, 0.0f };
            float vertex_scale[3] = { 1.0f, 1.0f, 1.0f };

            ImGuizmo::RecomposeMatrixFromComponents(vertex_translation, vertex_rotation, vertex_scale, transform_matrix);
            ImGuizmo::Manipulate(
                view_matrix,
                projection_matrix,
                editor_internal::gizmo_mode,
                ImGuizmo::WORLD,
                transform_matrix
            );

            if (ImGuizmo::IsUsing() && !g_mesh_edit_state.was_using_gizmo) {
                editor.save_state();
            }

            if (ImGuizmo::IsUsing()) {
                float next_translation[3] = {};
                float next_rotation[3] = {};
                float next_scale[3] = {};
                ImGuizmo::DecomposeMatrixToComponents(transform_matrix, next_translation, next_rotation, next_scale);
                set_mesh_vertex_world_position(
                    *entity,
                    g_mesh_edit_state.mesh_index,
                    vertex_index,
                    { next_translation[0], next_translation[1], next_translation[2] }
                );
            }

            g_mesh_edit_state.was_using_gizmo = ImGuizmo::IsUsing();
            return;
        }
    }

    ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, transform_matrix);
    ImGuizmo::Manipulate(
        view_matrix,
        projection_matrix,
        editor_internal::gizmo_mode,
        ImGuizmo::WORLD,
        transform_matrix
    );

    static bool was_using = false;
    if (ImGuizmo::IsUsing() && !was_using) {
        editor.save_state();
    }

    if (ImGuizmo::IsUsing()) {
        float next_translation[3] = {};
        float next_rotation[3] = {};
        float next_scale[3] = {};
        ImGuizmo::DecomposeMatrixToComponents(transform_matrix, next_translation, next_rotation, next_scale);

        entity->position = { next_translation[0], next_translation[1], next_translation[2] };
        entity->rotation = { next_rotation[0], next_rotation[1], next_rotation[2] };
        entity->scale = { next_scale[0], next_scale[1], next_scale[2] };
        if (!entity->texture_stretch) mark_entity_uv_dirty(entity);
    }

    was_using = ImGuizmo::IsUsing();
    g_mesh_edit_state.was_using_gizmo = false;
}

void handle_scene_asset_drop(Editor& editor, Camera3D camera, bool is_hovered) {
    if (!editor_internal::scene_asset_dragging) return;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return;

    const std::string asset_name = editor_internal::dragged_scene_asset_name;
    editor_internal::scene_asset_dragging = false;
    editor_internal::dragged_scene_asset_name.clear();

    if (ImGuizmo::IsUsing()) return;
    if (!is_hovered) return;

    ModelAsset* asset = find_asset_by_name(asset_name);
    if (!asset) return;

    Entity entity = make_entity_from_asset(editor.scene, *asset);
    if (!has_valid_model_data(entity.model)) return;

    editor.save_state();
    entity.position = get_scene_drop_position(camera);

    editor.scene.entities.push_back(entity);
    editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
}

void draw_ui(Editor& editor, Shader shader, Camera3D camera) {
    using namespace editor_internal;

    ImGuizmo::BeginFrame();

    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), dockspace_flags);

    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        reset_editor_layout(dockspace_id);
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) project_save(editor.project_path, editor.scene);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) CloseWindow();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) editor.undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) editor.redo();

            ImGui::Separator();

            Entity* entity = editor.scene.get_selected();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, entity != nullptr)) {
                clipboard_data = *entity;
                has_clipboard = true;
            }

            if (ImGui::MenuItem("Paste", "Ctrl+V", false, has_clipboard)) {
                ModelAsset* asset = find_asset_by_name(clipboard_data.asset_name);
                if (asset) {
                    editor.save_state();
                    Entity pasted = make_entity_from_asset(editor.scene, *asset);
                    pasted.position = clipboard_data.position;
                    pasted.rotation = clipboard_data.rotation;
                    pasted.scale = clipboard_data.scale;
                    pasted.color = clipboard_data.color;
                    pasted.outline_color = clipboard_data.outline_color;
                    pasted.has_light = clipboard_data.has_light;
                    pasted.light = clipboard_data.light;
                    pasted.light_created = false;
                    editor.scene.entities.push_back(pasted);
                    editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
                }
            }

            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, entity != nullptr)) {
                editor.save_state();
                Entity copy = *entity;
                copy.id = static_cast<int>(editor.scene.entities.size());
                copy.name = editor.scene.make_default_name_for(copy);
                if (copy.has_light) {
                    copy.light_created = false;
                    copy.light.id = -1;
                    copy.light.light = {0};
                }
                editor.scene.entities.push_back(copy);
                editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", "Del", false, entity != nullptr)) {
                editor.save_state();
                const int index = editor.scene.selected;
                if (entity->has_light && entity->light_created) {
                    entity->light.enabled = false;
                    if (entity->light.id != -1) update_lighting(shader, entity->light);
                    free_light_id(entity->light.id);
                }
                editor.scene.entities.erase(editor.scene.entities.begin() + index);
                editor.scene.selected = -1;
            }

            ImGui::Separator();
            ImGui::MenuItem("Preferences");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Reset Layout")) {
                reset_editor_layout(dockspace_id);
            }
            ImGui::Separator();
            ImGui::MenuItem("Hierarchy", nullptr, &show_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &show_inspector);
            ImGui::MenuItem("Assets", nullptr, &show_assets);
            ImGui::MenuItem("Scene", nullptr, &show_scene);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create")) {
            for (auto& asset : assets) {
                if (!asset.is_procedural) continue;
                if (ImGui::MenuItem(asset.name.c_str())) {
                    editor.save_state();
                    Entity created = make_entity_from_asset(editor.scene, asset);
                    if (has_valid_model_data(created.model)) {
                        editor.scene.entities.push_back(created);
                        editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) show_about_window = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ImGuiIO& io = ImGui::GetIO();

    if (show_hierarchy) {
        ImGui::Begin("Hierarchy", &show_hierarchy);

    for (int i = 0; i < static_cast<int>(editor.scene.entities.size()); i++) {
        Entity& entity = editor.scene.entities[i];

        ImGui::PushID(i);
        const bool selected = editor.scene.selected == i;
        if (ImGui::Selectable(entity.name.c_str(), selected)) editor.scene.selected = i;

        if (ImGui::BeginPopupContextItem(TextFormat("context_%d", entity.id))) {
            if (ImGui::MenuItem("Delete")) {
                editor.save_state();
                if (entity.has_light && entity.light_created) {
                    entity.light.enabled = false;
                    if (entity.light.id != -1) update_lighting(shader, entity.light);
                    free_light_id(entity.light.id);
                }

                editor.scene.entities.erase(editor.scene.entities.begin() + i);
                if (editor.scene.selected == i) editor.scene.selected = -1;
                else if (editor.scene.selected > i) editor.scene.selected--;

                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }

            if (ImGui::MenuItem("Rename")) {
                editor.save_state();
                renaming_index = i;
                const size_t copied = entity.name.copy(rename_buf, sizeof(rename_buf) - 1);
                rename_buf[copied] = '\0';
            }

            if (ImGui::MenuItem("Duplicate")) {
                editor.save_state();
                Entity copy = entity;
                copy.id = static_cast<int>(editor.scene.entities.size());
                copy.name = editor.scene.make_default_name_for(copy);
                if (copy.has_light) {
                    copy.light_created = false;
                    copy.light.id = -1;
                    copy.light.light = {0};
                    copy.light.intensity = entity.light.intensity;
                    copy.light.range = entity.light.range;
                }
                editor.scene.entities.push_back(copy);
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("Create")) {
            editor.save_state();
            for (int asset_index = 0; asset_index < static_cast<int>(assets.size()); asset_index++) {
                auto& asset = assets[asset_index];
                const std::string label = asset.name + "##create_" + std::to_string(asset_index);
                if (ImGui::MenuItem(label.c_str())) {
                    Entity entity = make_entity_from_asset(editor.scene, asset);
                    if (!has_valid_model_data(entity.model)) continue;
                    editor.scene.entities.push_back(entity);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::End();
    }

    if (show_inspector) {
        ImGui::Begin("Inspector", &show_inspector);
    ImGui::Text("Mode");
    ImGui::SameLine();
    if (ImGui::Button("P")) gizmo_mode = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::Button("R")) gizmo_mode = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::Button("S")) gizmo_mode = ImGuizmo::SCALE;

    Entity* entity = editor.scene.get_selected();
    if (entity) {
        ImGui::Separator();
        char inspector_name[128] = {};
        const size_t copied = entity->name.copy(inspector_name, sizeof(inspector_name) - 1);
        inspector_name[copied] = '\0';

        static std::string last_name;
        if (ImGui::InputText("Name", inspector_name, IM_ARRAYSIZE(inspector_name))) {
            if (last_name != inspector_name) {
                editor.save_state();
                assign_entity_name(*entity, inspector_name);
                last_name = inspector_name;
            }
        } else {
            last_name = inspector_name;
        }

        ImGui::Separator();
        ImGui::Text("Transform");
        float position[3] = { entity->position.x, entity->position.y, entity->position.z };
        float rotation[3] = { entity->rotation.x, entity->rotation.y, entity->rotation.z };
        float scale[3] = { entity->scale.x, entity->scale.y, entity->scale.z };

        static Vector3 last_position = {};
        static Vector3 last_rotation = {};
        static Vector3 last_scale = {};

        if (ImGui::DragFloat3("Position", position, 0.1f)) {
            if (last_position.x != position[0] || last_position.y != position[1] || last_position.z != position[2]) {
                editor.save_state();
                last_position = { position[0], position[1], position[2] };
            }
            entity->position = { position[0], position[1], position[2] };
        }

        if (ImGui::DragFloat3("Rotation", rotation, 1.0f)) {
            if (last_rotation.x != rotation[0] || last_rotation.y != rotation[1] || last_rotation.z != rotation[2]) {
                editor.save_state();
                last_rotation = { rotation[0], rotation[1], rotation[2] };
            }
            entity->rotation = { rotation[0], rotation[1], rotation[2] };
        }

        if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
            if (last_scale.x != scale[0] || last_scale.y != scale[1] || last_scale.z != scale[2]) {
                editor.save_state();
                last_scale = { scale[0], scale[1], scale[2] };
            }
            entity->scale = { scale[0], scale[1], scale[2] };
            if (!entity->texture_stretch) mark_entity_uv_dirty(entity);
        }

        ImGui::Separator();
        ImGui::Text("Mesh");

        if (entity->asset && entity->asset->is_procedural) {
            int max_segments = 125;
            if (entity->type == SPHERE || entity->type == HEMISPHERE) max_segments = 100;

            static int last_segments = 0;
            if (ImGui::DragInt("Segments", &entity->segments, 1, 3, max_segments)) {
                if (last_segments != entity->segments) {
                    editor.save_state();
                    last_segments = entity->segments;
                    clear_mesh_overrides(*entity);
                    update_model(entity);
                    store_uv(entity);
                    mark_entity_uv_dirty(entity);
                    mark_entity_bounds_dirty(entity);
                    entity->shader_assigned = false;
                }
            }
        }

        static int current_model_index = 0;
        std::vector<const char*> model_names;
        model_names.reserve(assets.size());
        for (int i = 0; i < static_cast<int>(assets.size()); i++) {
            model_names.push_back(assets[i].name.c_str());
            if (entity->asset_name == assets[i].name) current_model_index = i;
        }

        static int last_model_index = -1;
        if (!model_names.empty() &&
            ImGui::Combo("Model", &current_model_index, model_names.data(), static_cast<int>(model_names.size()))) {
            if (last_model_index != current_model_index) {
                editor.save_state();
                last_model_index = current_model_index;

                if (entity_owns_model(*entity) && entity->model.meshCount > 0) {
                    UnloadModel(entity->model);
                }
                entity->model = {0};
                entity->original_texcoords.clear();
                entity->original_material_textures.clear();
                clear_mesh_overrides(*entity);

                entity->asset = &assets[current_model_index];
                entity->asset_name = entity->asset->name;
                entity->type = entity->asset->type;
                entity->shader_assigned = false;

                if (entity->asset->is_procedural) {
                    entity->segments = 16;
                    update_model(entity);
                    entity->owns_model_instance = true;
                    store_uv(entity);
                    store_material_textures(entity);
                    mark_entity_uv_dirty(entity);
                    mark_entity_bounds_dirty(entity);
                    entity->texture_source = TEXTURE_NONE;
                    entity->texture_name.clear();
                } else {
                    if (!load_model_instance(*entity->asset, entity->model)) {
                        entity->asset = nullptr;
                        entity->asset_name.clear();
                        entity->model = {0};
                        entity->owns_model_instance = false;
                        entity->texture_source = TEXTURE_NONE;
                        entity->texture_name.clear();
                        ImGui::End();
                        return;
                    }

                    entity->owns_model_instance = true;
                    store_uv(entity);
                    store_material_textures(entity);
                    mark_entity_uv_dirty(entity);
                    mark_entity_bounds_dirty(entity);

                    bool has_embedded = false;
                    for (int i = 0; i < entity->model.materialCount; i++) {
                        if (entity->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                            has_embedded = true;
                            break;
                        }
                    }

                    entity->texture_source = has_embedded ? TEXTURE_MODEL : TEXTURE_NONE;
                    entity->texture_name.clear();
                }
            }
        }

        if (has_valid_model_data(entity->model)) {
            sync_mesh_edit_state(editor);
            if (g_mesh_edit_state.mesh_index >= entity->model.meshCount) g_mesh_edit_state.mesh_index = 0;
            if (entity->model.meshCount > 1) {
                ImGui::SliderInt("Editable Mesh", &g_mesh_edit_state.mesh_index, 0, entity->model.meshCount - 1);
            } else {
                ImGui::Text("Editable Mesh: 0");
            }

            Mesh& editable_mesh = entity->model.meshes[g_mesh_edit_state.mesh_index];
            if (editable_mesh.triangleCount > 0) {
                if (g_mesh_edit_state.triangle_index < 0) g_mesh_edit_state.triangle_index = 0;
                if (g_mesh_edit_state.triangle_index >= editable_mesh.triangleCount) {
                    g_mesh_edit_state.triangle_index = editable_mesh.triangleCount - 1;
                }
                ImGui::SliderInt("Triangle", &g_mesh_edit_state.triangle_index, 0, editable_mesh.triangleCount - 1);
                ImGui::Text("Vertices: %d  Triangles: %d", editable_mesh.vertexCount, editable_mesh.triangleCount);
                ImGui::Checkbox("Vertex Gizmo", &g_mesh_edit_state.enabled);
                ImGui::SameLine();
                ImGui::TextUnformatted(g_mesh_edit_state.enabled ? "Scene gizmo edits selected vertex" : "Inspector fields only");

                int triangle_vertices[3] = {};
                if (get_mesh_triangle_vertex_indices(editable_mesh, g_mesh_edit_state.triangle_index, triangle_vertices)) {
                    ImGui::SliderInt("Vertex", &g_mesh_edit_state.vertex_corner, 0, 2, "Vertex %d");

                    auto edit_triangle_vertex = [&](const char* label, int vertex_index) {
                        float vertex[3] = {
                            editable_mesh.vertices[vertex_index * 3 + 0],
                            editable_mesh.vertices[vertex_index * 3 + 1],
                            editable_mesh.vertices[vertex_index * 3 + 2]
                        };

                        if (ImGui::DragFloat3(label, vertex, 0.01f)) {
                            editor.save_state();
                            set_mesh_vertex_local_position(*entity, g_mesh_edit_state.mesh_index, vertex_index, {
                                vertex[0], vertex[1], vertex[2]
                            });
                        }
                    };

                    edit_triangle_vertex("Vertex A", triangle_vertices[0]);
                    edit_triangle_vertex("Vertex B", triangle_vertices[1]);
                    edit_triangle_vertex("Vertex C", triangle_vertices[2]);

                    ImGui::TextUnformatted(entity->mesh_triangles_detached
                        ? "Triangle sculpt mode is active."
                        : "Triangles are still shared until first edit.");

                    if (entity_has_mesh_overrides(*entity) && ImGui::Button("Reset Mesh Edits")) {
                        editor.save_state();
                        reset_mesh_edit_model(*entity);
                    }
                }
            }
        }

        int current_texture_index = 0;
        std::vector<const char*> texture_names = { "None" };
        std::vector<TextureSource> texture_types = { TEXTURE_NONE };
        std::vector<std::string> texture_sources = { "" };

        for (int i = 1; i < static_cast<int>(texture_options.size()); i++) {
            texture_names.push_back(texture_options[i].name.c_str());
            texture_types.push_back(TEXTURE_EXTERNAL);
            texture_sources.push_back(texture_options[i].name);
        }

        bool has_model_texture = false;
        static std::string model_texture_label;
        if (entity->asset && !entity->asset->is_procedural) {
            for (const auto& texture : entity->original_material_textures) {
                if (texture.id != 0 && texture.id != 1) {
                    has_model_texture = true;
                    break;
                }
            }
        }

        if (has_model_texture) {
            model_texture_label = TextFormat("%s [texture]", entity->asset_name.c_str());
            texture_names.push_back(model_texture_label.c_str());
            texture_types.push_back(TEXTURE_MODEL);
            texture_sources.push_back("__model__");
        }

        for (int i = 0; i < static_cast<int>(texture_types.size()); i++) {
            if (texture_types[i] != entity->texture_source) continue;
            if (entity->texture_source == TEXTURE_EXTERNAL && entity->texture_name == texture_sources[i]) {
                current_texture_index = i;
                break;
            }
            if (entity->texture_source != TEXTURE_EXTERNAL) {
                current_texture_index = i;
                break;
            }
        }

        ImGui::Separator();
        ImGui::Text("Material");

        if (ImGui::Combo("Texture", &current_texture_index, texture_names.data(), static_cast<int>(texture_names.size()))) {
            editor.save_state();
            TextureSource selected_type = texture_types[current_texture_index];

            if (selected_type == TEXTURE_NONE) {
                entity->texture_source = TEXTURE_NONE;
                entity->texture_name.clear();
                entity->texture = {0};
                clear_material_textures(entity);
                mark_entity_uv_dirty(entity);
            } else if (selected_type == TEXTURE_EXTERNAL) {
                entity->texture_source = TEXTURE_EXTERNAL;
                entity->texture_name = texture_sources[current_texture_index];
                entity->texture = {0};

                for (int i = 1; i < static_cast<int>(texture_options.size()); i++) {
                    if (texture_options[i].name == entity->texture_name) {
                        entity->texture = texture_options[i].texture;
                        break;
                    }
                }

                for (int i = 0; i < entity->model.materialCount; i++) {
                    entity->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = entity->texture;
                }
                mark_entity_uv_dirty(entity);
            } else {
                entity->texture_source = TEXTURE_MODEL;
                entity->texture_name.clear();
                entity->texture = {0};
                restore_model_textures(entity);
                mark_entity_uv_dirty(entity);
            }
        }

        static bool last_stretch_texture = false;
        if (ImGui::Checkbox("Stretch Texture", &entity->texture_stretch)) {
            if (last_stretch_texture != entity->texture_stretch) {
                editor.save_state();
                last_stretch_texture = entity->texture_stretch;
                mark_entity_uv_dirty(entity);
            }
        }

        if (!entity->texture_stretch) {
            static bool last_auto_uv = false;
            if (ImGui::Checkbox("Auto UV", &entity->auto_uv)) {
                if (last_auto_uv != entity->auto_uv) {
                    editor.save_state();
                    last_auto_uv = entity->auto_uv;
                    mark_entity_uv_dirty(entity);
                }
            }

            if (entity->auto_uv) {
                static Vector2 last_uv_scale = {};
                if (ImGui::InputFloat("Scale X", &entity->uv_scale_vec.x, 0.1f, 1.0f, "%.2f")) {
                    if (last_uv_scale.x != entity->uv_scale_vec.x) {
                        editor.save_state();
                        last_uv_scale.x = entity->uv_scale_vec.x;
                        mark_entity_uv_dirty(entity);
                    }
                    if (entity->uv_scale_vec.x < 0.01f) entity->uv_scale_vec.x = 0.01f;
                }
                if (ImGui::InputFloat("Scale Y", &entity->uv_scale_vec.y, 0.1f, 1.0f, "%.2f")) {
                    if (last_uv_scale.y != entity->uv_scale_vec.y) {
                        editor.save_state();
                        last_uv_scale.y = entity->uv_scale_vec.y;
                        mark_entity_uv_dirty(entity);
                    }
                    if (entity->uv_scale_vec.y < 0.01f) entity->uv_scale_vec.y = 0.01f;
                }
            } else {
                static float last_repeat_u = 0.0f;
                static float last_repeat_v = 0.0f;
                if (ImGui::InputFloat("Repeat U", &entity->texture_repeat_u, 0.1f, 1.0f, "%.2f")) {
                    if (last_repeat_u != entity->texture_repeat_u) {
                        editor.save_state();
                        last_repeat_u = entity->texture_repeat_u;
                        mark_entity_uv_dirty(entity);
                    }
                    if (entity->texture_repeat_u < 0.01f) entity->texture_repeat_u = 0.01f;
                }
                if (ImGui::InputFloat("Repeat V", &entity->texture_repeat_v, 0.1f, 1.0f, "%.2f")) {
                    if (last_repeat_v != entity->texture_repeat_v) {
                        editor.save_state();
                        last_repeat_v = entity->texture_repeat_v;
                        mark_entity_uv_dirty(entity);
                    }
                    if (entity->texture_repeat_v < 0.01f) entity->texture_repeat_v = 0.01f;
                }
            }
        }

        float color[4] = {
            entity->color.r / 255.0f,
            entity->color.g / 255.0f,
            entity->color.b / 255.0f,
            entity->color.a / 255.0f
        };
        static Color last_color = {};
        if (ImGui::ColorEdit4("Color", color)) {
            Color next_color = {
                static_cast<unsigned char>(color[0] * 255),
                static_cast<unsigned char>(color[1] * 255),
                static_cast<unsigned char>(color[2] * 255),
                static_cast<unsigned char>(color[3] * 255)
            };
            if (memcmp(&last_color, &next_color, sizeof(Color)) != 0) {
                editor.save_state();
                last_color = next_color;
                entity->color = next_color;
            }
        }

        float outline[4] = {
            entity->outline_color.r / 255.0f,
            entity->outline_color.g / 255.0f,
            entity->outline_color.b / 255.0f,
            entity->outline_color.a / 255.0f
        };
        static Color last_outline = {};
        if (ImGui::ColorEdit4("Outline", outline)) {
            Color next_outline = {
                static_cast<unsigned char>(outline[0] * 255),
                static_cast<unsigned char>(outline[1] * 255),
                static_cast<unsigned char>(outline[2] * 255),
                static_cast<unsigned char>(outline[3] * 255)
            };
            if (memcmp(&last_outline, &next_outline, sizeof(Color)) != 0) {
                editor.save_state();
                last_outline = next_outline;
                entity->outline_color = next_outline;
            }
        }

        ImGui::Separator();
        ImGui::Text("Lighting");

        static bool last_has_light = false;
        if (ImGui::Checkbox("Has Light", &entity->has_light)) {
            if (last_has_light != entity->has_light) {
                editor.save_state();
                last_has_light = entity->has_light;
            }
        }

        if (entity->has_light) {
            float light_color[4] = {
                entity->light.color.r / 255.0f,
                entity->light.color.g / 255.0f,
                entity->light.color.b / 255.0f,
                entity->light.color.a / 255.0f
            };

            static Color last_light_color = {};
            if (ImGui::ColorEdit4("Light Color", light_color)) {
                Color next_light_color = {
                    static_cast<unsigned char>(light_color[0] * 255),
                    static_cast<unsigned char>(light_color[1] * 255),
                    static_cast<unsigned char>(light_color[2] * 255),
                    static_cast<unsigned char>(light_color[3] * 255)
                };
                if (memcmp(&last_light_color, &next_light_color, sizeof(Color)) != 0) {
                    editor.save_state();
                    last_light_color = next_light_color;
                    entity->light.color = next_light_color;
                }
            }

            static float last_intensity = 0.0f;
            static float last_range = 0.0f;
            if (ImGui::InputFloat("Intensity", &entity->light.intensity, 0.1f, 1.0f, "%.2f")) {
                if (last_intensity != entity->light.intensity) {
                    editor.save_state();
                    last_intensity = entity->light.intensity;
                }
            }

            if (ImGui::InputFloat("Range", &entity->light.range, 0.1f, 1.0f, "%.2f")) {
                if (last_range != entity->light.range) {
                    editor.save_state();
                    last_range = entity->light.range;
                }
            }

            const char* light_types[] = { "Directional", "Point", "Spot", "Area" };
            int light_type = entity->light.light.type;
            if (ImGui::Combo("Light Type", &light_type, light_types, 4)) {
                editor.save_state();
                entity->light.light.type = light_type;
                if (entity->light_created) update_lighting(shader, entity->light);
            }

            if (light_type == LIGHT_SPOT && ImGui::SliderFloat("Spot Angle", &entity->light.spot_angle, 1.0f, 90.0f, "%.1f deg")) {
                if (entity->light_created) update_lighting(shader, entity->light);
            }

            float target[3] = { entity->light.target.x, entity->light.target.y, entity->light.target.z };
            static Vector3 last_target = {};

            if (ImGui::DragFloat3("Target", target, 0.1f)) {
                if (last_target.x != target[0] || last_target.y != target[1] || last_target.z != target[2]) {
                    editor.save_state();
                    last_target = { target[0], target[1], target[2] };
                }
                entity->light.target = { target[0], target[1], target[2] };
            }

            if (!entity->light_created) {
                const int new_id = allocate_light_id();
                if (new_id == -1) {
                    entity->has_light = false;
                } else {
                    entity->light = create_lighting(entity->position, entity->light.color);
                    entity->light.id = new_id;
                    entity->light.light = create_light_at_slot(
                        new_id,
                        LIGHT_POINT,
                        entity->position,
                        Vector3Zero(),
                        entity->light.color,
                        shader
                    );
                    initialize_lighting_uniform_cache(entity->light, shader, new_id);
                    entity->light_created = true;
                }
            }
        } else if (entity->light_created) {
            entity->light.enabled = false;
            if (entity->light.id != -1) update_lighting(shader, entity->light);
        }
    }

    ImGui::End();
    }

    if (show_scene) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin("Scene", &show_scene)) {
            g_scene_window_pos = ImGui::GetCursorScreenPos();
            g_scene_window_size = ImGui::GetContentRegionAvail();
            if (scene_rt.id > 0) {
                if (scene_rt.texture.width != (int)g_scene_window_size.x || scene_rt.texture.height != (int)g_scene_window_size.y) {
                    UnloadRenderTexture(scene_rt);
                    scene_rt = LoadRenderTexture((int)g_scene_window_size.x, (int)g_scene_window_size.y);
                }
                Rectangle src = { 0, 0, (float)scene_rt.texture.width, -(float)scene_rt.texture.height };
                rlImGuiImageRect(&scene_rt.texture, (int)g_scene_window_size.x, (int)g_scene_window_size.y, src);
            }
            ImGui::SetCursorScreenPos(g_scene_window_pos);
            ImGui::InvisibleButton("SceneCanvas", g_scene_window_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
            g_is_scene_hovered = ImGui::IsItemHovered();
            g_is_scene_active = ImGui::IsItemActive();

            draw_gizmo(editor, camera);
            handle_scene_asset_drop(editor, camera, g_is_scene_hovered);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (renaming_index != -1) ImGui::OpenPopup("Rename");

    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename", rename_buf, IM_ARRAYSIZE(rename_buf));
        if (ImGui::Button("OK")) {
            if (renaming_index >= 0 && renaming_index < static_cast<int>(editor.scene.entities.size())) {
                assign_entity_name(editor.scene.entities[renaming_index], rename_buf);
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

    if (show_assets) draw_assets_ui(editor);
    draw_model_viewer_window();
    draw_material_viewer_window();

    if (show_about_window) {
        ImGui::OpenPopup("About Quark Engine");
        show_about_window = false;
    }

    if (ImGui::BeginPopupModal("About Quark Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Quark Engine v1.0");
        ImGui::Separator();
        ImGui::Text("Raylib Version: %s", RAYLIB_VERSION);
        ImGui::Text("ImGui Version: %s", IMGUI_VERSION);
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void Editor::draw_ui(Shader shader, Camera3D camera) {
    ::draw_ui(*this, shader, camera);
}
