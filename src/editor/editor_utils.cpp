#include "editor_utils.h"
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

std::string get_asset_name_for_path(const fs::path& project_path_value, const fs::path& asset_path) {
    std::error_code ec;
    const fs::path resource_dir = project_path_value / "resources";
    const fs::path relative = fs::relative(asset_path, resource_dir, ec);
    if (!ec) return relative.generic_string();
    return asset_path.filename().generic_string();
}

Vector3 get_scene_drop_position(Camera3D camera) {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    const float epsilon = 0.0001f;

    if (fabsf(ray.direction.y) > epsilon) {
        const float t = -ray.position.y / ray.direction.y;
        if (t >= 0.0f) {
            return {
                ray.position.x + ray.direction.x * t,
                0.0f,
                ray.position.z + ray.direction.z * t
            };
        }
    }

    return {
        camera.position.x + camera.target.x,
        0.0f,
        camera.position.z + camera.target.z
    };
}

bool has_valid_model_data(const Model& model) {
    return model.meshCount > 0 && model.meshes != nullptr;
}