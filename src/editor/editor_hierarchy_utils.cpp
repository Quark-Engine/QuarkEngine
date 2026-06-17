#include "editor/editor_hierarchy_utils.h"

std::vector<int> get_entity_children(const Scene& scene, int parent_id) {
    std::vector<int> children;
    for (int i = 0; i < static_cast<int>(scene.entities.size()); i++) {
        if (scene.entities[i].parent_id == parent_id) {
            children.push_back(i);
        }
    }
    return children;
}

std::vector<int> get_entity_descendants(const Scene& scene, int entity_id) {
    std::vector<int> descendants;
    std::vector<int> to_process = get_entity_children(scene, entity_id);
    
    while (!to_process.empty()) {
        int current = to_process.back();
        to_process.pop_back();
        
        descendants.push_back(current);
        
        auto children = get_entity_children(scene, current);
        for (int child : children) {
            to_process.push_back(child);
        }
    }
    
    return descendants;
}

void move_entity_to_parent(Scene& scene, int entity_id, int new_parent_id) {
    if (entity_id < 0 || entity_id >= static_cast<int>(scene.entities.size())) return;
    if (new_parent_id == entity_id) return;
    
    if (new_parent_id >= 0) {
        auto descendants = get_entity_descendants(scene, entity_id);
        for (int desc : descendants) {
            if (desc == new_parent_id) return;
        }
    }
    
    scene.entities[entity_id].parent_id = new_parent_id;
}

int create_group(Scene& scene, const std::string& name, int parent_id) {
    Entity group;
    group.id = static_cast<int>(scene.entities.size());
    group.name = name;
    group.parent_id = parent_id;
    group.is_group = true;
    
    scene.entities.push_back(group);
    return group.id;
}

void delete_group(Scene& scene, int group_id, bool reparent_to_parent) {
    if (group_id < 0 || group_id >= static_cast<int>(scene.entities.size())) return;
    
    Entity& group = scene.entities[group_id];
    if (!group.is_group) return;
    
    int parent_of_group = group.parent_id;
    
    if (reparent_to_parent) {
        auto children = get_entity_children(scene, group_id);
        for (int child : children) {
            scene.entities[child].parent_id = parent_of_group;
        }
    }
    
    scene.entities.erase(scene.entities.begin() + group_id);
    
    for (int i = group_id; i < static_cast<int>(scene.entities.size()); i++) {
        scene.entities[i].id = i;
    }
}

bool is_entity_group(const Entity& entity) {
    return entity.is_group;
}

std::vector<int> get_root_entities(const Scene& scene) {
    return get_entity_children(scene, -1);
}
