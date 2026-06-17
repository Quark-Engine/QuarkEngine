#pragma once

#include "../entity.h"
#include "../scene.h"
#include <vector>
#include <string>

std::vector<int> get_entity_children(const Scene& scene, int parent_id);
std::vector<int> get_entity_descendants(const Scene& scene, int entity_id);
std::vector<int> get_root_entities(const Scene& scene);

void move_entity_to_parent(Scene& scene, int entity_id, int new_parent_id);
int create_group(Scene& scene, const std::string& name, int parent_id = -1);
void delete_group(Scene& scene, int group_id, bool reparent_to_parent = true);
bool is_entity_group(const Entity& entity);