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

bool CSceneDocument::Deserialize(const std::string& document, CScene& scene, const CComponentFactoryRegistry& factories)
{
    nlohmann::json j = nlohmann::json::parse(document, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("entities") || !j["entities"].is_array())
    {
        return false;
    }

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

SSceneSnapshot CSceneDocument::CaptureSnapshot(const CScene& scene)
{
    SSceneSnapshot snapshot;
    snapshot.Document = Serialize(scene, 0);
    snapshot.Selected = scene.m_Selected;
    snapshot.vSelectedEntities = scene.m_vSelectedEntities;
    return snapshot;
}

} // quark
