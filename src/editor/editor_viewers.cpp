#include "editor_viewers.h"
#include "editor_utils.h"
#include "models.h"
#include "imgui.h"
#include <rlImGui.h>

bool show_model_viewer = false;
Model viewer_model = { 0 };
RenderTexture2D viewer_rt = { 0 };
bool show_material_viewer = false;
int material_preview_primitive = 0;

Color material_albedo = WHITE;
float material_albedo_f[4] = {1,1,1,1};
float material_brightness = 1.0f;

Texture2D material_texture = {0};
Model viewer_mat_sphere = { 0 };
RenderTexture2D viewer_mat_rt = { 0 };

Vector3 viewer_target = { 0, 0, 0 };
Vector3 viewer_model_center = { 0, 0, 0 };
Vector3 viewer_model_rotation = { 0, 0, 0 };

std::unordered_map<std::string, Texture> model_preview_cache;
std::unordered_map<std::string, RenderTexture2D> model_render_cache;

float viewer_phi = 20.0f, viewer_theta = 45.0f, viewer_radius = 5.0f;


void draw_model_viewer_window() {
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

void apply_material_settings() {
    if (viewer_mat_sphere.meshCount == 0) return;

    Material& mat = viewer_mat_sphere.materials[0];

    Color finalColor = {
        (unsigned char)(material_albedo.r * material_brightness),
        (unsigned char)(material_albedo.g * material_brightness),
        (unsigned char)(material_albedo.b * material_brightness),
        material_albedo.a
    };

    mat.maps[MATERIAL_MAP_DIFFUSE].color = finalColor;

    if (material_texture.id != 0) {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = material_texture;
    }
}

void rebuild_material_preview_mesh() {
    if (viewer_mat_sphere.meshCount > 0) {
        UnloadModel(viewer_mat_sphere);
        viewer_mat_sphere = {0};
    }

    Mesh mesh = {0};

    switch (material_preview_primitive) {
        case 0: mesh = GenMeshSphere(1.0f, 64, 64); break;
        case 1: mesh = GenMeshCube(2.0f, 2.0f, 2.0f); break;
        case 2: mesh = GenMeshPlane(3.0f, 3.0f, 1, 1); break;
    }

    viewer_mat_sphere = LoadModelFromMesh(mesh);

    apply_material_settings();
}

void draw_material_viewer_window() {
    if (!show_material_viewer) {
        material_preview_primitive = 0;

        if (viewer_mat_sphere.meshCount > 0) {
            UnloadModel(viewer_mat_sphere);
            viewer_mat_sphere = { 0 };
        }
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Material Editor", &show_material_viewer)) {

        ImGui::Columns(2, nullptr, true);
        ImGui::BeginChild("MaterialSettings");
        ImGui::Text("Material");

        if (ImGui::ColorEdit4("Albedo", material_albedo_f)) {
            material_albedo = {
                (unsigned char)(material_albedo_f[0] * 255),
                (unsigned char)(material_albedo_f[1] * 255),
                (unsigned char)(material_albedo_f[2] * 255),
                (unsigned char)(material_albedo_f[3] * 255)
            };

            apply_material_settings();
        }

        if (ImGui::SliderFloat("Brightness", &material_brightness, 0.1f, 2.0f)) {
            apply_material_settings();
        }

        ImGui::Separator();

        ImGui::Text("Primitive");

        const char* primitives[] = { "Sphere", "Cube", "Plane" };
        if (ImGui::Combo("Mesh", &material_preview_primitive, primitives, 3)) {
            rebuild_material_preview_mesh();
        }

        ImGui::EndChild();
        ImGui::NextColumn();

        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1) size.x = 1;
        if (size.y < 1) size.y = 1;

        if (viewer_mat_rt.id == 0 ||
            viewer_mat_rt.texture.width != (int)size.x ||
            viewer_mat_rt.texture.height != (int)size.y) {

            if (viewer_mat_rt.id != 0) UnloadRenderTexture(viewer_mat_rt);
            viewer_mat_rt = LoadRenderTexture((int)size.x, (int)size.y);
        }

        ImVec2 viewport_pos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("Viewport", size,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        if (ImGui::IsItemHovered()) {
            viewer_radius -= ImGui::GetIO().MouseWheel * 0.5f;
            if (viewer_radius < 0.1f) viewer_radius = 0.1f;
        }

        if (ImGui::IsItemActive()) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                viewer_model_rotation.y += delta.x * 0.5f;
                viewer_model_rotation.x += delta.y * 0.5f;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                viewer_theta -= delta.x * 0.5f; 
                viewer_phi -= delta.y * 0.5f;
            }

            viewer_phi = Clamp(viewer_phi, -89.0f, 89.0f);
        }

        Camera3D cam = { 0 };
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        cam.target = viewer_target;
        cam.up = { 0, 1, 0 };

        cam.position.x = viewer_target.x + viewer_radius * cosf(viewer_phi * DEG2RAD) * sinf(viewer_theta * DEG2RAD);
        cam.position.y = viewer_target.y + viewer_radius * sinf(viewer_phi * DEG2RAD);
        cam.position.z = viewer_target.z + viewer_radius * cosf(viewer_phi * DEG2RAD) * cosf(viewer_theta * DEG2RAD);

        BeginTextureMode(viewer_mat_rt);
        ClearBackground({ 40, 40, 45, 255 });

        BeginMode3D(cam);

        if (viewer_mat_sphere.meshCount > 0) {
            Matrix rot = MatrixRotateXYZ({
                viewer_model_rotation.x * DEG2RAD,
                viewer_model_rotation.y * DEG2RAD,
                0
            });

            viewer_mat_sphere.transform = rot;
            DrawModel(viewer_mat_sphere, {0,0,0}, 1.0f, WHITE);
        }

        EndMode3D();
        EndTextureMode();

        ImGui::SetCursorScreenPos(viewport_pos);

        Rectangle src = {
            0, 0,
            (float)viewer_mat_rt.texture.width,
            -(float)viewer_mat_rt.texture.height
        };

        rlImGuiImageRect(&viewer_mat_rt.texture, (int)size.x, (int)size.y, src);

        ImGui::Columns(1);
    }

    ImGui::End();
}

Texture create_model_preview(const ModelAsset& asset, const std::string& cache_key, int preview_size = 64) {
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