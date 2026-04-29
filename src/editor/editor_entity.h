#pragma once
#include "headers/entity.h"
#include "headers/scene.h"
#include "headers/models.h"

Entity make_entity_from_asset(Scene& scene, ModelAsset& asset);
ModelAsset* find_asset_by_name(const std::string& asset_name);