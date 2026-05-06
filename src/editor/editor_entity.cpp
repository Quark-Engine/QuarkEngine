#include "editor_entity.h"
#include "editor_assets.h"
#include "editor_utils.h"
#include "../headers/models.h"
#include "../headers/entity.h"
#include <filesystem>

namespace fs = std::filesystem;

void assign_entity_name(Entity& entity, const char* new_name) {
    if (!new_name || new_name[0] == '\0') return;
    entity.name = new_name;
}

Entity make_entity_from_asset(Scene& scene, ModelAsset& asset) {
    Entity entity;
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return entity;

    entity.id = static_cast<int>(scene.entities.size());
    mesh->type = asset.type;
    mesh->asset = &asset;
    mesh->asset_name = asset.name;
    mesh->segments = 16;
    const std::string base_name = asset.is_procedural ? object_type_name(asset.type) : fs::path(asset.name).stem().string();
    entity.name = scene.make_unique_name(base_name.empty() ? "Model" : base_name);

    auto mat_comp = std::make_shared<MaterialComponent>();
    entity.get_components()->add_component(mat_comp);
    MaterialComponent* mat = mat_comp.get();

    if (asset.is_procedural) {
        mesh->model = asset.generator(mesh->segments);
        mesh->owns_model_instance = true;
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);
        mat->texture_source = TEXTURE_NONE;
        mat->texture_name.clear();
    } 
    else {
        if (!load_model_instance(asset, mesh->model)) {
            mesh->asset = nullptr;
            mesh->asset_name.clear();
            mesh->model = {0};
            return entity;
        }

        mesh->owns_model_instance = true;
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);

        bool has_embedded = false;
        for (int i = 0; i < mesh->model.materialCount; i++) {
            if (mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                has_embedded = true;
                break;
            }
        }

        mat->texture_source = has_embedded ? TEXTURE_MODEL : TEXTURE_NONE;
    }

    mat->texture = {0};
    return entity;
}