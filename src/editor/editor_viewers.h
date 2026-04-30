#pragma once
#include "raylib.h"
#include "tex.h"

extern bool show_model_viewer = false;
extern Model viewer_model = { 0 };
extern RenderTexture2D viewer_rt = { 0 };
extern bool show_material_viewer = false;
extern int material_preview_primitive = 0;

extern Color material_albedo = WHITE;
extern float material_albedo_f[4] = {1,1,1,1};
extern float material_brightness = 1.0f;

extern Texture2D material_texture = {0};
extern Model viewer_mat_sphere = { 0 };
extern RenderTexture2D viewer_mat_rt = { 0 };

extern Vector3 viewer_target = { 0, 0, 0 };
extern Vector3 viewer_model_center = { 0, 0, 0 };
extern Vector3 viewer_model_rotation = { 0, 0, 0 };

extern std::unordered_map<std::string, Texture> model_preview_cache;
extern std::unordered_map<std::string, RenderTexture2D> model_render_cache;

extern float viewer_phi = 20.0f, viewer_theta = 45.0f, viewer_radius = 5.0f;

void draw_model_viewer_window();
void draw_material_viewer_window();
void apply_material_settings();
void rebuild_material_preview_mesh();
Texture create_model_preview(const ModelAsset& asset, const std::string& cache_key, int preview_size = 64);