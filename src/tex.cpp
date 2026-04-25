#include "headers/tex.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

std::vector<TextureOption> texture_options;
std::vector<AssetEntry> asset_entries;

static std::vector<fs::directory_entry> collect_resource_entries(const fs::path& resource_dir) {
    std::vector<fs::directory_entry> result;
    std::error_code ec;
    fs::recursive_directory_iterator it(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return result;

    for (const auto& entry : it) {
        result.push_back(entry);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const fs::directory_entry& lhs, const fs::directory_entry& rhs) {
            std::error_code lhs_ec;
            std::error_code rhs_ec;
            const bool lhs_is_dir = lhs.is_directory(lhs_ec) && !lhs_ec;
            const bool rhs_is_dir = rhs.is_directory(rhs_ec) && !rhs_ec;

            if (lhs_is_dir != rhs_is_dir) return lhs_is_dir > rhs_is_dir;
            return lhs.path().generic_string() < rhs.path().generic_string();
        }
    );

    return result;
}

static std::vector<fs::path> collect_resource_files(const fs::path& resource_dir) {
    std::vector<fs::path> result;
    for (const auto& entry : collect_resource_entries(resource_dir)) {
        std::error_code ec;
        if (!entry.is_regular_file(ec) || ec) {
            continue;
        }

        result.push_back(entry.path());
    }

    return result;
}

bool is_image_file(const fs::path& p) {
    std::string ext = p.extension().string();
    for (auto& c : ext) c = (char)tolower(c);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

void load_textures(std::string project_path) {
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    unload_textures();
    texture_options.clear();
    texture_options.push_back({ "None", {0} });

    for (const auto& path : collect_resource_files(resource_dir)) {
        if (!is_image_file(path)) continue;

        Texture2D tex = LoadTexture(path.string().c_str());
        texture_options.push_back({ fs::relative(path, resource_dir).generic_string(), tex });
    }
}

void unload_textures() {
    std::unordered_set<unsigned int> released_ids;
    for (const auto& opt : texture_options) {
        if (opt.texture.id == 0) continue;
        if (released_ids.insert(opt.texture.id).second) {
            UnloadTexture(opt.texture);
        }
    }
    texture_options.clear();
}

void apply_texture_repeat(Entity &e) {
    for (int m = 0; m < e.model.meshCount; m++) {
        Mesh &mesh = e.model.meshes[m];
        if (!mesh.texcoords) continue;

        for (int i = 0; i < mesh.vertexCount; i++) {
            float u, v;

            if (e.auto_uv) {
                Vector3 pos = {
                    mesh.vertices[i*3+0],
                    mesh.vertices[i*3+1],
                    mesh.vertices[i*3+2]
                };

                Vector3 normal = {
                    mesh.normals[i*3+0],
                    mesh.normals[i*3+1],
                    mesh.normals[i*3+2]
                };

                float ax = fabs(normal.x);
                float ay = fabs(normal.y);
                float az = fabs(normal.z);

                float sx = e.scale.x;
                float sy = e.scale.y;
                float sz = e.scale.z;

                if (ay > ax && ay > az) {
                    u = pos.x * sx;
                    v = pos.z * sz;
                } 
                
                else if (ax > az) {
                    u = pos.z * sz;
                    v = pos.y * sy;
                } 
                
                else {
                    u = pos.x * sx;
                    v = pos.y * sy;
                }

                u *= e.uv_scale_vec.x;
                v *= e.uv_scale_vec.y;
            } else {
                if (m >= e.original_texcoords.size()) continue;
                auto& base = e.original_texcoords[m];

                u = base[i*2+0] * e.texture_repeat_u * e.scale.x;
                v = base[i*2+1] * e.texture_repeat_v * e.scale.y;
            }

            mesh.texcoords[i*2+0] = u;
            mesh.texcoords[i*2+1] = v;
        }

        UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    }
}

void store_uv(Entity* e) {
    e->original_texcoords.clear();

    for (int m = 0; m < e->model.meshCount; m++) {
        Mesh& mesh = e->model.meshes[m];

        if (!mesh.texcoords) {
            e->original_texcoords.push_back({});
            continue;
        }

        std::vector<float> uv(mesh.vertexCount * 2);
        memcpy(uv.data(), mesh.texcoords, uv.size() * sizeof(float));

        e->original_texcoords.push_back(uv);
    }
}

void store_material_textures(Entity* e) {
    e->original_material_textures.clear();
    e->original_material_textures.reserve(e->model.materialCount);

    for (int i = 0; i < e->model.materialCount; i++) {
        e->original_material_textures.push_back(
            e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture
        );
    }
}

void restore_model_textures(Entity* e) {
    if (e->original_material_textures.size() != static_cast<size_t>(e->model.materialCount)) return;

    for (int i = 0; i < e->model.materialCount; i++) {
        e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e->original_material_textures[i];
    }
}

void clear_material_textures(Entity* e) {
    for (int i = 0; i < e->model.materialCount; i++) {
        e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
    }
}

void draw_entity_with_texture(Entity& e) {
    if (e.texture.id != 0) {
        for (int i = 0; i < e.model.materialCount; i++)
            e.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e.texture;

        if (e.texture_stretch) {
            for (int m = 0; m < e.model.meshCount; m++) {
                Mesh& mesh = e.model.meshes[m];
                if (!mesh.texcoords || m >= e.original_texcoords.size()) continue;

                memcpy(mesh.texcoords, e.original_texcoords[m].data(), mesh.vertexCount * 2 * sizeof(float));
                UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
            }
        } 
        
        else {
            apply_texture_repeat(e);
        }
    }

    rlPushMatrix();
    rlTranslatef(e.position.x, e.position.y, e.position.z);

    rlRotatef(e.rotation.x, 1, 0, 0);
    rlRotatef(e.rotation.y, 0, 1, 0);
    rlRotatef(e.rotation.z, 0, 0, 1);
    
    rlScalef(e.scale.x, e.scale.y, e.scale.z);

    DrawModel(e.model, {0,0,0}, 1.0f, e.color);
    DrawModelWires(e.model, {0,0,0}, 1.0f, e.outline_color);

    rlPopMatrix();
}

void refresh_textures(Scene* scene, const std::string& project_path) {
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    std::unordered_map<std::string, Texture2D> old_by_name;
    for (const auto& opt : texture_options) {
        if (opt.texture.id != 0) {
            old_by_name[opt.name] = opt.texture;
        }
    }

    std::vector<TextureOption> next_options;
    next_options.push_back({ "None", {0} });

    for (const auto& path : collect_resource_files(resource_dir)) {
        if (!is_image_file(path)) continue;

        const std::string texture_name = fs::relative(path, resource_dir).generic_string();
        auto old_it = old_by_name.find(texture_name);
        if (old_it != old_by_name.end()) {
            next_options.push_back({ texture_name, old_it->second });
            old_by_name.erase(old_it);
            continue;
        }

        Texture2D tex = LoadTexture(path.string().c_str());
        next_options.push_back({ texture_name, tex });
    }

    if (scene) {
        for (const auto& [_, removed_tex] : old_by_name) {
            for (auto& entity : scene->entities) {
                if (entity.texture.id == removed_tex.id) {
                    entity.texture = {0};
                }
            }
        }
    }

    std::unordered_set<unsigned int> released_ids;
    for (const auto& [_, removed_tex] : old_by_name) {
        if (removed_tex.id == 0) continue;
        if (released_ids.insert(removed_tex.id).second) {
            UnloadTexture(removed_tex);
        }
    }

    texture_options = std::move(next_options);
}

void load_assets(std::string project_path) {
    asset_entries.clear();
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& entry : collect_resource_entries(resource_dir)) {
        std::error_code ec;
        AssetEntry asset_entry;
        asset_entry.filename = fs::relative(entry.path(), resource_dir).generic_string();
        asset_entry.is_directory = entry.is_directory(ec) && !ec;
        asset_entry.is_image = !asset_entry.is_directory && is_image_file(entry.path());

        if (asset_entry.is_image) {
            asset_entry.texture = LoadTexture(entry.path().string().c_str());
        }

        asset_entries.push_back(asset_entry);
    }
}

void refresh_assets(std::string project_path) {
    for (auto& asset : asset_entries) {
        if (asset.is_image && asset.texture.id != 0) {
            UnloadTexture(asset.texture);
        }
    }

    asset_entries.clear();
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& entry : collect_resource_entries(resource_dir)) {
        std::error_code ec;
        AssetEntry a;
        a.filename = fs::relative(entry.path(), resource_dir).generic_string();
        a.is_directory = entry.is_directory(ec) && !ec;
        a.is_image = !a.is_directory && is_image_file(entry.path());

        if (a.is_image) a.texture = LoadTexture(entry.path().string().c_str());
        asset_entries.push_back(a);
    }
}
void clone_model_materials(Entity* e) {
    if (e->model.materialCount <= 0) return;

    if (e->owns_materials) {
        if (e->model.materials)    RL_FREE(e->model.materials);
        if (e->model.meshMaterial) RL_FREE(e->model.meshMaterial);
        e->model.materials    = nullptr;
        e->model.meshMaterial = nullptr;
        e->owns_materials     = false;
    }

    Material* cloned = (Material*)RL_MALLOC(e->model.materialCount * sizeof(Material));
    memcpy(cloned, e->asset->loaded_model.materials, e->model.materialCount * sizeof(Material));
    e->model.materials = cloned;

    e->model.meshMaterial = (int*)RL_MALLOC(e->model.meshCount * sizeof(int));
    memcpy(e->model.meshMaterial, e->asset->loaded_model.meshMaterial, e->model.meshCount * sizeof(int));

    e->owns_materials = true;
}
