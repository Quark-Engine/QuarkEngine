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
    entity.id = static_cast<int>(scene.entities.size());
    entity.type = asset.type;
    entity.asset = &asset;
    entity.asset_name = asset.name;
    entity.segments = 16;
    const std::string base_name = asset.is_procedural ? object_type_name(asset.type) : fs::path(asset.name).stem().string();
    entity.name = scene.make_unique_name(base_name.empty() ? "Model" : base_name);

    if (asset.is_procedural) {
        entity.model = asset.generator(entity.segments);
        entity.owns_model_instance = true;
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);
        entity.texture_source = TEXTURE_NONE;
        entity.texture_name.clear();
    } 
    else {
        if (!load_model_instance(asset, entity.model)) {
            entity.asset = nullptr;
            entity.asset_name.clear();
            entity.model = {0};
            return entity;
        }

        entity.owns_model_instance = true;
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);

        bool has_embedded = false;
        for (int i = 0; i < entity.model.materialCount; i++) {
            if (entity.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                has_embedded = true;
                break;
            }
        }

        if (has_embedded) entity.texture_source = TEXTURE_MODEL;
        else entity.texture_source = TEXTURE_NONE;
    }

    entity.texture = {0};
    return entity;
}
