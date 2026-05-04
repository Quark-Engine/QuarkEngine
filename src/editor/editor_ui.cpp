#include "editor_ui.h"

#include "editor.h"
#include "editor_assets.h"
#include "editor_components_ui.h"
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

ImVec2 g_scene_window_pos = { 0, 0 };
ImVec2 g_scene_window_size = { 0, 0 };

MeshEditState g_mesh_edit_state;

Matrix compose_entity_transform_matrix(const Entity& entity) {
    const TransformComponent* transform = entity.get_transform_component();
    if (!transform) return MatrixIdentity();
    Matrix matScale = MatrixScale(transform->scale.x, transform->scale.y, transform->scale.z);
    Matrix matRotation = MatrixRotateXYZ({transform->rotation.x * DEG2RAD, transform->rotation.y * DEG2RAD, transform->rotation.z * DEG2RAD});
    Matrix matTranslation = MatrixTranslate(transform->position.x, transform->position.y, transform->position.z);
    return MatrixMultiply(MatrixMultiply(matTranslation, matRotation), matScale);
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
    const MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || !has_valid_model_data(mesh_component->model)) return false;
    if (mesh_index < 0 || mesh_index >= mesh_component->model.meshCount) return false;

    const Mesh& mesh = mesh_component->model.meshes[mesh_index];
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
    const MeshComponent* mesh_component = entity.get_mesh_component();
    const Mesh& mesh = mesh_component->model.meshes[mesh_index];
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
    MeshComponent* mesh = entity.get_mesh_component();
    if (mesh && !mesh->mesh_triangles_detached) detach_mesh_triangles(entity);
    capture_mesh_overrides_from_model(entity);
    return entity_has_mesh_overrides(entity);
}

bool set_mesh_vertex_local_position(Entity& entity, int mesh_index, int vertex_index, const Vector3& local_position) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || !has_valid_model_data(mesh_component->model)) return false;
    if (mesh_index < 0 || mesh_index >= mesh_component->model.meshCount) return false;

    Mesh& mesh = mesh_component->model.meshes[mesh_index];
    if (!mesh.vertices || vertex_index < 0 || vertex_index >= mesh.vertexCount) return false;
    if (!ensure_mesh_edit_ready(entity)) return false;
    if (mesh_index >= static_cast<int>(mesh_component->mesh_vertex_overrides.size())) return false;

    std::vector<float>& mesh_override = mesh_component->mesh_vertex_overrides[mesh_index];
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
    const MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || !has_valid_model_data(mesh_component->model)) return false;
    if (mesh_index < 0 || mesh_index >= mesh_component->model.meshCount) return false;

    const Mesh& mesh = mesh_component->model.meshes[mesh_index];
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
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return;
    clear_mesh_overrides(entity);

    if (mesh->asset && mesh->asset->is_procedural) {
        update_model(&entity);
    } else if (mesh->asset) {
        if (entity_owns_model(entity) && mesh->model.meshCount > 0) {
            UnloadModel(mesh->model);
        }

        mesh->model = {0};
        if (!load_model_instance(*mesh->asset, mesh->model)) {
            mesh->asset = nullptr;
            mesh->asset_name.clear();
            mesh->owns_model_instance = false;
        } else {
            mesh->owns_model_instance = true;
        }
    }

    if (mesh->asset) {
        store_uv(&entity);
        store_material_textures(&entity);
        mesh->shader_assigned = false;
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

void draw_gizmo(Editor& editor, FlyCamera camera) {
    sync_mesh_edit_state(editor);
    ImGuizmo::Enable(!camera.active);

    Entity* entity = editor.scene.get_selected();
    if (!entity) return;
    TransformComponent* transform = entity->get_transform_component();
    MeshComponent* mesh = entity->get_mesh_component();
    if (!transform || !mesh) return;

    if (g_scene_window_size.x <= 0 || g_scene_window_size.y <= 0) return;

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(g_scene_window_pos.x, g_scene_window_pos.y, g_scene_window_size.x, g_scene_window_size.y);

    Matrix view = MatrixTranspose(GetCameraMatrix(camera.get_camera()));
    Matrix projection = MatrixTranspose(MatrixPerspective(
        camera.get_camera().fovy * DEG2RAD,
        g_scene_window_size.x / g_scene_window_size.y,
        0.1f,
        1000.0f
    ));

    float view_matrix[16] = {};
    float projection_matrix[16] = {};
    float transform_matrix[16] = {};

    memcpy(view_matrix, &view, sizeof(view_matrix));
    memcpy(projection_matrix, &projection, sizeof(projection_matrix));

    draw_mesh_vertex_overlay(editor, camera.get_camera());

    if (g_mesh_edit_state.enabled && has_valid_model_data(mesh->model)) {
        if (g_mesh_edit_state.mesh_index >= mesh->model.meshCount) 
            g_mesh_edit_state.mesh_index = 0;

        int vertex_index = -1;
        if (get_selected_vertex_index(
            *entity, 
            g_mesh_edit_state.mesh_index, 
            g_mesh_edit_state.triangle_index, 
            g_mesh_edit_state.vertex_corner, 
            vertex_index)) {
            
            const Vector3 vertex_world = get_mesh_vertex_world_position(
                *entity, 
                g_mesh_edit_state.mesh_index, 
                vertex_index
            );
            
            float vertex_translation[3] = { vertex_world.x, vertex_world.y, vertex_world.z };
            float vertex_rotation[3] = { 0.0f, 0.0f, 0.0f };
            float vertex_scale[3] = { 1.0f, 1.0f, 1.0f };

            ImGuizmo::RecomposeMatrixFromComponents(
                vertex_translation, 
                vertex_rotation, 
                vertex_scale, 
                transform_matrix
            );
            
            ImGuizmo::Manipulate(
                view_matrix,
                projection_matrix,
                ImGuizmo::TRANSLATE,
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
                ImGuizmo::DecomposeMatrixToComponents(
                    transform_matrix, 
                    next_translation, 
                    next_rotation, 
                    next_scale
                );
                
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

    float translation[3] = { transform->position.x, transform->position.y, transform->position.z };
    float rotation[3] = { transform->rotation.x, transform->rotation.y, transform->rotation.z };
    float scale[3] = { transform->scale.x, transform->scale.y, transform->scale.z };

    ImGuizmo::RecomposeMatrixFromComponents(
        translation, 
        rotation, 
        scale, 
        transform_matrix
    );
    
    static bool was_using = false;
    
    ImGuizmo::Manipulate(
        view_matrix,
        projection_matrix,
        editor_internal::gizmo_mode,
        ImGuizmo::WORLD,
        transform_matrix
    );

    if (ImGuizmo::IsUsing() && !was_using) {
        editor.save_state();
    }

    if (ImGuizmo::IsUsing()) {
        float next_translation[3] = {};
        float next_rotation[3] = {};
        float next_scale[3] = {};
        ImGuizmo::DecomposeMatrixToComponents(
            transform_matrix, 
            next_translation, 
            next_rotation, 
            next_scale
        );

        transform->position = { next_translation[0], next_translation[1], next_translation[2] };
        transform->rotation = { next_rotation[0], next_rotation[1], next_rotation[2] };
        transform->scale = { next_scale[0], next_scale[1], next_scale[2] };

        mark_entity_bounds_dirty(entity);
        if (!mesh->texture_stretch) mark_entity_uv_dirty(entity);
    }

    was_using = ImGuizmo::IsUsing();
    g_mesh_edit_state.was_using_gizmo = false;
}

static Vector2 world_to_scene_screen(const Vector3& world, const Camera3D& camera) {
    Camera3D cam = camera;
    
    float aspect = g_scene_window_size.x / g_scene_window_size.y;
    float fovy_rad = cam.fovy * DEG2RAD;
    
    Matrix view = GetCameraMatrix(cam);
    Vector3 view_pos = Vector3Transform(world, view);
    
    float proj_x = view_pos.x / (-view_pos.z * tanf(fovy_rad * 0.5f) * aspect);
    float proj_y = view_pos.y / (-view_pos.z * tanf(fovy_rad * 0.5f));
    
    return {
        g_scene_window_pos.x + (proj_x * 0.5f + 0.5f) * g_scene_window_size.x,
        g_scene_window_pos.y + (1.0f - (proj_y * 0.5f + 0.5f)) * g_scene_window_size.y
    };
}

static Ray scene_screen_to_world_ray(const Vector2& mouse, const Camera3D& camera) {
    float nx = (mouse.x - g_scene_window_pos.x) / g_scene_window_size.x;
    float ny = (mouse.y - g_scene_window_pos.y) / g_scene_window_size.y;

    Matrix view   = GetCameraMatrix(camera);
    Matrix proj   = MatrixPerspective(
        camera.fovy * DEG2RAD,
        g_scene_window_size.x / g_scene_window_size.y,
        0.1f, 1000.0f
    );
    Matrix inv_vp = MatrixInvert(MatrixMultiply(proj, view));

    float ndcX =  nx * 2.0f - 1.0f;
    float ndcY = -(ny * 2.0f - 1.0f);

    auto mul = [](Matrix m, Vector4 v) -> Vector4 {
        return {
            m.m0*v.x + m.m4*v.y + m.m8*v.z  + m.m12*v.w,
            m.m1*v.x + m.m5*v.y + m.m9*v.z  + m.m13*v.w,
            m.m2*v.x + m.m6*v.y + m.m10*v.z + m.m14*v.w,
            m.m3*v.x + m.m7*v.y + m.m11*v.z + m.m15*v.w
        };
    };

    Vector4 near_w = mul(inv_vp, {ndcX, ndcY, -1.0f, 1.0f});
    Vector4 far_w  = mul(inv_vp, {ndcX, ndcY,  1.0f, 1.0f});

    Vector3 near_pos = { near_w.x/near_w.w, near_w.y/near_w.w, near_w.z/near_w.w };
    Vector3 far_pos  = { far_w.x/far_w.w,   far_w.y/far_w.w,   far_w.z/far_w.w  };

    Ray ray;
    ray.position  = near_pos;
    ray.direction = Vector3Normalize(Vector3Subtract(far_pos, near_pos));
    return ray;
}

void draw_mesh_vertex_overlay(Editor& editor, Camera3D camera) {
    sync_mesh_edit_state(editor);
    if (!g_mesh_edit_state.enabled) return;

    Entity* entity = editor.scene.get_selected();
    MeshComponent* mesh = entity ? entity->get_mesh_component() : nullptr;
    if (!entity || !mesh || !has_valid_model_data(mesh->model)) return;
    if (g_mesh_edit_state.mesh_index >= mesh->model.meshCount) g_mesh_edit_state.mesh_index = 0;

    int triangle_vertices[3] = {};
    if (!get_selected_triangle_vertices(*entity, g_mesh_edit_state.mesh_index, g_mesh_edit_state.triangle_index, triangle_vertices)) return;

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    Vector2 screen_points[3] = {};

    for (int i = 0; i < 3; i++) {
        Vector3 wp = get_mesh_vertex_world_position(*entity, g_mesh_edit_state.mesh_index, triangle_vertices[i]);
        screen_points[i] = world_to_scene_screen(wp, camera);
    }

    // debug
    for (int i = 0; i < 3; i++) {
        Vector3 wp = get_mesh_vertex_world_position(*entity, g_mesh_edit_state.mesh_index, triangle_vertices[i]);
        Vector2 raylib_screen = GetWorldToScreen(wp, camera);
        draw_list->AddLine(
            ImVec2(raylib_screen.x - 10, raylib_screen.y),
            ImVec2(raylib_screen.x + 10, raylib_screen.y),
            IM_COL32(255, 0, 0, 255), 2.0f
        );
        draw_list->AddLine(
            ImVec2(raylib_screen.x, raylib_screen.y - 10),
            ImVec2(raylib_screen.x, raylib_screen.y + 10),
            IM_COL32(255, 0, 0, 255), 2.0f
        );
    }

    for (int i = 0; i < 3; i++) {
        const int next = (i + 1) % 3;
        draw_list->AddLine(
            ImVec2(screen_points[i].x, screen_points[i].y),
            ImVec2(screen_points[next].x, screen_points[next].y),
            IM_COL32(255, 210, 120, 220), 2.0f
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
    if (!g_is_scene_hovered) return;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    const Vector2 mouse = GetMousePosition();
    float best_distance = 18.0f;
    int best_corner = -1;

    for (int i = 0; i < 3; i++) {
        const float dx = mouse.x - screen_points[i].x;
        const float dy = mouse.y - screen_points[i].y;
        if (sqrtf(dx*dx + dy*dy) < best_distance) {
            best_distance = sqrtf(dx*dx + dy*dy);
            best_corner = i;
        }
    }

    if (best_corner >= 0) {
        g_mesh_edit_state.vertex_corner = best_corner;
        return;
    }

    int picked_triangle = -1;
    int picked_corner = 0;
    if (pick_mesh_triangle(*entity, g_mesh_edit_state.mesh_index,
            scene_screen_to_world_ray(mouse, camera),
            picked_triangle, picked_corner)) {
        g_mesh_edit_state.triangle_index = picked_triangle;
        g_mesh_edit_state.vertex_corner = picked_corner;
    }
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
    MeshComponent* mesh = entity.get_mesh_component();
    TransformComponent* transform = entity.get_transform_component();
    if (!mesh || !transform || !has_valid_model_data(mesh->model)) return;

    editor.save_state();
    transform->position = get_scene_drop_position(camera);

    editor.scene.entities.push_back(entity);
    editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
}

void draw_ui(Editor& editor, Shader shader, FlyCamera camera) {
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
                const MeshComponent* clipboard_mesh = clipboard_data.get_mesh_component();
                const TransformComponent* clipboard_transform = clipboard_data.get_transform_component();
                const LightComponent* clipboard_light = clipboard_data.get_light_component();
                ModelAsset* asset = (clipboard_mesh && !clipboard_mesh->asset_name.empty())
                    ? find_asset_by_name(clipboard_mesh->asset_name)
                    : nullptr;
                if (asset) {
                    editor.save_state();
                    Entity pasted = make_entity_from_asset(editor.scene, *asset);
                    if (auto pasted_transform = pasted.get_transform_component(); pasted_transform && clipboard_transform) {
                        pasted_transform->position = clipboard_transform->position;
                        pasted_transform->rotation = clipboard_transform->rotation;
                        pasted_transform->scale = clipboard_transform->scale;
                    }
                    if (auto pasted_mesh = pasted.get_mesh_component(); pasted_mesh && clipboard_mesh) {
                        pasted_mesh->color = clipboard_mesh->color;
                        pasted_mesh->outline_color = clipboard_mesh->outline_color;
                        pasted_mesh->texture_source = clipboard_mesh->texture_source;
                        pasted_mesh->texture_name = clipboard_mesh->texture_name;
                        pasted_mesh->texture = clipboard_mesh->texture;
                        pasted_mesh->texture_stretch = clipboard_mesh->texture_stretch;
                        pasted_mesh->auto_uv = clipboard_mesh->auto_uv;
                        pasted_mesh->texture_repeat_u = clipboard_mesh->texture_repeat_u;
                        pasted_mesh->texture_repeat_v = clipboard_mesh->texture_repeat_v;
                        pasted_mesh->uv_scale_vec = clipboard_mesh->uv_scale_vec;
                        pasted_mesh->uv_scale = clipboard_mesh->uv_scale;
                        pasted_mesh->mesh_triangles_detached = clipboard_mesh->mesh_triangles_detached;
                        pasted_mesh->mesh_vertex_overrides = clipboard_mesh->mesh_vertex_overrides;
                        apply_mesh_overrides(pasted);
                    }
                    if (clipboard_light) {
                        auto light_copy = std::make_shared<LightComponent>(*clipboard_light);
                        const int light_type = light_copy->light.light.type;
                        light_copy->created = false;
                        light_copy->light.id = -1;
                        light_copy->light.light = {0};
                        light_copy->light.light.type = light_type;
                        pasted.get_components()->add_component(light_copy);
                    }
                    editor.scene.entities.push_back(pasted);
                    editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
                }
            }

            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, entity != nullptr)) {
                editor.save_state();
                Entity copy = *entity;
                copy.id = static_cast<int>(editor.scene.entities.size());
                copy.name = editor.scene.make_default_name_for(copy);
                if (auto light = copy.get_light_component()) {
                    const int light_type = light->light.light.type;
                    light->created = false;
                    light->light.id = -1;
                    light->light.light = {0};
                    light->light.light.type = light_type;
                }
                editor.scene.entities.push_back(copy);
                editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", "Del", false, entity != nullptr)) {
                editor.save_state();
                const int index = editor.scene.selected;
                if (auto light = entity->get_light_component(); light && light->created) {
                    light->light.enabled = false;
                    if (light->light.id != -1) update_lighting(shader, light->light);
                    free_light_id(light->light.id);
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
                    const MeshComponent* created_mesh = created.get_mesh_component();
                    if (created_mesh && has_valid_model_data(created_mesh->model)) {
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
                if (auto light = entity.get_light_component(); light && light->created) {
                    light->light.enabled = false;
                    if (light->light.id != -1) update_lighting(shader, light->light);
                    free_light_id(light->light.id);
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
                if (auto light = copy.get_light_component()) {
                    const int light_type = light->light.light.type;
                    light->created = false;
                    light->light.id = -1;
                    light->light.light = {0};
                    light->light.light.type = light_type;
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
                    const MeshComponent* mesh = entity.get_mesh_component();
                    if (!mesh || !has_valid_model_data(mesh->model)) continue;
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
        ImGui::Spacing();

        ComponentUIHelper::draw_entity_inspector(editor, *entity, shader);
    }

    ImGui::End();
    }

    if (show_scene) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin("Scene", &show_scene)) {
            g_scene_window_pos = ImGui::GetCursorScreenPos();
            g_scene_window_size = ImGui::GetContentRegionAvail();

            if (g_scene_window_size.x > 0 && g_scene_window_size.y > 0) {
                if (scene_rt.id > 0) {
                    if (scene_rt.texture.width  != (int)g_scene_window_size.x ||
                        scene_rt.texture.height != (int)g_scene_window_size.y) {
                        UnloadRenderTexture(scene_rt);
                        scene_rt = LoadRenderTexture((int)g_scene_window_size.x, (int)g_scene_window_size.y);
                    }
                    ImGui::GetWindowDrawList()->AddImage(
                        (ImTextureID)(intptr_t)scene_rt.texture.id,
                        g_scene_window_pos,
                        ImVec2(g_scene_window_pos.x + g_scene_window_size.x,
                            g_scene_window_pos.y + g_scene_window_size.y),
                        ImVec2(0, 1), ImVec2(1, 0)
                    );
                }

                g_is_scene_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                g_is_scene_active  = ImGui::IsWindowFocused();

                draw_gizmo(editor, camera);
                handle_scene_asset_drop(editor, camera.get_camera(), g_is_scene_hovered);
            }
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

void Editor::draw_ui(Shader shader, FlyCamera camera) {
    ::draw_ui(*this, shader, camera);
}
