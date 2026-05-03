#include "../headers/component.h"
#define _CRT_SECURE_NO_WARNINGS

#include "../headers/entity.h"
#include "nlohmann/json.hpp"

void MeshComponent::serialize(nlohmann::json& json) const {
    json["segments"] = segments;
    json["type"] = static_cast<int>(type);
    json["auto_uv"] = auto_uv;
    json["texture_stretch"] = texture_stretch;
    json["texture_repeat_u"] = texture_repeat_u;
    json["texture_repeat_v"] = texture_repeat_v;
    json["uv_scale"] = uv_scale;
    json["uv_scale_vec"] = {uv_scale_vec.x, uv_scale_vec.y};
    
    char color_buf[16];
    sprintf(color_buf, "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    json["color"] = std::string(color_buf);
    
    sprintf(color_buf, "%02X%02X%02X%02X", outline_color.r, outline_color.g, outline_color.b, outline_color.a);
    json["outline_color"] = std::string(color_buf);
    
    if (!asset_name.empty()) {
        json["asset_name"] = asset_name;
    }
    
    if (texture_source != TEXTURE_NONE) {
        json["texture_source"] = static_cast<int>(texture_source);
        json["texture_name"] = texture_name;
    }
}

void MeshComponent::deserialize(const nlohmann::json& json) {
    if (json.contains("segments")) segments = json["segments"];
    if (json.contains("type")) type = static_cast<ObjectType>(json["type"].get<int>());
    if (json.contains("auto_uv")) auto_uv = json["auto_uv"];
    if (json.contains("texture_stretch")) texture_stretch = json["texture_stretch"];
    if (json.contains("texture_repeat_u")) texture_repeat_u = json["texture_repeat_u"];
    if (json.contains("texture_repeat_v")) texture_repeat_v = json["texture_repeat_v"];
    if (json.contains("uv_scale")) uv_scale = json["uv_scale"];
    if (json.contains("uv_scale_vec")) {
        auto& uv = json["uv_scale_vec"];
        uv_scale_vec = {uv[0], uv[1]};
    }
    
    if (json.contains("color")) {
        std::string hex = json["color"];
        unsigned int rgba = std::stoul(hex, nullptr, 16);
        color = {
            static_cast<unsigned char>((rgba >> 24) & 0xFF),
            static_cast<unsigned char>((rgba >> 16) & 0xFF),
            static_cast<unsigned char>((rgba >> 8) & 0xFF),
            static_cast<unsigned char>(rgba & 0xFF)
        };
    }
    
    if (json.contains("outline_color")) {
        std::string hex = json["outline_color"];
        unsigned int rgba = std::stoul(hex, nullptr, 16);
        outline_color = {
            static_cast<unsigned char>((rgba >> 24) & 0xFF),
            static_cast<unsigned char>((rgba >> 16) & 0xFF),
            static_cast<unsigned char>((rgba >> 8) & 0xFF),
            static_cast<unsigned char>(rgba & 0xFF)
        };
    }
    
    if (json.contains("asset_name")) {
        asset_name = json["asset_name"];
    }
    
    if (json.contains("texture_source")) {
        texture_source = static_cast<TextureSource>(json["texture_source"].get<int>());
        if (json.contains("texture_name")) {
            texture_name = json["texture_name"];
        }
    }
}

void LightComponent::serialize(nlohmann::json& json) const {
    json["light_enabled"] = light.enabled;
    json["light_position"] = {light.position.x, light.position.y, light.position.z};
    json["light_target"] = {light.target.x, light.target.y, light.target.z};
    json["light_rotation"] = {light.rotation.x, light.rotation.y, light.rotation.z};
    
    char color_buf[16];
    sprintf(color_buf, "%02X%02X%02X%02X", light.color.r, light.color.g, light.color.b, light.color.a);
    json["light_color"] = std::string(color_buf);
    
    json["light_intensity"] = light.intensity;
    json["light_range"] = light.range;
    json["light_spot_angle"] = light.spot_angle;
    json["light_type"] = light.light.type;
}

LightComponent::LightComponent() : Component(COMPONENT_LIGHT, "Light"), created(false) {
    light = create_lighting({0, 0, 0}, WHITE);
}

void LightComponent::deserialize(const nlohmann::json& json) {
    if (json.contains("light_enabled")) light.enabled = json["light_enabled"];
    if (json.contains("light_position")) {
        auto& p = json["light_position"];
        light.position = {p[0], p[1], p[2]};
    }
    if (json.contains("light_target")) {
        auto& t = json["light_target"];
        light.target = {t[0], t[1], t[2]};
    }
    if (json.contains("light_rotation")) {
        auto& r = json["light_rotation"];
        light.rotation = {r[0], r[1], r[2]};
    }
    if (json.contains("light_color")) {
        std::string hex = json["light_color"];
        unsigned int rgba = std::stoul(hex, nullptr, 16);
        light.color = {
            static_cast<unsigned char>((rgba >> 24) & 0xFF),
            static_cast<unsigned char>((rgba >> 16) & 0xFF),
            static_cast<unsigned char>((rgba >> 8) & 0xFF),
            static_cast<unsigned char>(rgba & 0xFF)
        };
    }
    if (json.contains("light_intensity")) light.intensity = json["light_intensity"];
    if (json.contains("light_range")) light.range = json["light_range"];
    if (json.contains("light_spot_angle")) light.spot_angle = json["light_spot_angle"];
    if (json.contains("light_type")) light.light.type = json["light_type"];
}

void LightComponent::on_entity_transform_changed() {
}

void ComponentManager::deserialize(const nlohmann::json& json) {
    if (!json.contains("components")) return;
    
    components.clear();
    for (const auto& comp_json : json["components"]) {
        std::string type_name = comp_json["type"];
        bool comp_enabled = comp_json.value("enabled", true);
        
        std::shared_ptr<Component> comp;
        
        if (type_name == "Transform") {
            auto transform = std::make_shared<TransformComponent>();
            if (comp_json.contains("data")) {
                transform->deserialize(comp_json["data"]);
            }
            comp = transform;
        }
        else if (type_name == "Mesh") {
            auto mesh = std::make_shared<MeshComponent>();
            if (comp_json.contains("data")) {
                mesh->deserialize(comp_json["data"]);
            }
            comp = mesh;
        }
        else if (type_name == "Light") {
            auto light = std::make_shared<LightComponent>();
            if (comp_json.contains("data")) {
                light->deserialize(comp_json["data"]);
            }
            comp = light;
        }
        
        if (comp) {
            comp->enabled = comp_enabled;
            components.push_back(comp);
        }
    }
}

ComponentManager* Entity::get_components() {
    return components.get();
}

const ComponentManager* Entity::get_components() const {
    return components.get();
}

TransformComponent* Entity::get_transform_component() {
    return components ? components->get_transform() : nullptr;
}

const TransformComponent* Entity::get_transform_component() const {
    return components ? components->get_transform() : nullptr;
}

MeshComponent* Entity::get_mesh_component() {
    return components ? components->get_mesh() : nullptr;
}

const MeshComponent* Entity::get_mesh_component() const {
    return components ? components->get_mesh() : nullptr;
}

LightComponent* Entity::get_light_component() {
    return components ? components->get_light() : nullptr;
}

const LightComponent* Entity::get_light_component() const {
    return components ? components->get_light() : nullptr;
}
