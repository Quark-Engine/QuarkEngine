#pragma once
#include <filesystem>
#include "raylib.h"

struct LocalEntry {
    std::string filename;

    bool is_directory;
    bool is_image;
    bool is_model;
    bool is_material;

    Texture texture;
    std::string extension;
};

extern std::filesystem::path current_asset_path;
extern std::string project_path;
extern int selected_asset_index;

bool import_path_to_resources(const std::filesystem::path& src, const std::filesystem::path& resource_dir);
ModelAsset* find_asset_by_path(const fs::path& full_path, const fs::path& project_path_value);
std::string build_resource_signature(const std::filesystem::path& resource_dir);
void draw_assets_ui();