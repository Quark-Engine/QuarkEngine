#include "headers/tex.h"
#include "headers/models.h"
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
    MeshComponent* mesh_component = e.get_mesh_component();
    MaterialComponent* mat_component = e.get_material_component();
    const TransformComponent* transform = e.get_transform_component();
    if (!mesh_component || !transform || !mat_component) return;

    for (int m = 0; m < mesh_component->model.meshCount; m++) {
        Mesh &mesh = mesh_component->model.meshes[m];
        if (!mesh.texcoords) continue;

        for (int i = 0; i < mesh.vertexCount; i++) {
            float u, v;

            if (mat_component->auto_uv) {
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

                float sx = transform->scale.x;
                float sy = transform->scale.y;
                float sz = transform->scale.z;

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

                u *= mat_component->uv_scale.x;
                v *= mat_component->uv_scale.y;
            } else {
                if (m >= mat_component->original_texcoords.size()) continue;
                auto& base = mat_component->original_texcoords[m];

                u = base[i*2+0] * mat_component->texture_repeat_u * transform->scale.x;
                v = base[i*2+1] * mat_component->texture_repeat_v * transform->scale.y;
            }

            mesh.texcoords[i*2+0] = u;
            mesh.texcoords[i*2+1] = v;
        }

        UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    }
}

void store_uv(Entity* e) {
    if (!e) return;
    MeshComponent* mesh_component = e->get_mesh_component();
    MaterialComponent* mat_component = e->get_material_component();
    if (!mesh_component || !mat_component) return;
    mat_component->original_texcoords.clear();

    for (int m = 0; m < mesh_component->model.meshCount; m++) {
        Mesh& mesh = mesh_component->model.meshes[m];

        if (!mesh.texcoords) {
            mat_component->original_texcoords.push_back({});
            continue;
        }

        std::vector<float> uv(mesh.vertexCount * 2);
        memcpy(uv.data(), mesh.texcoords, uv.size() * sizeof(float));

        mat_component->original_texcoords.push_back(uv);
    }

    mesh_component->uv_dirty = true;
    mesh_component->bounds_dirty = true;
}

void mark_entity_uv_dirty(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (mesh) mesh->uv_dirty = true;
}

void mark_entity_bounds_dirty(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (mesh) mesh->bounds_dirty = true;
}

void store_material_textures(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    MaterialComponent* mat = e->get_material_component();
    if (!mesh || !mat) return;
    mat->original_material_textures.clear();
    mat->original_material_textures.reserve(mesh->model.materialCount);

    for (int i = 0; i < mesh->model.materialCount; i++) {
        mat->original_material_textures.push_back(
            mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture
        );
    }
}

void restore_model_textures(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    MaterialComponent* mat = e->get_material_component();
    if (!mesh) return;
    if (mat->original_material_textures.size() != static_cast<size_t>(mesh->model.materialCount)) return;

    for (int i = 0; i < mesh->model.materialCount; i++) {
        mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = mat->original_material_textures[i];
    }
}

void clear_material_textures(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (!mesh) return;
    for (int i = 0; i < mesh->model.materialCount; i++) {
        mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
    }
}

void refresh_entity_render_state(Entity& e) {
    MeshComponent* mesh = e.get_mesh_component();
    MaterialComponent* mat = e.get_material_component();
    if (!mesh || !mesh->uv_dirty || !mat) return;

    if (mat->texture.id != 0) {
        for (int i = 0; i < mesh->model.materialCount; i++) {
            mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = mat->texture;
        }
    }

    if (mat->texture_stretch) {
        for (int m = 0; m < mesh->model.meshCount; m++) {
            Mesh& model_mesh = mesh->model.meshes[m];
            if (!model_mesh.texcoords || m >= mat->original_texcoords.size()) continue;

            memcpy(model_mesh.texcoords, mat->original_texcoords[m].data(), model_mesh.vertexCount * 2 * sizeof(float));
            UpdateMeshBuffer(model_mesh, 1, model_mesh.texcoords, model_mesh.vertexCount * 2 * sizeof(float), 0);
        }
    } else {
        apply_texture_repeat(e);
    }

    mesh->uv_dirty = false;
}

void draw_collision_debug(Entity& entity) {
    CollisionComponent* collision = entity.get_collision_component();
    MeshComponent* mesh_component = entity.get_mesh_component();
    TransformComponent* transform = entity.get_transform_component();

    if (!collision || !transform || !collision->visualize) return;

    Vector3 pos = Vector3Add(transform->position, collision->center);

    rlPushMatrix();

    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(transform->rotation.x, 1, 0, 0);
    rlRotatef(transform->rotation.y, 0, 1, 0);
    rlRotatef(transform->rotation.z, 0, 0, 1);
    rlScalef(transform->scale.x, transform->scale.y, transform->scale.z);

    Color c = GREEN;

    switch (collision->collider_type) {

        case COLLIDER_BOX: {
            DrawCubeWires({0,0,0},
                collision->size.x,
                collision->size.y,
                collision->size.z,
                c);
            break;
        }

        case COLLIDER_SPHERE: {
            DrawSphereWires({0,0,0},
                collision->radius,
                12, 12, c);
            break;
        }

        case COLLIDER_CAPSULE: {
            float r = collision->radius;
            float h = collision->height;

            DrawCylinderWires(
                {0,0,0},
                r, r,
                h - r * 2.0f,
                12, c
            );

            break;
        }

        case COLLIDER_MESH: {
            if (!mesh_component) break;

            Model& model = mesh_component->model;

            rlDisableBackfaceCulling();

            rlBegin(RL_LINES);
            rlColor3f(0, 1, 0);

            for (int m = 0; m < model.meshCount; m++) {
                Mesh& mesh = model.meshes[m];
                if (!mesh.vertices || !mesh.indices) continue;

                for (int i = 0; i < mesh.triangleCount; i++) {

                    unsigned short i0 = mesh.indices[i * 3];
                    unsigned short i1 = mesh.indices[i * 3 + 1];
                    unsigned short i2 = mesh.indices[i * 3 + 2];

                    Vector3 v0 = {
                        mesh.vertices[i0 * 3 + 0],
                        mesh.vertices[i0 * 3 + 1],
                        mesh.vertices[i0 * 3 + 2]
                    };

                    Vector3 v1 = {
                        mesh.vertices[i1 * 3 + 0],
                        mesh.vertices[i1 * 3 + 1],
                        mesh.vertices[i1 * 3 + 2]
                    };

                    Vector3 v2 = {
                        mesh.vertices[i2 * 3 + 0],
                        mesh.vertices[i2 * 3 + 1],
                        mesh.vertices[i2 * 3 + 2]
                    };

                    rlVertex3f(v0.x, v0.y, v0.z);
                    rlVertex3f(v1.x, v1.y, v1.z);

                    rlVertex3f(v1.x, v1.y, v1.z);
                    rlVertex3f(v2.x, v2.y, v2.z);

                    rlVertex3f(v2.x, v2.y, v2.z);
                    rlVertex3f(v0.x, v0.y, v0.z);
                }
            }

            rlEnd();

            rlEnableBackfaceCulling();
            break;
        }
    }

    rlPopMatrix();
}

void draw_entity_with_texture(Entity& e) {
    refresh_entity_render_state(e);
    const MeshComponent* mesh = e.get_mesh_component();
    const MaterialComponent* mat = e.get_material_component();
    const TransformComponent* transform = e.get_transform_component();
    if (!mesh || !transform || !mat) return;

    rlPushMatrix();
    rlTranslatef(transform->position.x, transform->position.y, transform->position.z);

    rlRotatef(transform->rotation.x, 1, 0, 0);
    rlRotatef(transform->rotation.y, 0, 1, 0);
    rlRotatef(transform->rotation.z, 0, 0, 1);
    
    rlScalef(transform->scale.x, transform->scale.y, transform->scale.z);

    const bool edited_mesh_is_double_sided = entity_has_mesh_overrides(e) || mesh->mesh_triangles_detached;
    if (edited_mesh_is_double_sided) rlDisableBackfaceCulling();

    DrawModel(mesh->model, {0,0,0}, 1.0f, mat->color);
    if (mat->outline_color.a > 0) DrawModelWires(mesh->model, {0,0,0}, 1.0f, mat->outline_color);

    if (edited_mesh_is_double_sided) rlEnableBackfaceCulling();

    rlPopMatrix();
    draw_collision_debug(e);
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
                MeshComponent* mesh = entity.get_mesh_component();
                MaterialComponent* mat = entity.get_material_component();
                if (mesh && mat->texture.id == removed_tex.id) {
                    mat->texture = {0};
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
    if (project_path.empty()) return;

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
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (!mesh || mesh->model.materialCount <= 0) return;

    if (mesh->owns_materials) {
        if (mesh->model.materials)    RL_FREE(mesh->model.materials);
        if (mesh->model.meshMaterial) RL_FREE(mesh->model.meshMaterial);
        mesh->model.materials    = nullptr;
        mesh->model.meshMaterial = nullptr;
        mesh->owns_materials     = false;
    }

    if (!mesh->asset || !mesh->asset->loaded_model.materials) return;

    Material* cloned = (Material*)RL_MALLOC(mesh->model.materialCount * sizeof(Material));
    memcpy(cloned, mesh->asset->loaded_model.materials, mesh->model.materialCount * sizeof(Material));
    mesh->model.materials = cloned;

    mesh->model.meshMaterial = (int*)RL_MALLOC(mesh->model.meshCount * sizeof(int));
    memcpy(mesh->model.meshMaterial, mesh->asset->loaded_model.meshMaterial, mesh->model.meshCount * sizeof(int));

    mesh->owns_materials = true;
}

RenderTexture2D load_shadowmap_render_texture(int width, int height) {
    RenderTexture2D target = {0};

    target.id = rlLoadFramebuffer();
    target.texture.width = width;
    target.texture.height = height;

    if (target.id > 0) {
        rlEnableFramebuffer(target.id);

        unsigned char* data = (unsigned char*)malloc(width * height * 4);
        memset(data, 255, width * height * 4);
        target.texture.id = rlLoadTexture(data, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        free(data);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.texture.mipmaps = 1;

        target.depth.id = rlLoadTextureDepth(width, height, false);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;
        target.depth.mipmaps = 1;

        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);
        else TRACELOG(LOG_WARNING, "FBO: Framebuffer object is not complete");
        rlDisableFramebuffer();
    }

    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object could not be created");
    return target;
}

void unload_shadowmap_render_texture(RenderTexture2D& target) {
    if (target.id > 0) {
        rlUnloadFramebuffer(target.id);
    }
}
