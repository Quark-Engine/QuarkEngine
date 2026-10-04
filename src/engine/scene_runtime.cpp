#include "engine/scene_runtime.h"

#include "engine/material_texture_restore.h"
#include "models.h"
#include "tex.h"
#include "editor/editor_viewers.h"

using namespace qc;

namespace quark
{

void CSceneRuntime::RestoreEntityMaterial(CEntity& entity, const CAssetLibrary& assets)
{
    CMaterialComponent* pMaterial = entity.GetMaterialComponent();
    if (!pMaterial)
    {
        return;
    }

    auto applyTexture = [&]()
    {
        CMeshComponent* pMesh = entity.GetMeshComponent();
        if (!pMesh || !pMaterial->m_Texture.id)
        {
            return;
        }
        for (int index = 0; index < pMesh->m_Model.materialCount; ++index)
        {
            if (pMesh->m_Model.materials[index].maps)
            {
                pMesh->m_Model.materials[index].maps[MATERIAL_MAP_ALBEDO].texture = pMaterial->m_Texture;
            }
        }
    };

    const SMaterialTextureRestore restore = PlanMaterialTextureRestore(*pMaterial, assets.Textures());
    switch (restore.Action)
    {
    case EMaterialTextureRestore::DirectTexture:
        pMaterial->m_Texture = restore.DirectTexture;
        pMaterial->m_TextureSource = TEXTURE_EXTERNAL;
        CEntityTextureService::MarkEntityUVDirty(&entity);
        applyTexture();
        return;

    case EMaterialTextureRestore::MaterialFile:
        LoadMaterialToEntity(&entity, pMaterial->m_TextureName, -1);
        pMaterial->m_TextureSource = TEXTURE_EXTERNAL;
        CEntityTextureService::MarkEntityUVDirty(&entity);
        applyTexture();
        return;

    case EMaterialTextureRestore::ModelTextures:
        CEntityTextureService::RestoreModelTextures(&entity);
        break;

    case EMaterialTextureRestore::ClearTextures:
        pMaterial->m_Texture = {0};
        CEntityTextureService::ClearMaterialTextures(&entity);
        break;

    case EMaterialTextureRestore::None:
        break;
    }

    CEntityTextureService::MarkEntityUVDirty(&entity);
    applyTexture();
}

void CSceneRuntime::RestoreSceneEntityModels(CScene& scene, CAssetLibrary& assets)
{
    for (auto& entity : scene.m_vEntities)
    {
        CMeshComponent* pMesh = entity.GetMeshComponent();
        if (!pMesh)
        {
            continue;
        }

        pMesh->ReleaseOwnedResources();
        pMesh->m_BoundsDirty = true;

        pMesh->m_pAsset = pMesh->m_AssetName.empty() ? nullptr : assets.FindModelByName(pMesh->m_AssetName);
        if (!pMesh->m_pAsset)
        {
            continue;
        }

        if ((pMesh->m_IsEditableMesh || pMesh->m_VertexGizmo) && !pMesh->m_EditableMesh.m_vVertices.empty())
        {
            pMesh->m_Model = {};
            RebuildMeshFromEditable(pMesh->m_Model, pMesh->m_EditableMesh);
            pMesh->m_OwnsModelInstance = true;
            CEntityTextureService::StoreUV(&entity);
            CEntityTextureService::StoreMaterialTextures(&entity);
            CMeshOverrideService::Apply(entity);
            RestoreEntityMaterial(entity, assets);
        }
        else if (pMesh->m_pAsset->m_IsProcedural)
        {
            pMesh->m_Model = pMesh->m_pAsset->pfnGenerator(pMesh->m_Segments);
            pMesh->m_OwnsModelInstance = true;
            CEntityTextureService::StoreUV(&entity);
            CEntityTextureService::StoreMaterialTextures(&entity);
            CMeshOverrideService::Apply(entity);
            RestoreEntityMaterial(entity, assets);
        }
        else
        {
            if (!CModelService::LoadInstance(*pMesh->m_pAsset, pMesh->m_Model))
            {
                pMesh->m_pAsset = nullptr;
                pMesh->m_AssetName.clear();
                pMesh->m_Model = {};
                pMesh->m_OwnsModelInstance = false;
                continue;
            }

            pMesh->m_OwnsModelInstance = true;
            CEntityTextureService::StoreUV(&entity);
            CEntityTextureService::StoreMaterialTextures(&entity);
            CMeshOverrideService::Apply(entity);
            RestoreEntityMaterial(entity, assets);
        }

        pMesh->m_ShaderAssigned = false;
    }
}

void CSceneRuntime::ResetSceneLightRuntime(CScene& scene, CLightRegistry& lights)
{
    for (auto& entity : scene.m_vEntities)
    {
        CLightComponent* pLight = entity.GetLightComponent();
        if (!pLight)
        {
            continue;
        }

        if (pLight->m_Created && pLight->m_Light.m_Id >= 0)
        {
            lights.Free(pLight->m_Light.m_Id);
        }

        pLight->m_Created = false;
        pLight->m_Light.m_Id = -1;
        pLight->m_Light.m_Light.enabledLoc = -1;
        pLight->m_Light.m_Light.typeLoc = -1;
        pLight->m_Light.m_Light.positionLoc = -1;
        pLight->m_Light.m_Light.targetLoc = -1;
        pLight->m_Light.m_Light.colorLoc = -1;
        pLight->m_Light.m_Light.attenuationLoc = -1;
        pLight->m_Light.m_SpotAngleLoc = -1;
        pLight->m_Light.m_IntensityLoc = -1;
        pLight->m_Light.m_RangeLoc = -1;
    }
    lights.Reset();
}

} // quark
