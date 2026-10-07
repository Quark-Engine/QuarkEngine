#include "editor/editor_entity.h"
#include "editor/editor_viewers.h"
#include "models.h"
#include "nlohmann/json.hpp"

#include <fstream>
#include <memory>
void CEntityFactory::AssignName(CEntity& entity, const char* pNewName)
{
    if (!pNewName || pNewName[0] == '\0')
    {
        return;
    }
    entity.m_Name = pNewName;
}

CEntity CEntityFactory::FromAsset(CScene& scene, CModelAsset& asset)
{
    CEntity entity;
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return entity;
    }

    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    pMesh->m_Type = asset.m_Type;
    pMesh->m_pAsset = &asset;
    pMesh->m_AssetName = asset.m_Name;
    pMesh->m_Segments = 16;
    const std::string baseName = asset.m_IsProcedural ? asset.m_Name : fs::path(asset.m_Name).stem().string();
    entity.m_Name = scene.MakeUniqueName(baseName.empty() ? "Model" : baseName);

    auto pMatComp = std::make_shared<CMaterialComponent>();
    entity.GetComponents()->AddComponent(pMatComp);
    CMaterialComponent* pMat = pMatComp.get();

    if (asset.m_IsProcedural)
    {
        pMesh->m_Model = asset.pfnGenerator(pMesh->m_Segments);
        pMesh->m_OwnsModelInstance = true;
        CMeshOverrideService::Clear(entity);
        CEntityTextureService::StoreUV(&entity);
        CEntityTextureService::StoreMaterialTextures(&entity);
        pMat->m_TextureSource = TEXTURE_NONE;
        pMat->m_TextureName.clear();

        if (asset.m_Name == "Text")
        {
            auto pTextComp = std::make_shared<CText3DComponent>();
            entity.GetComponents()->AddComponent(pTextComp);
        }
    }
    else
    {
        if (!CModelService::LoadInstance(asset, pMesh->m_Model))
        {
            pMesh->m_pAsset = nullptr;
            pMesh->m_AssetName.clear();
            pMesh->m_Model;
            return entity;
        }

        pMesh->m_OwnsModelInstance = true;
        CMeshOverrideService::Clear(entity);
        CEntityTextureService::StoreUV(&entity);
        CEntityTextureService::StoreMaterialTextures(&entity);

        bool hasEmbedded = false;
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0)
            {
                hasEmbedded = true;
                break;
            }
        }

        pMat->m_TextureSource = hasEmbedded ? TEXTURE_MODEL : TEXTURE_NONE;

        if (!asset.m_FilePath.empty())
        {
            std::filesystem::path modelPath(asset.m_FilePath);
            std::filesystem::path mtlPath = modelPath.parent_path() / (modelPath.stem().string() + ".mtl");

            if (std::filesystem::exists(mtlPath))
            {
                LoadMaterialToEntity(&entity, mtlPath);
                return entity;
            }
        }
    }

    pMat->m_Texture = {0};
    return entity;
}

CEntity CEntityFactory::Light(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Light");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    for (size_t i = 0; i < pCm->GetComponentCount(); ++i)
    {
        if (pCm->GetComponent(i)->GetType() == COMPONENT_MESH)
        {
            pCm->RemoveComponent(i);
            break;
        }
    }

    auto pLight = std::make_shared<CLightComponent>();
    pLight->m_Light = CreateLighting({0, 0, 0}, WHITE);
    pCm->AddComponent(pLight);

    return entity;
}

void CEntityFactory::SavePrefab(CEntity entity, const fs::path path)
{
    nlohmann::json j;

    j["name"] = entity.m_Name;
    j["tags"] = entity.m_vTags;
    j["is_group"] = entity.m_IsGroup;
    j["parent_id"] = entity.m_ParentId;

    if (entity.m_pComponents)
    {
        entity.m_pComponents->Serialize(j);
    }

    std::ofstream f(path / (entity.m_Name + ".prefab"));

    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open %s.prefab ", entity.m_Name.c_str()));
        return;
    }

    f << j.dump(4);
    f.close();
}

CEntity CEntityFactory::FromPrefab(CScene& scene, const CAssetLibrary& assets, const fs::path filename,
                                   const CComponentFactoryRegistry& factories)
{
    std::ifstream f(filename);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open prefab %s", filename.string().c_str()));
        return {};
    }

    nlohmann::json j;
    f >> j;

    CEntity entity;

    entity.m_Name = j.value("name", "Entity");
    if (j.contains("tags") && j["tags"].is_array())
    {
        entity.m_vTags = j["tags"].get<std::vector<std::string>>();
    }
    entity.m_IsGroup = j.value("is_group", false);
    entity.m_ParentId = j.value("parent_id", -1);
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());

    if (j.contains("components"))
    {
        entity.m_pComponents->Deserialize(j, factories);
    }

    auto pMesh = entity.GetMeshComponent();
    if (pMesh && !pMesh->m_AssetName.empty())
    {
        CModelService::LoadInstance(*assets.FindModelByName(pMesh->m_AssetName), pMesh->m_Model);
    }

    auto pMat = entity.GetMaterialComponent();
    if (pMat && !pMat->m_TextureName.empty())
    {
        LoadMaterialToEntity(&entity, pMat->m_TextureName);
    }

    TraceLog(LogLevel::Info, "PREFAB", TextFormat("[TEXTURE_NAME] %s", pMat->m_TextureName.c_str()));

    return entity;
}
