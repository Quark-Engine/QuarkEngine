#pragma once
#include "raylib.h"
#include <string>
#include <filesystem>

std::string get_asset_name_for_path(const std::filesystem::path& project_path_value, const std::filesystem::path& asset_path);
Vector3 get_scene_drop_position(Camera3D camera);
bool has_valid_model_data(const Model& model);
