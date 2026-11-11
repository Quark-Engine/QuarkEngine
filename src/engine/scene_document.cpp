#include "engine/scene_document.h"

#include "engine/transform.h"
#include "version.h"
#include "nlohmann/json.hpp"

#include <unordered_map>
#include <unordered_set>

namespace quark
{

    namespace
    {

        void CollectPrefabBranchIndices(const CScene& scene, int entityIndex,
                                        std::unordered_set<int>& visited, std::vector<int>& vOutIndices)
        {
            if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
            {
                return;
            }
            if (!visited.insert(entityIndex).second)
            {
                return;
            }

            vOutIndices.push_back(entityIndex);

            for (int childIndex = 0; childIndex < static_cast<int>(scene.m_vEntities.size()); ++childIndex)
            {
                if (scene.m_vEntities[childIndex].m_ParentId == entityIndex)
                {
                    CollectPrefabBranchIndices(scene, childIndex, visited, vOutIndices);
                }
            }
        }

        std::string MakeUniqueEntityName(const CScene& scene, const std::string& baseName)
        {
            auto isNameTaken = [&scene](const std::string& candidate)
            {
                for (const auto& entity : scene.m_vEntities)
                {
                    if (entity.m_Name == candidate)
                    {
                        return true;
                    }
                }
                return false;
            };

            if (!isNameTaken(baseName))
            {
                return baseName;
            }

            for (int suffix = 1;; ++suffix)
            {
                const std::string candidate = baseName + " (" + std::to_string(suffix) + ")";
                if (!isNameTaken(candidate))
                {
                    return candidate;
                }
            }
        }

    } // anonymous

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
        parsed.m_Document = nlohmann::json::parse(document, nullptr, false);
        parsed.m_IsValid = !parsed.m_Document.is_discarded() && parsed.m_Document.is_object() &&
            parsed.m_Document.contains("entities") && parsed.m_Document["entities"].is_array();
        if (!parsed.m_IsValid)
        {
            return parsed;
        }

        try
        {
            const auto& entities = parsed.m_Document["entities"];
            for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
            {
                const auto& entity = entities[entityIndex];
                if (!entity.is_object() ||
                    (entity.contains("name") && !entity["name"].is_string()) ||
                    (entity.contains("tags") && !entity["tags"].is_array()) ||
                    (entity.contains("is_group") && !entity["is_group"].is_boolean()) ||
                    (entity.contains("parent_id") && !entity["parent_id"].is_number_integer()))
                {
                    parsed.m_IsValid = false;
                    break;
                }
                if (entity.contains("tags"))
                {
                    for (const auto& tag : entity["tags"])
                    {
                        if (!tag.is_string())
                        {
                            parsed.m_IsValid = false;
                            break;
                        }
                    }
                }
                if (!parsed.m_IsValid)
                {
                    break;
                }
                const auto& components = entity.value("components", nlohmann::json::array());
                if (!components.is_array())
                {
                    parsed.m_IsValid = false;
                    break;
                }
                for (const auto& component : components)
                {
                    if (!component.is_object() || !component.contains("type") ||
                        !component["type"].is_string() ||
                        (component.contains("data") && !component["data"].is_object()))
                    {
                        parsed.m_IsValid = false;
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
                        parsed.m_IsValid = false;
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
                            parsed.m_IsValid = false;
                            break;
                        }
                        SEditableVertex vertex;
                        vertex.m_Position = {
                            vertexJson[0].get<float>(),
                            vertexJson[1].get<float>(),
                            vertexJson[2].get<float>()
                        };
                        if (vertexJson.size() >= 5)
                        {
                            vertex.m_U = vertexJson[3].get<float>();
                            vertex.m_V = vertexJson[4].get<float>();
                        }
                        editableMesh.m_vVertices.push_back(vertex);
                    }
                    if (!parsed.m_IsValid)
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
                            parsed.m_IsValid = false;
                            break;
                        }
                        editableMesh.m_vTriangles.push_back({ triangleJson[0].get<int>(),
                                                              triangleJson[1].get<int>(),
                                                              triangleJson[2].get<int>() });
                    }
                    if (!parsed.m_IsValid)
                    {
                        break;
                    }

                    if (!editableMesh.m_vVertices.empty() || !editableMesh.m_vTriangles.empty())
                    {
                        parsed.m_vEditableMeshes.push_back({ entityIndex,
                                                             std::move(editableMesh) });
                    }
                }
                if (!parsed.m_IsValid)
                {
                    break;
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
            parsed.m_IsValid = false;
        }

        if (!parsed.m_IsValid)
        {
            parsed.m_vEditableMeshes.clear();
        }
        return parsed;
    }

    bool CSceneDocument::Deserialize(const SParsedSceneDocument& parsed, CScene& scene,
                                     const CComponentFactoryRegistry& factories)
    {
        if (!parsed.m_IsValid)
        {
            return false;
        }

        const nlohmann::json& j = parsed.m_Document;
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

    std::string CSceneDocument::SerializePrefab(const CScene& scene, int rootEntityIndex, int indent)
    {
        if (rootEntityIndex < 0 || rootEntityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return {};
        }

        std::unordered_set<int> visited;
        std::vector<int> vBranchIndices;
        CollectPrefabBranchIndices(scene, rootEntityIndex, visited, vBranchIndices);
        if (vBranchIndices.empty())
        {
            return {};
        }

        std::unordered_map<int, int> sceneToPrefabIndex;
        sceneToPrefabIndex.reserve(vBranchIndices.size());
        for (size_t i = 0; i < vBranchIndices.size(); ++i)
        {
            sceneToPrefabIndex[vBranchIndices[i]] = static_cast<int>(i);
        }

        nlohmann::json j;
        j["format"] = "quark-prefab";
        j["version"] = QUARK_ENGINE_VERSION;
        j["name"] = scene.m_vEntities[rootEntityIndex].m_Name;
        j["entities"] = nlohmann::json::array();

        for (size_t i = 0; i < vBranchIndices.size(); ++i)
        {
            const int sceneIndex = vBranchIndices[i];
            const CEntity& entity = scene.m_vEntities[sceneIndex];

            nlohmann::json ej;
            ej["name"] = entity.m_Name;
            ej["tags"] = entity.m_vTags;
            ej["is_group"] = entity.m_IsGroup;

            if (i == 0)
            {
                ej["parent_id"] = -1;
            }
            else
            {
                const auto parentIt = sceneToPrefabIndex.find(entity.m_ParentId);
                ej["parent_id"] = (parentIt != sceneToPrefabIndex.end()) ? parentIt->second : 0;
            }

            if (entity.m_pComponents)
            {
                entity.m_pComponents->Serialize(ej);
            }

            if (i == 0 && entity.m_ParentId >= 0 &&
                entity.m_ParentId < static_cast<int>(scene.m_vEntities.size()) &&
                ej.contains("components") && ej["components"].is_array())
            {
                CTransformComponent worldTransform;
                DecomposeWorld(ComposeWorld(scene, rootEntityIndex), worldTransform);
                for (auto& compJson : ej["components"])
                {
                    if (compJson.is_object() && compJson.value("type", "") == "Transform")
                    {
                        nlohmann::json transformData;
                        worldTransform.Serialize(transformData);
                        compJson["data"] = std::move(transformData);
                        break;
                    }
                }
            }

            j["entities"].push_back(std::move(ej));
        }

        return j.dump(indent);
    }

    SParsedSceneDocument CSceneDocument::ParsePrefab(const std::string& document)
    {
        SParsedSceneDocument parsed;
        const nlohmann::json raw = nlohmann::json::parse(document, nullptr, false);
        if (raw.is_discarded() || !raw.is_object())
        {
            return parsed;
        }

        if (raw.contains("entities"))
        {
            parsed = Parse(document);
        }
        else if (raw.contains("name") || raw.contains("components") ||
                 raw.contains("is_group") || raw.contains("tags"))
        {
            nlohmann::json singleEntity = raw;
            singleEntity["parent_id"] = -1;
            nlohmann::json wrapped;
            wrapped["entities"] = nlohmann::json::array({ std::move(singleEntity) });
            parsed = Parse(wrapped.dump());
        }
        else
        {
            return parsed;
        }

        if (!parsed.m_IsValid)
        {
            return parsed;
        }

        const auto& entities = parsed.m_Document["entities"];
        if (entities.empty())
        {
            parsed.m_IsValid = false;
            return parsed;
        }

        const int entityCount = static_cast<int>(entities.size());
        for (int i = 0; i < entityCount; ++i)
        {
            const int parentId = entities[i].value("parent_id", -1);
            if (parentId < -1 || parentId >= entityCount || parentId == i || (i == 0 && parentId != -1))
            {
                parsed.m_IsValid = false;
                parsed.m_vEditableMeshes.clear();
                return parsed;
            }

            int cursor = parentId;
            int steps = 0;
            while (cursor >= 0 && steps <= entityCount)
            {
                if (cursor == i)
                {
                    parsed.m_IsValid = false;
                    parsed.m_vEditableMeshes.clear();
                    return parsed;
                }
                cursor = entities[cursor].value("parent_id", -1);
                if (cursor < -1 || cursor >= entityCount)
                {
                    parsed.m_IsValid = false;
                    parsed.m_vEditableMeshes.clear();
                    return parsed;
                }
                ++steps;
            }
            if (steps > entityCount)
            {
                parsed.m_IsValid = false;
                parsed.m_vEditableMeshes.clear();
                return parsed;
            }
        }

        return parsed;
    }

    bool CSceneDocument::InstantiatePrefab(const SParsedSceneDocument& parsedPrefab, CScene& scene,
                                           const CComponentFactoryRegistry& factories, int parentEntityIndex,
                                           std::vector<int>* pvOutCreatedIndices)
    {
        if (pvOutCreatedIndices)
        {
            pvOutCreatedIndices->clear();
        }

        if (!parsedPrefab.m_IsValid || !parsedPrefab.m_Document.contains("entities") ||
            !parsedPrefab.m_Document["entities"].is_array() ||
            parsedPrefab.m_Document["entities"].empty())
        {
            return false;
        }

        const int baseIndex = static_cast<int>(scene.m_vEntities.size());
        if (parentEntityIndex < -1 || parentEntityIndex >= baseIndex)
        {
            return false;
        }

        const auto& entitiesJson = parsedPrefab.m_Document["entities"];
        scene.m_vEntities.reserve(scene.m_vEntities.size() + entitiesJson.size());

        std::vector<int> vCreatedIndices;
        vCreatedIndices.reserve(entitiesJson.size());

        for (size_t i = 0; i < entitiesJson.size(); ++i)
        {
            const auto& ej = entitiesJson[i];
            CEntity entity;
            const int newEntityIndex = static_cast<int>(scene.m_vEntities.size());
            entity.m_Id = newEntityIndex;

            std::string rawName = "Entity";
            if (ej.contains("name") && ej["name"].is_string())
            {
                rawName = ej["name"].get<std::string>();
                if (rawName.empty())
                {
                    rawName = "Entity";
                }
            }
            entity.m_Name = MakeUniqueEntityName(scene, rawName);

            if (ej.contains("tags") && ej["tags"].is_array())
            {
                entity.m_vTags = ej["tags"].get<std::vector<std::string>>();
            }
            if (ej.contains("is_group") && ej["is_group"].is_boolean())
            {
                entity.m_IsGroup = ej["is_group"].get<bool>();
            }

            const int prefabParentId = (i == 0) ? -1 : ej.value("parent_id", -1);
            entity.m_ParentId = (prefabParentId < 0)
                ? parentEntityIndex
                : (baseIndex + prefabParentId);

            if (ej.contains("components"))
            {
                entity.m_pComponents->Deserialize(ej, factories);
            }

            if (!entity.GetTransformComponent())
            {
                entity.GetComponents()->AddComponent(std::make_shared<CTransformComponent>());
            }
            if (!entity.m_IsGroup && !entity.GetLightComponent() && !entity.GetCameraComponent())
            {
                if (!entity.GetMeshComponent())
                {
                    entity.GetComponents()->AddComponent(std::make_shared<CMeshComponent>());
                }
                if (!entity.GetMaterialComponent())
                {
                    entity.GetComponents()->AddComponent(std::make_shared<CMaterialComponent>());
                }
            }

            scene.m_vEntities.push_back(std::move(entity));
            vCreatedIndices.push_back(newEntityIndex);
        }

        for (const auto& editableSnapshot : parsedPrefab.m_vEditableMeshes)
        {
            const size_t sceneIdx = static_cast<size_t>(baseIndex) + editableSnapshot.m_EntityIndex;
            if (sceneIdx < scene.m_vEntities.size())
            {
                if (CMeshComponent* pMesh = scene.m_vEntities[sceneIdx].GetMeshComponent())
                {
                    pMesh->m_EditableMesh = editableSnapshot.m_Mesh;
                }
            }
        }

        if (pvOutCreatedIndices)
        {
            *pvOutCreatedIndices = std::move(vCreatedIndices);
        }

        return true;
    }

    bool CSceneDocument::InstantiatePrefab(const std::string& document, CScene& scene,
                                           const CComponentFactoryRegistry& factories, int parentEntityIndex,
                                           std::vector<int>* pvOutCreatedIndices)
    {
        return InstantiatePrefab(
            ParsePrefab(document), scene, factories, parentEntityIndex, pvOutCreatedIndices);
    }

    SSceneSnapshot CSceneDocument::CaptureSnapshot(const CScene& scene)
    {
        SSceneSnapshot snapshot;
        snapshot.m_Document = Serialize(scene, 0);
        snapshot.m_Selected = scene.m_Selected;
        snapshot.m_vSelectedEntities = scene.m_vSelectedEntities;
        return snapshot;
    }

} // quark
