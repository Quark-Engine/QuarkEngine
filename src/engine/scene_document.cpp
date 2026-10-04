#include "engine/scene_document.h"

#include "version.h"
#include "nlohmann/json.hpp"

namespace quark
{

std::string CSceneDocument::Serialize(const CScene& scene, int indent)
{
    nlohmann::json j;
    j["entities"] = nlohmann::json::array();
    j["version"] = QUARK_ENGINE_VERSION;

    for (const CEntity& entity : scene.m_vEntities)
    {
        nlohmann::json ej;
        ej["name"] = entity.m_Name;
        ej["tags"] = entity.m_vTags;
        ej["is_group"] = entity.m_IsGroup;
        ej["parent_id"] = entity.m_ParentId;

        if (entity.m_pComponents)
        {
            entity.m_pComponents->Serialize(ej);
        }

        j["entities"].push_back(ej);
    }

    return j.dump(indent);
}

SParsedSceneDocument CSceneDocument::Parse(const std::string& document)
{
    SParsedSceneDocument parsed;
    parsed.Document = nlohmann::json::parse(document, nullptr, false);
    parsed.IsValid = !parsed.Document.is_discarded() && parsed.Document.is_object() &&
        parsed.Document.contains("entities") && parsed.Document["entities"].is_array();
    if (!parsed.IsValid)
    {
        return parsed;
    }

    try
    {
        const auto& entities = parsed.Document["entities"];
        for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
        {
            const auto& entity = entities[entityIndex];
            if (!entity.is_object() ||
                (entity.contains("name") && !entity["name"].is_string()) ||
                (entity.contains("tags") && !entity["tags"].is_array()) ||
                (entity.contains("is_group") && !entity["is_group"].is_boolean()) ||
                (entity.contains("parent_id") && !entity["parent_id"].is_number_integer()))
            {
                parsed.IsValid = false;
                break;
            }
            if (entity.contains("tags"))
            {
                for (const auto& tag : entity["tags"])
                {
                    if (!tag.is_string())
                    {
                        parsed.IsValid = false;
                        break;
                    }
                }
            }
            if (!parsed.IsValid)
            {
                break;
            }
            const auto& components = entity.value("components", nlohmann::json::array());
            if (!components.is_array())
            {
                parsed.IsValid = false;
                break;
            }
            for (const auto& component : components)
            {
                if (!component.is_object() || !component.contains("type") ||
                    !component["type"].is_string() ||
                    (component.contains("data") && !component["data"].is_object()))
                {
                    parsed.IsValid = false;
                    break;
                }
                if (component["type"] != "Mesh" || !component.contains("data"))
                {
                    continue;
                }

                const auto& data = component["data"];
                if (!data.contains("editable_vertices") || !data.contains("editable_triangles"))
                {
                    continue;
                }
                if (!data["editable_vertices"].is_array() || !data["editable_triangles"].is_array())
                {
                    parsed.IsValid = false;
                    break;
                }

                CEditableMesh editableMesh;
                for (const auto& vertexJson : data["editable_vertices"])
                {
                    if (!vertexJson.is_array() || vertexJson.size() < 3 || vertexJson.size() > 5 ||
                        !vertexJson[0].is_number() || !vertexJson[1].is_number() ||
                        !vertexJson[2].is_number() ||
                        (vertexJson.size() >= 5 &&
                            (!vertexJson[3].is_number() || !vertexJson[4].is_number())))
                    {
                        parsed.IsValid = false;
                        break;
                    }
                    SEditableVertex vertex;
                    vertex.Position = {
                        vertexJson[0].get<float>(),
                        vertexJson[1].get<float>(),
                        vertexJson[2].get<float>()
                    };
                    if (vertexJson.size() >= 5)
                    {
                        vertex.U = vertexJson[3].get<float>();
                        vertex.V = vertexJson[4].get<float>();
                    }
                    editableMesh.m_vVertices.push_back(vertex);
                }
                if (!parsed.IsValid)
                {
                    break;
                }

                for (const auto& triangleJson : data["editable_triangles"])
                {
                    if (!triangleJson.is_array() || triangleJson.size() < 3 ||
                        !triangleJson[0].is_number_integer() ||
                        !triangleJson[1].is_number_integer() ||
                        !triangleJson[2].is_number_integer())
                    {
                        parsed.IsValid = false;
                        break;
                    }
                    editableMesh.m_vTriangles.push_back({
                        triangleJson[0].get<int>(),
                        triangleJson[1].get<int>(),
                        triangleJson[2].get<int>()
                    });
                }
                if (!parsed.IsValid)
                {
                    break;
                }

                if (!editableMesh.m_vVertices.empty() || !editableMesh.m_vTriangles.empty())
                {
                    parsed.vEditableMeshes.push_back({
                        entityIndex,
                        std::move(editableMesh)
                    });
                }
            }
            if (!parsed.IsValid)
            {
                break;
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        parsed.IsValid = false;
    }

    if (!parsed.IsValid)
    {
        parsed.vEditableMeshes.clear();
    }
    return parsed;
}

bool CSceneDocument::Deserialize(const SParsedSceneDocument& parsed, CScene& scene,
    const CComponentFactoryRegistry& factories)
{
    if (!parsed.IsValid)
    {
        return false;
    }

    const nlohmann::json& j = parsed.Document;
    std::vector<CEntity> vEntities;
    vEntities.reserve(j["entities"].size());

    for (const auto& ej : j["entities"])
    {
        CEntity entity;
        entity.m_Id = static_cast<int>(vEntities.size());
        if (ej.contains("name") && ej["name"].is_string())
        {
            entity.m_Name = ej["name"].get<std::string>();
        }
        if (ej.contains("tags") && ej["tags"].is_array())
        {
            entity.m_vTags = ej["tags"].get<std::vector<std::string>>();
        }
        if (ej.contains("is_group"))
        {
            entity.m_IsGroup = ej["is_group"].get<bool>();
        }
        if (ej.contains("parent_id"))
        {
            entity.m_ParentId = ej["parent_id"].get<int>();
        }
        if (ej.contains("components"))
        {
            entity.m_pComponents->Deserialize(ej, factories);
        }

        vEntities.push_back(std::move(entity));
    }

    scene.m_vEntities = std::move(vEntities);
    return true;
}

bool CSceneDocument::Deserialize(const std::string& document, CScene& scene,
    const CComponentFactoryRegistry& factories)
{
    return Deserialize(Parse(document), scene, factories);
}

SSceneSnapshot CSceneDocument::CaptureSnapshot(const CScene& scene)
{
    SSceneSnapshot snapshot;
    snapshot.Document = Serialize(scene, 0);
    snapshot.Selected = scene.m_Selected;
    snapshot.vSelectedEntities = scene.m_vSelectedEntities;
    return snapshot;
}

} // quark
