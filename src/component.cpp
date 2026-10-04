#include "component.h"
#define _CRT_SECURE_NO_WARNINGS

#include "engine/component_factory_registry.h"
#include "entity.h"
#include "nlohmann/json.hpp"

using namespace qc;

void CMeshComponent::Serialize(nlohmann::json& json) const
{
    json["segments"] = m_Segments;
    json["type"] = static_cast<int>(m_Type);
    
    if (!m_AssetName.empty())
    {
        json["asset_name"] = m_AssetName;
    }

    json["is_editable_mesh"] = m_IsEditableMesh;
    json["editable_vertices"] = nlohmann::json::array();

    for (auto& v : m_EditableMesh.m_vVertices)
    {
        json["editable_vertices"].push_back({
            v.Position.x,
            v.Position.y,
            v.Position.z,
            v.U,
            v.V
        });
    }

    json["editable_triangles"] = nlohmann::json::array();

    for (auto& t : m_EditableMesh.m_vTriangles)
    {
        json["editable_triangles"].push_back({
            t.A,
            t.B,
            t.C
        });
    }
}

void CMeshComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("segments")) m_Segments = json["segments"];
    if (json.contains("type")) m_Type = static_cast<EObjectType>(json["type"].get<int>());
    
    if (json.contains("asset_name"))
    {
        m_AssetName = json["asset_name"];
    }

    if (json.contains("is_editable_mesh"))
    {
        m_IsEditableMesh = json["is_editable_mesh"];
    }

    if (json.contains("editable_vertices"))
    {
        m_EditableMesh.m_vVertices.clear();
        for (auto& v : json["editable_vertices"])
        {
            SEditableVertex vert;
            vert.Position = qc::Vec3(v[0],
                v[1],
                v[2]);
            if (v.size() >= 5)
            {
                vert.U = v[3];
                vert.V = v[4];
            }

            m_EditableMesh.m_vVertices.push_back(vert);
        }
    }

    if (json.contains("editable_triangles"))
    {
        m_EditableMesh.m_vTriangles.clear();
        for (auto& t : json["editable_triangles"])
        {
            SEditableTriangle tri;

            tri.A = t[0];
            tri.B = t[1];
            tri.C = t[2];

            m_EditableMesh.m_vTriangles.push_back(tri);
        }
    }
}

void CLightComponent::Serialize(nlohmann::json& json) const
{
    json["light_enabled"] = m_Light.m_Enabled;
    json["light_position"] = {m_Light.m_Position.x, m_Light.m_Position.y, m_Light.m_Position.z};
    json["light_target"] = {m_Light.m_Target.x, m_Light.m_Target.y, m_Light.m_Target.z};
    json["light_rotation"] = {m_Light.m_Rotation.x, m_Light.m_Rotation.y, m_Light.m_Rotation.z};
    
    char aColorBuf[16];
    snprintf(aColorBuf, sizeof(aColorBuf), "%02X%02X%02X%02X", m_Light.m_Color.r, m_Light.m_Color.g, m_Light.m_Color.b, m_Light.m_Color.a);
    json["light_color"] = std::string(aColorBuf);
    
    json["light_intensity"] = m_Light.m_Intensity;
    json["light_range"] = m_Light.m_Range;
    json["light_spot_angle"] = m_Light.m_SpotAngle;
    json["light_type"] = m_Light.m_Light.type;
}

CLightComponent::CLightComponent() : IComponent(COMPONENT_LIGHT, "Light"), m_Created(false)
{
    m_Light = CreateLighting({0, 0, 0}, qc::WHITE);
}

void CLightComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("light_enabled")) m_Light.m_Enabled = json["light_enabled"];
    if (json.contains("light_position"))
    {
        auto& p = json["light_position"];
        m_Light.m_Position = qc::Vec3(p[0], p[1], p[2]);
    }
    if (json.contains("light_target"))
    {
        auto& t = json["light_target"];
        m_Light.m_Target = qc::Vec3(t[0], t[1], t[2]);
    }
    if (json.contains("light_rotation"))
    {
        auto& r = json["light_rotation"];
        m_Light.m_Rotation = qc::Vec3(r[0], r[1], r[2]);
    }
    if (json.contains("light_color"))
    {
        std::string hex = json["light_color"];
        unsigned int rgba = std::stoul(hex, nullptr, 16);
        m_Light.m_Color = {
            static_cast<unsigned char>((rgba >> 24) & 0xFF),
            static_cast<unsigned char>((rgba >> 16) & 0xFF),
            static_cast<unsigned char>((rgba >> 8) & 0xFF),
            static_cast<unsigned char>(rgba & 0xFF)
        };
    }
    if (json.contains("light_intensity")) m_Light.m_Intensity = json["light_intensity"];
    if (json.contains("light_range")) m_Light.m_Range = json["light_range"];
    if (json.contains("light_spot_angle")) m_Light.m_SpotAngle = json["light_spot_angle"];
    if (json.contains("light_type")) m_Light.m_Light.type = json["light_type"];
}

void CLightComponent::OnEntityTransformChanged()
{
}

void CCollisionComponent::Serialize(nlohmann::json& json) const
{
    json["collider_type"] = static_cast<int>(m_ColliderType);
    json["is_trigger"] = m_IsTrigger;

    json["size"] = {m_Size.x, m_Size.y, m_Size.z};
    json["radius"] = m_Radius;
    json["height"] = m_Height;

    json["center"] = {m_Center.x, m_Center.y, m_Center.z};
}

void CCollisionComponent::Deserialize(const nlohmann::json& json)
{
    if (json.contains("collider_type")) m_ColliderType = static_cast<EColliderType>(json["collider_type"].get<int>());
    if (json.contains("is_trigger")) m_IsTrigger = json["is_trigger"];

    if (json.contains("size"))
    {
        auto& s = json["size"];
        m_Size = qc::Vec3(s[0], s[1], s[2]);
    }

    if (json.contains("radius")) m_Radius = json["radius"];
    if (json.contains("height")) m_Height = json["height"];
    
    if (json.contains("center"))
    {
        auto& c = json["center"];
        m_Center = qc::Vec3(c[0], c[1], c[2]);
    }

    m_Dirty = true;
}

void CCollisionComponent::OnEntityTransformChanged()
{
    m_Dirty = true;
}

void CComponentManager::Deserialize(const nlohmann::json& json, const CComponentFactoryRegistry& factories)
{
    if (!json.contains("components")) return;

    m_vComponents.clear();
    for (const auto& compJson : json["components"])
    {
        const std::string typeName = compJson["type"];
        const bool compEnabled = compJson.value("enabled", true);


        std::shared_ptr<IComponent> pComp = factories.Create(typeName);
        if (!pComp)
        {
            TraceLog(LogLevel::Warn, "COMPONENT",
                TextFormat("Dropping component '%s': no factory is registered for this type name.",
                    typeName.c_str()));
            continue;
        }

        if (compJson.contains("data"))
        {
            pComp->Deserialize(compJson["data"]);
        }
        pComp->m_Enabled = compEnabled;
        m_vComponents.push_back(pComp);
    }
}

void CComponentManager::CloneInto(const CComponentManager& source)
{
    m_vComponents.clear();
    m_vComponents.reserve(source.m_vComponents.size());

    for (const auto& pComponent : source.m_vComponents)
    {
        if (!pComponent)
        {
            continue;
        }

        std::shared_ptr<IComponent> pCloned;
        switch (pComponent->GetType())
        {
        case COMPONENT_TRANSFORM:
            pCloned = std::make_shared<CTransformComponent>(
                *std::dynamic_pointer_cast<CTransformComponent>(pComponent));
            break;

        case COMPONENT_MESH:
            pCloned = std::make_shared<CMeshComponent>(
                *std::dynamic_pointer_cast<CMeshComponent>(pComponent));
            break;

        case COMPONENT_LIGHT:
            pCloned = std::make_shared<CLightComponent>(
                *std::dynamic_pointer_cast<CLightComponent>(pComponent));
            break;

        case COMPONENT_MATERIAL:
            pCloned = std::make_shared<CMaterialComponent>(
                *std::dynamic_pointer_cast<CMaterialComponent>(pComponent));
            break;

        case COMPONENT_COLLISION:
            pCloned = std::make_shared<CCollisionComponent>(
                *std::dynamic_pointer_cast<CCollisionComponent>(pComponent));
            break;

        default:
            break;
        }

        if (pCloned)
        {
            m_vComponents.push_back(pCloned);
        }
    }
}
