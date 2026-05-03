#pragma once
#include "raylib.h"
#include "lighting.h"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <typeinfo>

struct Entity;
struct ModelAsset;

enum ObjectType { CUBE, SPHERE, CONE, CYLINDER, HEMISPHERE, TORUS };

enum TextureSource {
    TEXTURE_NONE,
    TEXTURE_EXTERNAL,
    TEXTURE_MODEL
};

enum ComponentType {
    COMPONENT_TRANSFORM,
    COMPONENT_MESH,
    COMPONENT_LIGHT,
    COMPONENT_CUSTOM
};

class Component {
public:
    std::string name;
    bool enabled = true;
    ComponentType type;

    Component() = default;
    Component(ComponentType type_val, const std::string& name_val)
        : name(name_val), type(type_val) {}

    virtual ~Component() = default;
    virtual void serialize(nlohmann::json& json) const {}
    virtual void deserialize(const nlohmann::json& json) {}
    virtual ComponentType get_type() const { return COMPONENT_CUSTOM; }
    virtual std::string get_type_name() const { return "Custom"; }
    virtual void on_entity_transform_changed() {}
};

class TransformComponent : public Component {
public:
    Vector3 position = {0, 0, 0};
    Vector3 rotation = {0, 0, 0};
    Vector3 scale = {1, 1, 1};

    TransformComponent() {
        name = "Transform";
        type = COMPONENT_TRANSFORM;
    }

    ComponentType get_type() const override { return COMPONENT_TRANSFORM; }
    std::string get_type_name() const override { return "Transform"; }

    void serialize(nlohmann::json& json) const override {
        json["position"] = {position.x, position.y, position.z};
        json["rotation"] = {rotation.x, rotation.y, rotation.z};
        json["scale"] = {scale.x, scale.y, scale.z};
    }

    void deserialize(const nlohmann::json& json) override {
        if (json.contains("position")) {
            auto& p = json["position"];
            position = {p[0], p[1], p[2]};
        }
        if (json.contains("rotation")) {
            auto& r = json["rotation"];
            rotation = {r[0], r[1], r[2]};
        }
        if (json.contains("scale")) {
            auto& s = json["scale"];
            scale = {s[0], s[1], s[2]};
        }
    }
};

class MeshComponent : public Component {
public:
    Model model = {0};
    bool owns_model_instance = false;
    ModelAsset* asset = nullptr;
    std::string asset_name;
    bool mesh_triangles_detached = false;
    std::vector<std::vector<float>> mesh_vertex_overrides;

    Texture2D texture = {0};
    TextureSource texture_source = TEXTURE_NONE;
    std::string texture_name;
    std::vector<Texture2D> original_material_textures;

    bool auto_uv = false;
    bool texture_stretch = true;
    float texture_repeat_u = 1.0f;
    float texture_repeat_v = 1.0f;
    Vector2 uv_scale_vec = {1, 1};
    float uv_scale = 1.0f;
    std::vector<std::vector<float>> original_texcoords;

    int segments = 16;
    ObjectType type = CUBE;

    Color color = WHITE;
    Color outline_color = LIGHTGRAY;

    bool shader_assigned = false;
    bool owns_materials = false;
    bool uv_dirty = true;
    bool bounds_dirty = true;
    BoundingBox cached_local_bounds = {{0, 0, 0}, {0, 0, 0}};

    MeshComponent() {
        name = "Mesh";
    }

    ComponentType get_type() const override { return COMPONENT_MESH; }
    std::string get_type_name() const override { return "Mesh"; }

    void serialize(nlohmann::json& json) const override;
    void deserialize(const nlohmann::json& json) override;
};

class LightComponent : public Component {
public:
    bool created = false;
    Lighting light;
    LightComponent();

    ComponentType get_type() const override { return COMPONENT_LIGHT; }
    std::string get_type_name() const override { return "Light"; }

    void serialize(nlohmann::json& json) const override;
    void deserialize(const nlohmann::json& json) override;
    void on_entity_transform_changed() override;
};

class ComponentManager {
private:
    std::vector<std::shared_ptr<Component>> components;

public:
    void add_component(std::shared_ptr<Component> component) {
        components.push_back(component);
    }

    void remove_component(size_t index) {
        if (index < components.size()) {
            components.erase(components.begin() + index);
        }
    }

    std::shared_ptr<Component> get_component(size_t index) {
        if (index < components.size()) {
            return components[index];
        }
        return nullptr;
    }

    size_t get_component_count() const {
        return components.size();
    }

    template<typename T>
    std::shared_ptr<T> get_component_of_type() {
        for (auto& comp : components) {
            auto casted = std::dynamic_pointer_cast<T>(comp);
            if (casted) return casted;
        }
        return nullptr;
    }

    TransformComponent* get_transform() {
        auto comp = get_component_of_type<TransformComponent>();
        return comp ? comp.get() : nullptr;
    }

    MeshComponent* get_mesh() {
        auto comp = get_component_of_type<MeshComponent>();
        return comp ? comp.get() : nullptr;
    }

    LightComponent* get_light() {
        auto comp = get_component_of_type<LightComponent>();
        return comp ? comp.get() : nullptr;
    }

    const std::vector<std::shared_ptr<Component>>& get_all_components() const {
        return components;
    }

    std::vector<std::shared_ptr<Component>>& get_all_components() {
        return components;
    }

    void serialize(nlohmann::json& json) const {
        json["components"] = nlohmann::json::array();
        for (const auto& comp : components) {
            nlohmann::json comp_json;
            comp_json["type"] = comp->get_type_name();
            comp_json["enabled"] = comp->enabled;
            nlohmann::json data;
            comp->serialize(data);
            comp_json["data"] = data;
            json["components"].push_back(comp_json);
        }
    }

    void deserialize(const nlohmann::json& json);
};
