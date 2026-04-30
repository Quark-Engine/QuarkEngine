#include "editor_entity.h"
#include "tex.h"

namespace fs = std::filesystem;

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

ModelAsset* find_asset_by_name(const std::string& asset_name) {
    for (auto& asset : assets) {
        if (asset.name == asset_name) return &asset;
    }
    return nullptr;
}