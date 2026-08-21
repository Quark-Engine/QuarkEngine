#ifndef __SCENE_H__
#define __SCENE_H__
#include <vector>
#include "entity.h"
#include "lighting.h"
#include <memory>
#include <string>

struct Scene {
    std::vector<Entity> entities;

    int selected = -1;

    Entity* get_selected();
    std::string make_unique_name(const std::string& base_name) const;
    std::string make_default_name_for(const Entity& entity) const;
    void release_resources();
};

#endif // __SCENE_H__
