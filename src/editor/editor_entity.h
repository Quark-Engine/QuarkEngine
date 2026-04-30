#pragma once

#include "../headers/scene.h"
#include "../headers/entity.h"

Entity make_entity_from_asset(Scene& scene, ModelAsset& asset);

void assign_entity_name(Entity& entity, const char* new_name);
