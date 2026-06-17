#include "editor/editor_utils.h"
#include "QuarkCore/QuarkCore.hpp"
#include "entity.h"
#include "models.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace qc;

namespace fs = std::filesystem;

bool has_valid_model_data(const Model& model) {
    return model.meshCount > 0 && model.meshes != nullptr;
}

std::string get_asset_name_for_path(const fs::path& project_path_value, const fs::path& asset_path) {
    std::error_code ec;
    const fs::path resource_dir = project_path_value / "resources";
    const fs::path relative = fs::relative(asset_path, resource_dir, ec);
    if (!ec) return relative.generic_string();
    return asset_path.filename().generic_string();
}

Vec3 get_scene_drop_position(Camera3D camera) {
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

void apply_negative_scale_winding(Entity* entity) {
    if (!entity) return;

    TransformComponent* transform = entity->get_transform_component();
    MeshComponent* mesh_comp = entity->get_mesh_component();
    if (!transform || !mesh_comp) return;

    float sx = transform->scale.x;
    float sy = transform->scale.y;
    float sz = transform->scale.z;

    int neg_count = (sx < 0.0f ? 1 : 0) + (sy < 0.0f ? 1 : 0) + (sz < 0.0f ? 1 : 0);
    if (neg_count % 2 == 0) return;

    for (int m = 0; m < mesh_comp->model.meshCount; m++) {
        Mesh& mesh = mesh_comp->model.meshes[m];
        if (!mesh.vertices || mesh.triangleCount <= 0) continue;

        if (mesh.indices) {
            for (int t = 0; t < mesh.triangleCount; t++) {
                std::swap(mesh.indices[t * 3 + 1], mesh.indices[t * 3 + 2]);
            }

            UpdateMeshBuffer(mesh, 6, mesh.indices, mesh.triangleCount * 3 * sizeof(unsigned short), 0);
        } 
        
        else {
            for (int t = 0; t < mesh.triangleCount; t++) {
                int b = t * 3;
                for (int c = 0; c < 3; c++) {
                    std::swap(mesh.vertices[(b + 1) * 3 + c], mesh.vertices[(b + 2) * 3 + c]);
                }

                if (mesh.texcoords) {
                    for (int c = 0; c < 2; c++) {
                        std::swap(mesh.texcoords[(b + 1) * 2 + c], mesh.texcoords[(b + 2) * 2 + c]);
                    }
                }
            }
            UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);
            if (mesh.texcoords)
                UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
        }

        rebuild_mesh_normals(mesh);
        UpdateMeshBuffer(mesh, 2, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);
    }
}