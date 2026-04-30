#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include "rlgl.h"
#include "headers/lighting.h"
#include "editor/editor.h"
#include "headers/camera.h"
#include "headers/project.h"
#include "headers/hub.h"
#include "headers/tex.h"
#include <iostream>

#define SHADOWMAP_RESOLUTION 4096

namespace fs = std::filesystem;

Shader shadowmap_shader = {0};
RenderTexture2D shadow_map = {0};
static Shader shadowcaster_shader = {0};

static Entity make_entity_from_asset(Scene& scene, ModelAsset* asset) {
    Entity entity;
    entity.id = static_cast<int>(scene.entities.size());
    entity.type = asset->type;
    entity.asset = asset;
    entity.segments = 16;
    entity.name = scene.make_default_name_for(entity);

    if (asset->is_procedural) {
        entity.model = asset->generator(entity.segments);
        store_uv(&entity);
    } else {
        entity.model = asset->loaded_model;
    }

    entity.texture = {0};
    return entity;
}

static Matrix compose_entity_transform(const Entity& entity) {
    Matrix transform = MatrixIdentity();
    transform = MatrixMultiply(transform, MatrixTranslate(entity.position.x, entity.position.y, entity.position.z));
    transform = MatrixMultiply(transform, MatrixRotateX(entity.rotation.x * DEG2RAD));
    transform = MatrixMultiply(transform, MatrixRotateY(entity.rotation.y * DEG2RAD));
    transform = MatrixMultiply(transform, MatrixRotateZ(entity.rotation.z * DEG2RAD));
    transform = MatrixMultiply(transform, MatrixScale(entity.scale.x, entity.scale.y, entity.scale.z));
    return transform;
}

static void expand_bounds_with_point(BoundingBox& bounds, const Vector3& point) {
    bounds.min.x = fminf(bounds.min.x, point.x);
    bounds.min.y = fminf(bounds.min.y, point.y);
    bounds.min.z = fminf(bounds.min.z, point.z);
    bounds.max.x = fmaxf(bounds.max.x, point.x);
    bounds.max.y = fmaxf(bounds.max.y, point.y);
    bounds.max.z = fmaxf(bounds.max.z, point.z);
}

static BoundingBox compute_scene_bounds(const Scene& scene) {
    BoundingBox bounds = {
        { FLT_MAX, FLT_MAX, FLT_MAX },
        { -FLT_MAX, -FLT_MAX, -FLT_MAX }
    };
    bool has_bounds = false;

    for (const auto& entity : scene.entities) {
        if (entity.model.meshCount <= 0 || !entity.model.meshes) continue;

        Entity& mutable_entity = const_cast<Entity&>(entity);
        if (mutable_entity.bounds_dirty) {
            mutable_entity.cached_local_bounds = GetModelBoundingBox(entity.model);
            mutable_entity.bounds_dirty = false;
        }
        BoundingBox local_bounds = mutable_entity.cached_local_bounds;
        Matrix transform = compose_entity_transform(entity);

        const Vector3 corners[8] = {
            { local_bounds.min.x, local_bounds.min.y, local_bounds.min.z },
            { local_bounds.max.x, local_bounds.min.y, local_bounds.min.z },
            { local_bounds.min.x, local_bounds.max.y, local_bounds.min.z },
            { local_bounds.max.x, local_bounds.max.y, local_bounds.min.z },
            { local_bounds.min.x, local_bounds.min.y, local_bounds.max.z },
            { local_bounds.max.x, local_bounds.min.y, local_bounds.max.z },
            { local_bounds.min.x, local_bounds.max.y, local_bounds.max.z },
            { local_bounds.max.x, local_bounds.max.y, local_bounds.max.z }
        };

        for (const Vector3& corner : corners) {
            expand_bounds_with_point(bounds, Vector3Transform(corner, transform));
        }
        has_bounds = true;
    }

    if (!has_bounds) {
        bounds.min = { -5.0f, -5.0f, -5.0f };
        bounds.max = { 5.0f, 5.0f, 5.0f };
    }

    return bounds;
}

static void set_model_shader(Model& model, Shader shader) {
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].shader = shader;
    }
}

void ApplyCustomImGuiTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // ====== SHAPES ======
    style.WindowRounding = 0.0f;
    style.FrameRounding  = 0.0f;
    style.PopupRounding  = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;

    style.FramePadding = ImVec2(6, 3);
    style.ItemSpacing = ImVec2(6, 4);

    ImVec4* colors = style.Colors;

    // ====== GLOBAL ======
    colors[ImGuiCol_Text]           = ImVec4(0.80f, 0.82f, 0.85f, 1.00f); // #c9cdd1
    colors[ImGuiCol_WindowBg]       = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #2a2c2f
    colors[ImGuiCol_ChildBg]        = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_PopupBg]        = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #32353a

    // ====== BORDERS ======
    colors[ImGuiCol_Border]         = ImVec4(0.27f, 0.28f, 0.30f, 1.00f); // #44484d
    colors[ImGuiCol_Separator]      = ImVec4(0.24f, 0.25f, 0.27f, 1.00f);

    // ====== FRAMES (inputs, edits) ======
    colors[ImGuiCol_FrameBg]        = ImVec4(0.14f, 0.15f, 0.16f, 1.00f); // #24272a
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045 hover
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== TITLE / MENUBAR ======
    colors[ImGuiCol_TitleBg]        = ImVec4(0.19f, 0.20f, 0.22f, 1.00f); // #31343a
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.20f, 0.21f, 0.23f, 1.00f);
    colors[ImGuiCol_MenuBarBg]      = ImVec4(0.19f, 0.20f, 0.22f, 1.00f);

    // ====== BUTTONS ======
    colors[ImGuiCol_Button]         = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #51565c
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.36f, 0.38f, 0.41f, 1.00f); // #5C6169 hover
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // #3D4247 pressed

    // ====== HEADERS (Tree, Selectable) ======
    colors[ImGuiCol_Header]         = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #292B2E
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.18f, 0.53f, 0.78f, 1.00f); // #2e87c7ff selection

    // ====== SELECTION ======
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.47f, 0.78f, 0.35f); // #2e78c759

    // ====== SCROLLBAR ======
    colors[ImGuiCol_ScrollbarBg]    = ImVec4(0.18f, 0.20f, 0.22f, 1.00f); // #2f3337
    colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.33f, 0.36f, 0.38f, 1.00f); // #555b62
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.53f, 0.56f, 1.00f); // #80878FFF

    // ====== TABS ======
    colors[ImGuiCol_Tab]            = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #333538FF
    colors[ImGuiCol_TabHovered]     = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #4D5259FF
    colors[ImGuiCol_TabActive]      = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // #3D4247FF

    // ====== CHECKBOX ======
    colors[ImGuiCol_CheckMark]      = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== RESIZE GRIP ======
    colors[ImGuiCol_ResizeGrip]         = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #4D5259FF
    colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff
}

int main(int argc, char* argv[]) {
    fs::create_directories("projects");
    fs::create_directories("assets");

    InitWindow(1280, 720, "Quark Engine");
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
    rlImGuiSetup(true);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF("assets/Rubik-Regular.ttf", 16.0f);
    io.Fonts->Build();

    ApplyCustomImGuiTheme();
    SetExitKey(0);

    std::string project_path = "";
    if (argc > 1)
        project_path = argv[1];
    else {
        project_path = run_hub();
        if (project_path.empty()) {
            rlImGuiShutdown();
            CloseWindow();
            return 0;
        }
    }

    fs::create_directories(fs::path(project_path) / "resources");

    Editor editor;
    FlyCamera camera;
    editor.project_path = project_path;

    shadowmap_shader = LoadShader("assets/lighting.vs", "assets/lighting.fs");
    shadowcaster_shader = LoadShader("assets/shadowmap.vs", "assets/shadowmap.fs");
    shadowmap_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shadowmap_shader, "viewPos");

    shadow_map = load_shadowmap_render_texture(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION);

    Vector3 light_dir = Vector3Normalize({0.35f, -1.0f, -0.35f});

    Camera3D light_cam = {0};
    light_cam.projection = CAMERA_ORTHOGRAPHIC;
    light_cam.up         = {0.0f, 1.0f, 0.0f};
    light_cam.fovy       = 20.0f;
    light_cam.target     = Vector3Zero();
    light_cam.position   = Vector3Scale(light_dir, -15.0f);

    int light_vp_loc      = GetShaderLocation(shadowmap_shader, "lightVP");
    int shadow_map_loc    = GetShaderLocation(shadowmap_shader, "shadowMap");
    int shadow_map_res    = SHADOWMAP_RESOLUTION;
    int emission_color_loc = GetShaderLocation(shadowmap_shader, "emissionColor");
    int emission_power_loc = GetShaderLocation(shadowmap_shader, "emissionPower");
    int use_tex_loc        = GetShaderLocation(shadowmap_shader, "useTexture");
    int ambient_loc        = GetShaderLocation(shadowmap_shader, "ambient");

    float ambient[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    int texture_active_slot = 10;

    SetShaderValue(shadowmap_shader, GetShaderLocation(shadowmap_shader, "shadowMapResolution"), &shadow_map_res, SHADER_UNIFORM_INT);
    SetShaderValue(shadowmap_shader, ambient_loc, ambient, SHADER_UNIFORM_VEC4);

    Matrix light_view = {0};
    Matrix light_proj = {0};

    load_models();
    load_textures(project_path);
    refresh_assets(project_path);
    refresh_models(project_path, editor.scene);

    if (fs::exists(project_path + "/scene.json"))
        project_load(project_path, editor.scene, shadowmap_shader);
    else
        project_new(project_path, editor.scene);

    while (!WindowShouldClose()) {
        SetWindowTitle(TextFormat("Quark Engine | %s | FPS: %d",
            fs::path(project_path).filename().string().c_str(), GetFPS()));

        BoundingBox scene_bounds = compute_scene_bounds(editor.scene);
        Vector3 scene_center = {
            (scene_bounds.min.x + scene_bounds.max.x) * 0.5f,
            (scene_bounds.min.y + scene_bounds.max.y) * 0.5f,
            (scene_bounds.min.z + scene_bounds.max.z) * 0.5f
        };
        Vector3 scene_extents = {
            (scene_bounds.max.x - scene_bounds.min.x) * 0.5f,
            (scene_bounds.max.y - scene_bounds.min.y) * 0.5f,
            (scene_bounds.max.z - scene_bounds.min.z) * 0.5f
        };
        float scene_radius = fmaxf(fmaxf(scene_extents.x, scene_extents.y), scene_extents.z);
        if (scene_radius < 10.0f) scene_radius = 10.0f;

        Vector3 cam_pos = camera.get_camera().position;
        SetShaderValue(shadowmap_shader, shadowmap_shader.locs[SHADER_LOC_VECTOR_VIEW], &cam_pos, SHADER_UNIFORM_VEC3);
        light_cam.target = scene_center;
        light_cam.position = Vector3Add(scene_center, Vector3Scale(light_dir, -(scene_radius * 2.5f)));
        light_cam.fovy = scene_radius * 2.2f;
        
        BeginTextureMode(shadow_map);
            ClearBackground(WHITE);
            BeginMode3D(light_cam);
                light_view = rlGetMatrixModelview();
                light_proj = rlGetMatrixProjection();
                for (auto& e : editor.scene.entities) {
                    if (!e.shader_assigned || e.model.materialCount > 0 && e.model.materials[0].shader.id != shadowcaster_shader.id) {
                        set_model_shader(e.model, shadowcaster_shader);
                    }
                    draw_entity_with_texture(e);
                }
            EndMode3D();
        EndTextureMode();

        Matrix light_view_proj = MatrixMultiply(light_proj, light_view);

        BeginDrawing();
            ClearBackground(DARKGRAY);
            rlImGuiBegin();

            editor.draw_gizmo(camera.get_camera());
            camera.update();
            editor.handle_input();

            SetShaderValueMatrix(shadowmap_shader, light_vp_loc, light_view_proj);
            rlEnableShader(shadowmap_shader.id);
            rlActiveTextureSlot(texture_active_slot);
            rlEnableTexture(shadow_map.depth.id);
            rlSetUniform(shadow_map_loc, &texture_active_slot, SHADER_UNIFORM_INT, 1);

            BeginMode3D(camera.get_camera());
                DrawGrid(20, 1.0f);

                for (auto& e : editor.scene.entities) {
                    if (!e.shader_assigned || e.model.materialCount > 0 && e.model.materials[0].shader.id != shadowmap_shader.id) {
                        set_model_shader(e.model, shadowmap_shader);
                    }
                    e.shader_assigned = true;

                    if (e.has_light && e.light_created) {
                        e.light.position       = e.position;
                        e.light.light.position = e.position;
                        e.light.light.color    = e.light.color;
                        e.light.light.target   = e.light.target;
                        e.light.enabled        = true;
                        update_lighting(shadowmap_shader, e.light);
                    }

                    if (e.has_light) {
                        Vector3 emission = { e.color.r / 255.0f, e.color.g / 255.0f, e.color.b / 255.0f };
                        float power = 2.0f;
                        SetShaderValue(shadowmap_shader, emission_color_loc, &emission, SHADER_UNIFORM_VEC3);
                        SetShaderValue(shadowmap_shader, emission_power_loc, &power, SHADER_UNIFORM_FLOAT);
                    } else {
                        Vector3 zero = {0.0f, 0.0f, 0.0f};
                        float power = 0.0f;
                        SetShaderValue(shadowmap_shader, emission_color_loc, &zero, SHADER_UNIFORM_VEC3);
                        SetShaderValue(shadowmap_shader, emission_power_loc, &power, SHADER_UNIFORM_FLOAT);
                    }

                    int use = 0;
                    if (e.texture.id != 0) {
                        use = 1;
                    } else {
                        for (int i = 0; i < e.model.materialCount; i++) {
                            if (e.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                                use = 1;
                                break;
                            }
                        }
                    }
                    SetShaderValue(shadowmap_shader, use_tex_loc, &use, SHADER_UNIFORM_INT);

                    draw_entity_with_texture(e);
                }

            EndMode3D();

            editor.draw_ui(shadowmap_shader);
            editor.handle_scene_asset_drop(camera.get_camera());

            rlImGuiEnd();
        EndDrawing();
    }

    editor.scene.release_resources();
    unload_models();
    unload_textures();
    UnloadShader(shadowcaster_shader);
    UnloadShader(shadowmap_shader);
    unload_shadowmap_render_texture(shadow_map);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
