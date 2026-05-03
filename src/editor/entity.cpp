#include "headers/entity.h"
#include "headers/component.h"

Entity::Entity() 
    : id(0),
      position{0, 0, 0},
      rotation{0, 0, 0},
      scale{1, 1, 1},
      auto_uv(false),
      texture_repeat_u(1.0f),
      texture_repeat_v(1.0f),
      uv_scale(1.0f),
      uv_scale_vec{1, 1},
      color(WHITE),
      outline_color(LIGHTGRAY),
      asset(nullptr),
      segments(16),
      type(CUBE),
      has_light(false),
      light_created(false)
{
    components = std::make_unique<ComponentManager>();
}

Entity::Entity(int _id) 
    : id(_id),
      position{0, 0, 0},
      rotation{0, 0, 0},
      scale{1, 1, 1},
      auto_uv(false),
      texture_repeat_u(1.0f),
      texture_repeat_v(1.0f),
      uv_scale(1.0f),
      uv_scale_vec{1, 1},
      color(WHITE),
      outline_color(LIGHTGRAY),
      asset(nullptr),
      segments(16),
      type(CUBE),
      has_light(false),
      light_created(false)
{
    name = object_type_name(type);

    components = std::make_unique<ComponentManager>();
}

Entity::~Entity() {
}

Entity::Entity(const Entity& other)
    : id(other.id),
      name(other.name),
      position(other.position),
      rotation(other.rotation),
      scale(other.scale),
      texture(other.texture),
      texture_source(other.texture_source),
      texture_name(other.texture_name),
      original_material_textures(other.original_material_textures),
      auto_uv(other.auto_uv),
      texture_stretch(other.texture_stretch),
      model(other.model),
      owns_model_instance(other.owns_model_instance),
      asset(other.asset),
      asset_name(other.asset_name),
      mesh_triangles_detached(other.mesh_triangles_detached),
      mesh_vertex_overrides(other.mesh_vertex_overrides),
      texture_repeat_u(other.texture_repeat_u),
      texture_repeat_v(other.texture_repeat_v),
      uv_scale_vec(other.uv_scale_vec),
      uv_scale(other.uv_scale),
      original_texcoords(other.original_texcoords),
      segments(other.segments),
      type(other.type),
      color(other.color),
      outline_color(other.outline_color),
      has_light(other.has_light),
      light_created(other.light_created),
      shader_assigned(other.shader_assigned),
      owns_materials(other.owns_materials),
      uv_dirty(other.uv_dirty),
      bounds_dirty(other.bounds_dirty),
      cached_local_bounds(other.cached_local_bounds),
      light(other.light)
{
    components = std::make_unique<ComponentManager>();
    if (other.components) {
        for (size_t i = 0; i < other.components->get_component_count(); ++i) {
            auto comp = other.components->get_component(i);
            if (!comp) continue;
            
            std::shared_ptr<Component> cloned;
            if (comp->get_type() == COMPONENT_TRANSFORM) {
                auto src = std::dynamic_pointer_cast<TransformComponent>(comp);
                auto dst = std::make_shared<TransformComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_MESH) {
                auto src = std::dynamic_pointer_cast<MeshComponent>(comp);
                auto dst = std::make_shared<MeshComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_LIGHT) {
                auto src = std::dynamic_pointer_cast<LightComponent>(comp);
                auto dst = std::make_shared<LightComponent>(*src);
                cloned = dst;
            }
            
            if (cloned) {
                components->add_component(cloned);
            }
        }
    }
}

Entity& Entity::operator=(const Entity& other) {
    if (this == &other) return *this;
    
    id = other.id;
    name = other.name;
    position = other.position;
    rotation = other.rotation;
    scale = other.scale;
    texture = other.texture;
    texture_source = other.texture_source;
    texture_name = other.texture_name;
    original_material_textures = other.original_material_textures;
    auto_uv = other.auto_uv;
    texture_stretch = other.texture_stretch;
    model = other.model;
    owns_model_instance = other.owns_model_instance;
    asset = other.asset;
    asset_name = other.asset_name;
    mesh_triangles_detached = other.mesh_triangles_detached;
    mesh_vertex_overrides = other.mesh_vertex_overrides;
    texture_repeat_u = other.texture_repeat_u;
    texture_repeat_v = other.texture_repeat_v;
    uv_scale_vec = other.uv_scale_vec;
    uv_scale = other.uv_scale;
    original_texcoords = other.original_texcoords;
    segments = other.segments;
    type = other.type;
    color = other.color;
    outline_color = other.outline_color;
    has_light = other.has_light;
    light_created = other.light_created;
    shader_assigned = other.shader_assigned;
    owns_materials = other.owns_materials;
    uv_dirty = other.uv_dirty;
    bounds_dirty = other.bounds_dirty;
    cached_local_bounds = other.cached_local_bounds;
    light = other.light;
    
    components = std::make_unique<ComponentManager>();
    if (other.components) {
        for (size_t i = 0; i < other.components->get_component_count(); ++i) {
            auto comp = other.components->get_component(i);
            if (!comp) continue;
            
            std::shared_ptr<Component> cloned;
            if (comp->get_type() == COMPONENT_TRANSFORM) {
                auto src = std::dynamic_pointer_cast<TransformComponent>(comp);
                auto dst = std::make_shared<TransformComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_MESH) {
                auto src = std::dynamic_pointer_cast<MeshComponent>(comp);
                auto dst = std::make_shared<MeshComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_LIGHT) {
                auto src = std::dynamic_pointer_cast<LightComponent>(comp);
                auto dst = std::make_shared<LightComponent>(*src);
                cloned = dst;
            }
            
            if (cloned) {
                components->add_component(cloned);
            }
        }
    }
    
    return *this;
}
