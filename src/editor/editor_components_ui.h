#pragma once
#include "headers/component.h"
#include "headers/entity.h"
#include "imgui.h"
#include "editor/editor.h"
#include <string>

class ComponentUIHelper {
public:
    static void draw_entity_inspector(Editor& editor, Entity& entity, Shader shader);
    
    static void draw_transform_component(Editor& editor, Entity& entity, TransformComponent* transform);
    static void draw_mesh_component(Editor& editor, Entity& entity, MeshComponent* mesh);
    static void draw_light_component(Editor& editor, Entity& entity, LightComponent* light, Shader shader);
    static void draw_material_component(Editor& editor, Entity& entity, MaterialComponent* material);
    
private:
    static bool should_show_component_menu;
    static int component_to_remove;
};

struct ComponentMenuItem {
    const char* name;
    const char* type_name;
};

static const ComponentMenuItem available_components[] = {
    {"Light", "Light"},
};

static const int available_components_count = sizeof(available_components) / sizeof(ComponentMenuItem);
