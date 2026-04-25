#pragma once
#include "raylib.h"
#include "entity.h"
#include "scene.h"
#include <functional>
#include <string>
#include <filesystem>
#include <cmath>
#include <cstring>

struct TextureOption {
    std::string name;
    Texture2D texture;
};

struct AssetEntry {
    std::string filename;
    bool is_directory = false;
    bool is_image;
    Texture2D texture = {0};
};

extern std::vector<TextureOption> texture_options;
extern std::vector<AssetEntry> asset_entries;

void load_textures(std::string project_path);
void unload_textures();
void apply_texture_repeat(Entity& e);
void store_uv(Entity* e);
void store_material_textures(Entity* e);
void restore_model_textures(Entity* e);
void clear_material_textures(Entity* e);
void draw_entity_with_texture(Entity& e);
void refresh_textures(Scene* scene, const std::string& project_path);
void load_assets(std::string project_path);
void refresh_assets(std::string project_path);
void clone_model_materials(Entity* e);
bool is_image_file(const std::filesystem::path& p);
