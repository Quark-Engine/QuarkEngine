#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include "rlgl.h"
#include "headers/lighting.h"
#include "headers/editor.h"
#include "headers/camera.h"
#include "headers/project.h"
#include "headers/hub.h"
#include <iostream>

namespace fs = std::filesystem;

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
    Shader shader = LoadShader("assets/lighting.vs", "assets/lighting.fs");

    int emission_color_loc = GetShaderLocation(shader, "emissionColor");
    int emission_power_loc = GetShaderLocation(shader, "emissionPower");

    load_models();
    load_textures(project_path);
    refresh_assets(project_path);
    refresh_models(project_path, editor.scene);

    if (std::filesystem::exists(project_path + "/scene.json"))
        project_load(project_path, editor.scene, shader);
    else
        project_new(project_path, editor.scene);

    int ambient_loc = GetShaderLocation(shader, "ambient");
    float ambient[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
    SetShaderValue(shader, ambient_loc, ambient, SHADER_UNIFORM_VEC4);

    while (!WindowShouldClose()) {
        SetWindowTitle(TextFormat("Quark Engine | %s | FPS: %d",
            fs::path(project_path).filename().string().c_str(), GetFPS()));

        BeginDrawing();
        ClearBackground(DARKGRAY);

        rlImGuiBegin();

        editor.draw_gizmo(camera.get_camera());
        camera.update();
        editor.handle_input();

        BeginMode3D(camera.get_camera());
        DrawGrid(20, 1.0f);
        BeginShaderMode(shader);
        SetShaderValue(shader, ambient_loc, ambient, SHADER_UNIFORM_VEC4);

        for (auto& e : editor.scene.entities)
        {
            if (!e.shader_assigned) {
                for (int i = 0; i < e.model.materialCount; i++)
                    e.model.materials[i].shader = shader;
                e.shader_assigned = true;
            }

            if (e.has_light && e.light_created) {
                e.light.position       = e.position;
                e.light.light.position = e.position;
                e.light.light.color    = e.light.color;
                e.light.enabled        = true;
                update_lighting(shader, e.light);
            }

            if (e.has_light) {
                Vector3 emission = { e.color.r / 255.0f, e.color.g / 255.0f, e.color.b / 255.0f };
                float power = 2.0f;
                SetShaderValue(shader, emission_color_loc, &emission, SHADER_UNIFORM_VEC3);
                SetShaderValue(shader, emission_power_loc, &power,    SHADER_UNIFORM_FLOAT);
            } else {
                Vector3 zero = {0, 0, 0};
                float power = 0.0f;
                SetShaderValue(shader, emission_color_loc, &zero,  SHADER_UNIFORM_VEC3);
                SetShaderValue(shader, emission_power_loc, &power, SHADER_UNIFORM_FLOAT);
            }

            int useTexLoc = GetShaderLocation(shader, "useTexture");
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
            SetShaderValue(shader, useTexLoc, &use, SHADER_UNIFORM_INT);

            draw_entity_with_texture(e);
        }

        EndShaderMode();
        EndMode3D();

        editor.draw_ui(shader);
        editor.handle_scene_asset_drop(camera.get_camera());

        rlImGuiEnd();
        EndDrawing();
    }

    editor.scene.release_resources();
    unload_models();
    unload_textures();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
