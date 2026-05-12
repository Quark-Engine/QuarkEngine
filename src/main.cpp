#include "QuarkCore/QuarkCore.hpp"
#include "imgui.h"
#include "quark_imgui.h"
#include "plugins/plugin_manager.h"
#include "headers/lighting.h"
#include "headers/language_manager.h"
#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "headers/camera.h"
#include "headers/project.h"
#include "headers/hub.h"
#include "headers/tex.h"
#include <iostream>
#include <cfloat>

using namespace qc;

#define SHADOWMAP_RESOLUTION 1024

namespace fs = std::filesystem;

Shader shadowmap_shader = {0};
RenderTexture2D shadow_map = {0};
RenderTexture2D scene_rt = {0};

extern bool g_is_scene_hovered;
extern bool g_is_scene_active;
static Shader shadowcaster_shader = {0};

static bool language_uses_ms_pgothic(const std::string& language_code) {
    return language_code == "japanese" ||
        language_code == "korean" ||
        language_code == "simplified_chinese" ||
        language_code == "traditional_chinese";
}

static const ImWchar* get_ms_pgothic_glyph_ranges(ImGuiIO& io, const std::string& language_code) {
    if (language_code == "japanese") return io.Fonts->GetGlyphRangesJapanese();
    if (language_code == "korean") return io.Fonts->GetGlyphRangesKorean();
    if (language_code == "simplified_chinese") return io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    if (language_code == "traditional_chinese") return io.Fonts->GetGlyphRangesChineseFull();
    return nullptr;
}

static void reload_editor_fonts(const std::string& language_code) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const std::string base_font_path = LanguageManager::get().editor_font_path();
    const std::string merge_font_path = LanguageManager::get().editor_font_merge_path();
    ImFont* default_font = io.Fonts->AddFontFromFileTTF(base_font_path.c_str(), 16.0f);

    if (!merge_font_path.empty()) {
        ImFontConfig merge_config = {};
        merge_config.MergeMode = true;
        merge_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            merge_font_path.c_str(),
            16.0f,
            &merge_config,
            nullptr
        );
    } else if (language_uses_ms_pgothic(language_code) && base_font_path != "assets/MS-Pgothic-Regular.ttf") {
        ImFontConfig merge_config = {};
        merge_config.MergeMode = true;
        merge_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            "assets/MS-Pgothic-Regular.ttf",
            16.0f,
            &merge_config,
            get_ms_pgothic_glyph_ranges(io, language_code)
        );
    }

    io.FontDefault = default_font;
    io.Fonts->Build();
}

static void update_plugins(PluginManager& plugin_manager, Editor& editor) {
    static Editor* s_editor = nullptr;
    s_editor = &editor;

    PluginContext ctx;
    ctx.delta_time      = GetDeltaTime();
    ctx.entity_count    = (int)s_editor->scene.entities.size();
    ctx.selected        = &s_editor->scene.selected;

    ctx.ui_begin            = [](const char* t) { return ImGui::Begin(t); };
    ctx.ui_end              = []() { ImGui::End(); };
    ctx.ui_text             = [](const char* t) { ImGui::Text("%s", t); };
    ctx.ui_button           = [](const char* l) { return ImGui::Button(l); };
    ctx.ui_checkbox         = [](const char* l, bool* v) { return ImGui::Checkbox(l, v); };
    ctx.ui_slider_float     = [](const char* l, float* v, float mn, float mx) { return ImGui::SliderFloat(l, v, mn, mx); };
    ctx.ui_input_float      = [](const char* l, float* v) { return ImGui::InputFloat(l, v); };
    ctx.ui_color_edit3      = [](const char* l, float c[3]) { return ImGui::ColorEdit3(l, c); };
    ctx.ui_separator        = []() { ImGui::Separator(); };
    ctx.ui_same_line        = []() { ImGui::SameLine(); };

    ctx.entity_get_name     = [](int i) -> const char* { return s_editor->scene.entities[i].name.c_str(); };
    ctx.entity_get_position = [](int i, float* x, float* y, float* z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) { *x = t->position.x; *y = t->position.y; *z = t->position.z; } };
    ctx.entity_get_rotation = [](int i, float* x, float* y, float* z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) { *x = t->rotation.x; *y = t->rotation.y; *z = t->rotation.z; } };
    ctx.entity_get_scale    = [](int i, float* x, float* y, float* z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) { *x = t->scale.x; *y = t->scale.y; *z = t->scale.z; } };
    ctx.entity_get_color    = [](int i, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a) { if (auto* m = s_editor->scene.entities[i].get_material_component()) { *r = m->color.r; *g = m->color.g; *b = m->color.b; *a = m->color.a; } };

    ctx.entity_set_position = [](int i, float x, float y, float z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) t->position = {x, y, z}; };
    ctx.entity_set_rotation = [](int i, float x, float y, float z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) t->rotation = {x, y, z}; };
    ctx.entity_set_scale    = [](int i, float x, float y, float z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) t->scale = {x, y, z}; };
    ctx.entity_set_color    = [](int i, unsigned char r, unsigned char g, unsigned char b, unsigned char a) { if (auto* m = s_editor->scene.entities[i].get_material_component()) m->color = {r, g, b, a}; };
    ctx.entity_set_name     = [](int i, const char* name) { s_editor->scene.entities[i].name = name; };

    ctx.scene_save = []() { project_save(s_editor->project_path, s_editor->scene); };
    ctx.scene_spawn = [](const char* asset_name) -> int {
        for (auto& a : assets) {
            if (a.name == asset_name) {
                Entity e = make_entity_from_asset(s_editor->scene, a);
                const MeshComponent* mesh = e.get_mesh_component();
                if (!mesh || !mesh->model.meshCount) return -1;
                s_editor->scene.entities.push_back(e);
                return (int)s_editor->scene.entities.size() - 1;
            }
        }
        return -1;
    };
    ctx.scene_delete = [](int i) {
        if (i < 0 || i >= (int)s_editor->scene.entities.size()) return;
        s_editor->scene.entities.erase(s_editor->scene.entities.begin() + i);
        if (s_editor->scene.selected >= (int)s_editor->scene.entities.size())
            s_editor->scene.selected = -1;
    };

    plugin_manager.update_all(ctx);
    plugin_manager.draw_ui_all(ctx);
}

static Matrix compose_entity_transform(const Entity& entity) {
    const TransformComponent* transform = entity.get_transform_component();
    if (!transform) return MatrixIdentity();
    Matrix matScale = MatrixScale(transform->scale.x, transform->scale.y, transform->scale.z);
    Matrix matRotation = MatrixRotateXYZ({transform->rotation.x * DEG2RAD, transform->rotation.y * DEG2RAD, transform->rotation.z * DEG2RAD});
    Matrix matTranslation = MatrixTranslate(transform->position.x, transform->position.y, transform->position.z);
    return MatrixMultiply(MatrixMultiply(matTranslation, matRotation), matScale);
}

static void expand_bounds_with_point(BoundingBox& bounds, const Vec3& point) {
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
        const MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || mesh->model.meshCount <= 0 || !mesh->model.meshes) continue;

        Entity& mutable_entity = const_cast<Entity&>(entity);
        MeshComponent* mutable_mesh = mutable_entity.get_mesh_component();
        if (mutable_mesh->bounds_dirty) {
            mutable_mesh->cached_local_bounds = GetModelBoundingBox(mesh->model);
            mutable_mesh->bounds_dirty = false;
        }
        BoundingBox local_bounds = mutable_mesh->cached_local_bounds;
        Matrix transform = compose_entity_transform(entity);

        const Vec3 corners[8] = {
            { local_bounds.min.x, local_bounds.min.y, local_bounds.min.z },
            { local_bounds.max.x, local_bounds.min.y, local_bounds.min.z },
            { local_bounds.min.x, local_bounds.max.y, local_bounds.min.z },
            { local_bounds.max.x, local_bounds.max.y, local_bounds.min.z },
            { local_bounds.min.x, local_bounds.min.y, local_bounds.max.z },
            { local_bounds.max.x, local_bounds.min.y, local_bounds.max.z },
            { local_bounds.min.x, local_bounds.max.y, local_bounds.max.z },
            { local_bounds.max.x, local_bounds.max.y, local_bounds.max.z }
        };

        for (const Vec3& corner : corners) {
            expand_bounds_with_point(bounds, Vec3Transform(corner, transform));
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

static void set_shader_light_enabled(Shader shader, int slot, bool enabled) {
    int enabled_loc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", slot));
    int enabled_value = enabled ? 1 : 0;
    SetShaderValue(shader, enabled_loc, &enabled_value, SHADER_UNIFORM_INT);
}

static void disable_all_shader_lights(Shader shader) {
    for (int slot = 0; slot < MAX_LIGHTS; slot++) {
        set_shader_light_enabled(shader, slot, false);
    }
}

static bool prepare_scene_light_uniforms(Scene& scene, Shader shader, const Vec3& scene_center) {
    disable_all_shader_lights(shader);

    bool has_active_scene_light = false;
    for (auto& e : scene.entities) {
        LightComponent* light = e.get_light_component();
        TransformComponent* transform = e.get_transform_component();
        if (!light || !transform || !light->enabled) continue;

        light->light.enabled = true;
        if (!light->created) {
            int new_id = allocate_light_id();
            if (new_id != -1) {
                light->light.id = new_id;
                light->light.light = create_light_at_slot(new_id, light->light.light.type,
                    transform->position, light->light.target, light->light.color, shader);
                initialize_lighting_uniform_cache(light->light, shader, new_id);
                light->created = true;
            }
        }

        if (!light->created || light->light.id == -1) continue;

        light->light.position = transform->position;

        if (light->light.light.type == LIGHT_DIRECTIONAL &&
            Vec3LengthSqr(Vec3Subtract(light->light.position, light->light.target)) <= 0.000001f) {
            light->light.target = scene_center;
        }

        light->light.light.position = light->light.position;
        light->light.light.target = light->light.target;
        light->light.light.color = light->light.color;
        update_lighting(shader, light->light);
        has_active_scene_light = true;
    }

    if (!has_active_scene_light) {
        Vec3 fallback_target = scene_center;
        Vec3 fallback_position = Vec3Add(scene_center, { 6.0f, 10.0f, 6.0f });
        Light fallback_light = create_light_at_slot(0, LIGHT_DIRECTIONAL, fallback_position, fallback_target, WHITE, shader);
        Lighting fallback_lighting = {};
        fallback_lighting.id = 0;
        fallback_lighting.enabled = true;
        fallback_lighting.position = fallback_position;
        fallback_lighting.target = fallback_target;
        fallback_lighting.color = WHITE;
        fallback_lighting.intensity = 1.0f;
        fallback_lighting.range = 20.0f;
        fallback_lighting.spot_angle = 30.0f;
        fallback_lighting.light = fallback_light;
        initialize_lighting_uniform_cache(fallback_lighting, shader, 0);
        update_lighting(shader, fallback_lighting);
    }

    return has_active_scene_light;
}

static void draw_entity_shadow_caster(const Entity& entity) {
    const MeshComponent* mesh = entity.get_mesh_component();
    const TransformComponent* transform = entity.get_transform_component();
    if (!mesh || !transform) return;

    PushMatrix();
    Translate(transform->position.x, transform->position.y, transform->position.z);
    Rotate(transform->rotation.x, Vec3(1.0f, 0.0f, 0.0f));
    Rotate(transform->rotation.y, Vec3(0.0f, 1.0f, 0.0f));
    Rotate(transform->rotation.z, Vec3(0.0f, 0.0f, 1.0f));
    Scale(transform->scale.x, transform->scale.y, transform->scale.z);

    const bool edited_mesh_is_double_sided = entity_has_mesh_overrides(entity) || mesh->mesh_triangles_detached;
    if (edited_mesh_is_double_sided) DisableBackfaceCulling();

    DrawModel(mesh->model, {0, 0, 0}, 1.0f, WHITE);

    if (edited_mesh_is_double_sided) EnableBackfaceCulling();
    PopMatrix();
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

    // ====== DOCKING ======
    colors[ImGuiCol_DockingPreview] = ImVec4(0.78f, 0.52f, 0.17f, 0.4f); // #c9802b66
}

int main(int argc, char* argv[]) {
    fs::create_directories("projects");
    fs::create_directories("assets");

    std::string lang_code = load_or_create_config();
    LanguageManager::get().set_lang(lang_code);

    InitWindow(1280, 720, "Quark Engine");
    SetTargetFPS(GetCurrentMonitorRefreshRate());
    InitImGui();
    reload_editor_fonts(LanguageManager::get().current);

    ApplyCustomImGuiTheme();

    std::string project_path = "";
    if (argc > 1)
        project_path = argv[1];
    else {
        project_path = run_hub();
        if (project_path.empty()) {
            ImGuiShutdown();
            CloseWindow();
            return 0;
        }
    }

    project_path = project_resolve_root(project_path);

    fs::create_directories(fs::path(project_path) / "resources");

    Editor editor;
    FlyCamera camera;
    editor.project_path = project_path;

    shadowmap_shader = LoadShader("assets/lighting.vs", "assets/lighting.fs");
    shadowcaster_shader = LoadShader("assets/shadowcaster.vs", "assets/shadowcaster.fs");
    shadowmap_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shadowmap_shader, "viewPos");

    shadow_map = load_shadowmap_render_texture(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION);
    
    scene_rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    Camera3D light_cam;
    light_cam.projection = CAMERA_ORTHOGRAPHIC;
    light_cam.up         = {0.0f, 1.0f, 0.0f};
    light_cam.fovy       = 20.0f;
    light_cam.target     = Vec3Zero();
    light_cam.position   = {0.0f, 15.0f, 0.0f};

    int light_vp_loc      = GetShaderLocation(shadowmap_shader, "lightVP");
    int shadow_map_loc    = GetShaderLocation(shadowmap_shader, "shadowMap");
    int shadow_map_res    = SHADOWMAP_RESOLUTION;
    int emission_color_loc = GetShaderLocation(shadowmap_shader, "emissionColor");
    int emission_power_loc = GetShaderLocation(shadowmap_shader, "emissionPower");
    int use_tex_loc        = GetShaderLocation(shadowmap_shader, "useTexture");
    int ambient_loc        = GetShaderLocation(shadowmap_shader, "ambient");
    int shadow_light_dir_loc = GetShaderLocation(shadowmap_shader, "shadowLightDir");
    int shadows_enabled_loc  = GetShaderLocation(shadowmap_shader, "shadowsEnabled");

    float ambient[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    int texture_active_slot = 10;

    SetShaderValue(shadowmap_shader, GetShaderLocation(shadowmap_shader, "shadowMapResolution"), &shadow_map_res, SHADER_UNIFORM_INT);
    SetShaderValue(shadowmap_shader, ambient_loc, ambient, SHADER_UNIFORM_VEC4);

    Mat4 light_view = {0};
    Mat4 light_proj = {0};

    load_models();
    load_textures(project_path);
    refresh_assets(project_path);
    refresh_models(project_path, editor.scene);

    if (project_is_valid(project_path))
        project_load(project_path, editor.scene, shadowmap_shader);
    else
        project_new(project_path, editor.scene);

    PluginManager plugin_manager;
    plugin_manager.load_all("plugins");
    std::string active_font_language = LanguageManager::get().current;

    while (!WindowShouldClose()) {
        if (active_font_language != LanguageManager::get().current) {
            active_font_language = LanguageManager::get().current;
            reload_editor_fonts(active_font_language);
        }

        SetWindowTitle(TextFormat("Quark Engine | %s | FPS: %d",
            fs::path(project_path).filename().string().c_str(), GetFPS()));

        BoundingBox scene_bounds = compute_scene_bounds(editor.scene);
        Vec3 scene_center = {
            (scene_bounds.min.x + scene_bounds.max.x) * 0.5f,
            (scene_bounds.min.y + scene_bounds.max.y) * 0.5f,
            (scene_bounds.min.z + scene_bounds.max.z) * 0.5f
        };
        Vec3 scene_extents = {
            (scene_bounds.max.x - scene_bounds.min.x) * 0.5f,
            (scene_bounds.max.y - scene_bounds.min.y) * 0.5f,
            (scene_bounds.max.z - scene_bounds.min.z) * 0.5f
        };
        float scene_radius = fmaxf(fmaxf(scene_extents.x, scene_extents.y), scene_extents.z);
        if (scene_radius < 10.0f) scene_radius = 10.0f;

        Vec3 active_shadow_target = scene_center;
        Vec3 active_shadow_pos = scene_center + { 6.0f, 10.0f, 6.0f };
        Vec3 active_shadow_dir = (active_shadow_pos - active_shadow_target).normalized();
        bool has_shadow_light = false;
        int active_shadow_type = LIGHT_DIRECTIONAL;

        for (auto& e : editor.scene.entities) {
            LightComponent* light = e.get_light_component();
            TransformComponent* transform = e.get_transform_component();
            if (!light || !transform || !light->enabled || !light->light.enabled) continue;

            active_shadow_pos = transform->position;

            active_shadow_target = light->light.target;
            if ((active_shadow_pos - active_shadow_target).length() <= 0.000001f) {
                active_shadow_target = scene_center;
            }

            active_shadow_dir = (active_shadow_pos - active_shadow_target).normalized();
            active_shadow_type = light->light.light.type;

            if (active_shadow_type == LIGHT_DIRECTIONAL) {
                light_cam.projection = CAMERA_ORTHOGRAPHIC;
                light_cam.up = {0.0f, 1.0f, 0.0f};
                light_cam.fovy = scene_radius * 2.0f;
                light_cam.target = scene_center;
                light_cam.position = scene_center + (active_shadow_dir * fmaxf(scene_radius * 2.0f, 15.0f));
                active_shadow_dir = (light_cam.position - light_cam.target).normalized();
            } else {
                light_cam.projection = CAMERA_PERSPECTIVE;
                light_cam.up = {0.0f, 0.0f, -1.0f};
                light_cam.fovy = 20.0f;
                light_cam.target = active_shadow_target;
                light_cam.position = active_shadow_pos;
            }

            has_shadow_light = true;
            break;
        }

        if (!has_shadow_light) {
            light_cam.projection = CAMERA_ORTHOGRAPHIC;
            light_cam.up = {0.0f, 1.0f, 0.0f};
            light_cam.fovy = scene_radius * 2.0f;
            light_cam.target = active_shadow_target;
            light_cam.position = scene_center + (active_shadow_dir * fmaxf(scene_radius * 2.0f, 15.0f)));
            active_shadow_dir = (light_cam.position - light_cam.target).normalized();
        }

        Vec3 cam_pos = camera.get_camera().position;
        int shadows_enabled = 1;
        float shadow_light_dir[3] = { active_shadow_dir.x, active_shadow_dir.y, active_shadow_dir.z };
        SetShaderValue(shadowmap_shader, shadowmap_shader.locs[SHADER_LOC_VECTOR_VIEW], &cam_pos, SHADER_UNIFORM_VEC3);
        SetShaderValue(shadowmap_shader, shadow_light_dir_loc, shadow_light_dir, SHADER_UNIFORM_VEC3);
        SetShaderValue(shadowmap_shader, shadows_enabled_loc, &shadows_enabled, SHADER_UNIFORM_INT);
        
        BeginTextureMode(shadow_map);
            ClearBackground(WHITE);
            BeginMode3D(light_cam);
                light_view = GetMatrixModelview();
                light_proj = GetMatrixProjection();
                for (auto& e : editor.scene.entities) {
                    MeshComponent* mesh = e.get_mesh_component();
                    if (!mesh || mesh->model.meshCount <= 0) continue;
                    set_model_shader(mesh->model, shadowcaster_shader);
                    draw_entity_shadow_caster(e);
                }
            EndMode3D();
        EndTextureMode();

        Matrix light_view_proj = MatrixMultiply(light_view, light_proj);

        BeginTextureMode(scene_rt);
        ClearBackground(DARKGRAY);
        SetShaderValueMatrix(shadowmap_shader, light_vp_loc, light_view_proj);

        SetShaderValueTexture(shadowmap_shader, shadow_map_loc, shadow_map.depth);
        prepare_scene_light_uniforms(editor.scene, shadowmap_shader, scene_center);

        BeginMode3D(camera.get_camera());
            DrawGrid(20, 1.0f);
            for (auto& e : editor.scene.entities) {
                MeshComponent* mesh = e.get_mesh_component();
                TransformComponent* transform = e.get_transform_component();
                MaterialComponent* mat = e.get_material_component();
                if (!mesh || !transform) continue;

                if (!mesh->shader_assigned || (mesh->model.materialCount > 0 &&
                    mesh->model.materials[0].shader.id != shadowmap_shader.id)) {
                    set_model_shader(mesh->model, shadowmap_shader);
                }
                mesh->shader_assigned = true;

                int use = (mat && mat->texture.id != 0) ? 1 : 0;
                SetShaderValue(shadowmap_shader, use_tex_loc, &use, SHADER_UNIFORM_INT);
                draw_entity_with_texture(e);
            }
        EndMode3D();
    EndTextureMode();

        BeginDrawing();
            ClearBackground(DARKGRAY);
            BeginImGui();

            const bool gizmo_busy = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
            if (!gizmo_busy && (IsCursorHidden() || g_is_scene_hovered)) {
                camera.update(editor.scene);
            }

            editor.handle_input();

            editor.draw_ui(shadowmap_shader, camera);
            
            update_plugins(plugin_manager, editor);

            EndImGui();
        EndDrawing();
    }

    editor.scene.release_resources();
    unload_models();
    unload_textures();
    UnloadShader(shadowcaster_shader);
    UnloadShader(shadowmap_shader);
    unload_shadowmap_render_texture(shadow_map);
    plugin_manager.unload_all();
    ShutdownImGui();
    CloseWindow();
    return 0;
}
