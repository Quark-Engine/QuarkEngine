#include "editor_viewers.h"
#include "editor_utils.h"
#include "../headers/tex.h"
#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include <cmath>
#include <fstream>
#include <sstream>

// Global state for viewers
static bool show_model_viewer = false;
static Model viewer_model = { 0 };
static RenderTexture2D viewer_rt = { 0 };

static bool show_material_viewer = false;
static int material_preview_primitive = 0;
static Color material_albedo = WHITE;
static float material_albedo_f[4] = {1,1,1,1};
static float material_brightness = 1.0f;
static Texture2D material_texture = {0};
static Model viewer_mat_sphere = { 0 };
static RenderTexture2D viewer_mat_rt = { 0 };

static Vector3 viewer_target = { 0, 0, 0 };
static Vector3 viewer_model_center = { 0, 0, 0 };
static Vector3 viewer_model_rotation = { 0, 0, 0 };
static float viewer_phi = 20.0f, viewer_theta = 45.0f, viewer_radius = 5.0f;

bool open_model_viewer_for_asset(const ModelAsset& asset) {
    if (viewer_model.meshCount > 0) {
        UnloadModel(viewer_model);
        viewer_model = {0};
    }

    if (!load_model_instance(asset, viewer_model)) return false;

    show_model_viewer = true;
    viewer_radius = 5.0f;
    viewer_phi = 20.0f;
    viewer_theta = 45.0f;
    viewer_target = { 0, 0, 0 };
    viewer_model_rotation = { 0, 0, 0 };

    const BoundingBox box = GetModelBoundingBox(viewer_model);
    viewer_model_center = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    return true;
}

bool open_material_viewer_for_path(const std::filesystem::path& material_path, std::unordered_map<std::string, Texture>& texture_cache) {
    std::ifstream material_file(material_path);
    if (!material_file.is_open()) return false;

    if (viewer_mat_sphere.meshCount > 0) {
        UnloadModel(viewer_mat_sphere);
        viewer_mat_sphere = {0};
    }

    viewer_mat_sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));
    Material& material = viewer_mat_sphere.materials[0];
    material_albedo = WHITE;
    material_albedo_f[0] = 1.0f;
    material_albedo_f[1] = 1.0f;
    material_albedo_f[2] = 1.0f;
    material_albedo_f[3] = 1.0f;
    material_brightness = 1.0f;
    material_texture = {0};

    std::string line;
    while (std::getline(material_file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "Kd") {
            float r = 1.0f;
            float g = 1.0f;
            float b = 1.0f;
            if (stream >> r >> g >> b) {
                material.maps[MATERIAL_MAP_DIFFUSE].color = {
                    static_cast<unsigned char>(r * 255),
                    static_cast<unsigned char>(g * 255),
                    static_cast<unsigned char>(b * 255),
                    255
                };
                material_albedo = material.maps[MATERIAL_MAP_DIFFUSE].color;
                material_albedo_f[0] = r;
                material_albedo_f[1] = g;
                material_albedo_f[2] = b;
                material_albedo_f[3] = 1.0f;
            }
        } else if (type == "map_Kd") {
            std::string texture_name;
            if (!(stream >> texture_name)) continue;

            const std::filesystem::path texture_path = material_path.parent_path() / texture_name;
            const std::string cache_key = texture_path.string();
            if (!texture_cache.count(cache_key) && std::filesystem::exists(texture_path)) {
                texture_cache[cache_key] = LoadTexture(cache_key.c_str());
            }

            if (texture_cache.count(cache_key)) {
                material.maps[MATERIAL_MAP_DIFFUSE].texture = texture_cache[cache_key];
                material_texture = texture_cache[cache_key];
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
    return true;
}

// State accessors
bool is_model_viewer_visible() {
    return show_model_viewer;
}

bool is_material_viewer_visible() {
    return show_material_viewer;
}

void show_model_viewer_window(bool show) {
    show_model_viewer = show;
}

void show_material_viewer_window(bool show) {
    show_material_viewer = show;
}

void set_model_viewer_model(const Model& model) {
    if (viewer_model.meshCount > 0) {
        UnloadModel(viewer_model);
    }
    viewer_model = model;
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

void cleanup_viewers() {
    if (viewer_model.meshCount > 0) {
        UnloadModel(viewer_model);
        viewer_model = { 0 };
    }

    if (viewer_rt.id != 0) {
        UnloadRenderTexture(viewer_rt);
        viewer_rt = { 0 };
    }

    if (viewer_mat_sphere.meshCount > 0) {
        UnloadModel(viewer_mat_sphere);
        viewer_mat_sphere = { 0 };
    }

    if (viewer_mat_rt.id != 0) {
        UnloadRenderTexture(viewer_mat_rt);
        viewer_mat_rt = { 0 };
    }
}
