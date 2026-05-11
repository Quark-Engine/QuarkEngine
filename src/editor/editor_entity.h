#pragma once

#include <filesystem>
#include "../headers/scene.h"
#include "../headers/entity.h"

namespace fs = std::filesystem;

Entity make_entity_from_asset(Scene& scene, ModelAsset& asset);
Entity make_entity_from_prefab(Scene& scene);

void assign_entity_name(Entity& entity, const char* new_name);
void make_prefab(Entity entity, const fs::path path);