#pragma once

#include "../headers/scene.h"
#include "../headers/entity.h"

// Entity creation
Entity make_entity_from_asset(Scene& scene, ModelAsset& asset);

// Entity naming
void assign_entity_name(Entity& entity, const char* new_name);
