#include "headers/scene.h"
#include "headers/models.h"
#include <unordered_set>

Entity* Scene::get_selected() {
    if (selected < 0 || selected >= static_cast<int>(entities.size())) return nullptr;
    return &entities[selected];
}

std::string Scene::make_unique_name(const std::string& base_name) const {
    auto is_name_taken = [this](const std::string& candidate) {
        for (const auto& entity : entities) {
            if (entity.name == candidate) return true;
        }
        return false;
    };

    if (!is_name_taken(base_name)) return base_name;

    for (int suffix = 1;; ++suffix) {
        const std::string candidate = base_name + " (" + std::to_string(suffix) + ")";
        if (!is_name_taken(candidate)) return candidate;
    }
}

std::string Scene::make_default_name_for(const Entity& entity) const {
    const std::string base_name = entity.has_light ? "Light" : object_type_name(entity.type);
    return make_unique_name(base_name);
}

void Scene::release_resources() {
    std::unordered_set<void*> released_meshes;

    for (auto& entity : entities) {
        const bool owns_model = entity_owns_model(entity);
        if (entity.owns_materials) {
            if (entity.model.materials) {
                RL_FREE(entity.model.materials);
                entity.model.materials = nullptr;
            }

            if (entity.model.meshMaterial) {
                RL_FREE(entity.model.meshMaterial);
                entity.model.meshMaterial = nullptr;
            }

            entity.owns_materials = false;
        }

        if (owns_model && entity.model.meshCount > 0 && entity.model.meshes) {
            void* mesh_ptr = static_cast<void*>(entity.model.meshes);
            if (released_meshes.insert(mesh_ptr).second) {
                UnloadModel(entity.model);
            }
        }
        entity.model = {0};
        entity.texture = {0};
    }

    entities.clear();
    selected = -1;
}
