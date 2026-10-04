#include "QuarkCore/QuarkCore.hpp"

#include "application_plugin_bridge.h"

#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "project.h"

#include "imgui.h"

using namespace qc;

namespace
{
void AssignUiCallbacks(SPluginContext* pCtx)
{
    pCtx->pfnUiBegin        = [](const char* pTitle)
    {
        return ImGui::Begin(pTitle);
    };
    pCtx->pfnUiEnd          = []()
    {
        ImGui::End();
    };
    pCtx->pfnUiBeginMenu   = [](const char* pLabel)
    {
        return ImGui::BeginMenu(pLabel);
    };
    pCtx->pfnUiEndMenu     = []()
    {
        ImGui::EndMenu();
    };
    pCtx->pfnUiMenuItem    = [](const char* pLabel)
    {
        return ImGui::MenuItem(pLabel);
    };
    pCtx->pfnUiText         = [](const char* pTitle)
    {
        ImGui::Text("%s", pTitle);
    };
    pCtx->pfnUiButton       = [](const char* pLabel)
    {
        return ImGui::Button(pLabel);
    };
    pCtx->pfnUiCheckbox     = [](const char* pLabel, bool* pValue)
    {
        return ImGui::Checkbox(pLabel, pValue);
    };
    pCtx->pfnUiSliderFloat = [](const char* pLabel, float* pValue, float min, float max)
    {
        return ImGui::SliderFloat(pLabel, pValue, min, max);
    };
    pCtx->pfnUiInputFloat  = [](const char* pLabel, float* pValue)
    {
        return ImGui::InputFloat(pLabel, pValue);
    };
    pCtx->pfnUiColorEdit3  = [](const char* pLabel, float aColor[3])
    {
        return ImGui::ColorEdit3(pLabel, aColor);
    };
    pCtx->pfnUiSeparator    = []()
    {
        ImGui::Separator();
    };
    pCtx->pfnUiSameLine    = []()
    {
        ImGui::SameLine();
    };
}

bool RegisterComponentFactoryForPlugins(SPluginContext* pCtx, const char* pTypeName, FPluginComponentFactory create)
{
    if (!pCtx || !pCtx->pComponentRegistry || !pTypeName || !create)
    {
        return false;
    }
    return pCtx->pComponentRegistry->Register(pTypeName, [create]() -> std::shared_ptr<IComponent>
    {
        IComponent* pComponent = static_cast<IComponent*>(create());
        return pComponent ? std::shared_ptr<IComponent>(pComponent) : nullptr;
    });
}

void UnregisterComponentFactoryForPlugins(SPluginContext* pCtx, const char* pTypeName)
{
    if (pCtx && pCtx->pComponentRegistry && pTypeName)
    {
        pCtx->pComponentRegistry->Unregister(pTypeName);
    }
}

void RegisterUiCallbackForPlugins(SPluginContext* pCtx, EUIRegion region, FPluginUICallback pfnCallback)
{
    if (pCtx && pCtx->pPluginManager && pfnCallback)
    {
        pCtx->pPluginManager->RegisterUiCallback(region, pfnCallback);
    }
}

const char* EntityGetName(CScene* pScene, int index)
{
    return pScene->m_vEntities[index].m_Name.c_str();
}

void EntityGetPosition(CScene* pScene, int index, float* pX, float* pY, float* pZ)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        *pX = pTransform->m_Position.x; *pY = pTransform->m_Position.y; *pZ = pTransform->m_Position.z;
    }
}

void EntityGetRotation(CScene* pScene, int index, float* pX, float* pY, float* pZ)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        *pX = pTransform->m_Rotation.x; *pY = pTransform->m_Rotation.y; *pZ = pTransform->m_Rotation.z;
    }
}

void EntityGetScale(CScene* pScene, int index, float* pX, float* pY, float* pZ)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        *pX = pTransform->m_Scale.x; *pY = pTransform->m_Scale.y; *pZ = pTransform->m_Scale.z;
    }
}

void EntityGetColor(CScene* pScene, int index, unsigned char* pR, unsigned char* pG,
    unsigned char* pB, unsigned char* pA)
{
    if (auto* pMaterial = pScene->m_vEntities[index].GetMaterialComponent())
    {
        *pR = pMaterial->m_Color.r; *pG = pMaterial->m_Color.g;
        *pB = pMaterial->m_Color.b; *pA = pMaterial->m_Color.a;
    }
}

void EntitySetPosition(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent()) pTransform->m_Position = Vec3(x, y, z);
}

void EntitySetRotation(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent()) pTransform->m_Rotation = Vec3(x, y, z);
}

void EntitySetScale(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent()) pTransform->m_Scale = Vec3(x, y, z);
}

void EntitySetColor(CScene* pScene, int index, unsigned char r, unsigned char g,
    unsigned char b, unsigned char a)
{
    if (auto* pMaterial = pScene->m_vEntities[index].GetMaterialComponent()) pMaterial->m_Color = {r, g, b, a};
}

void EntitySetName(CScene* pScene, int index, const char* pName)
{
    pScene->m_vEntities[index].m_Name = pName;
}

void SceneSave(const char* pProjectPath, CScene* pScene)
{
    CProjectService::Save(pProjectPath, *pScene);
}

int SceneSpawn(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName)
{
    for (auto& a : pAssets->Models())
    {
        if (a.m_Name == pAssetName)
        {
            CEntity e = CEntityFactory::FromAsset(*pScene, a);
            const CMeshComponent* pMesh = e.GetMeshComponent();
            if (!pMesh || !pMesh->m_Model.meshCount)
            {
                return -1;
            }
            pScene->m_vEntities.push_back(std::move(e));
            return (int)pScene->m_vEntities.size() - 1;
        }
    }
    return -1;
}

void SceneDelete(CScene* pScene, int index)
{
    if (index < 0 || index >= (int)pScene->m_vEntities.size())
    {
        return;
    }
    pScene->m_vEntities.erase(pScene->m_vEntities.begin() + index);
    if (pScene->m_Selected >= (int)pScene->m_vEntities.size())
    {
        pScene->m_Selected = -1;
    }
}

void AssignEntityAndSceneCallbacks(SPluginContext* pCtx)
{
    pCtx->pfnEntityGetName     = EntityGetName;
    pCtx->pfnEntityGetPosition = EntityGetPosition;
    pCtx->pfnEntityGetRotation = EntityGetRotation;
    pCtx->pfnEntityGetScale    = EntityGetScale;
    pCtx->pfnEntityGetColor    = EntityGetColor;

    pCtx->pfnEntitySetPosition = EntitySetPosition;
    pCtx->pfnEntitySetRotation = EntitySetRotation;
    pCtx->pfnEntitySetScale    = EntitySetScale;
    pCtx->pfnEntitySetColor    = EntitySetColor;
    pCtx->pfnEntitySetName     = EntitySetName;

    pCtx->pfnSceneSave = SceneSave;
    pCtx->pfnSceneSpawn = SceneSpawn;
    pCtx->pfnSceneDelete = SceneDelete;
}

void AssignPluginHostCallbacks(SPluginContext* pCtx, CEditor& editor, CPluginManager& pluginManager)
{
    pCtx->pfnRegisterUICallback = RegisterUiCallbackForPlugins;
    pCtx->pfnRegisterComponentFactory = RegisterComponentFactoryForPlugins;
    pCtx->pfnUnregisterComponentFactory = UnregisterComponentFactoryForPlugins;
    pCtx->pComponentRegistry = &editor.m_ComponentFactories;
    pCtx->pPluginManager = &pluginManager;
}
}

void CPluginBridge::SyncContext(CEditor& editor, CPluginManager& pluginManager)
{
    if (!m_pContext)
    {
        return;
    }

    SPluginContext& ctx = *m_pContext;
    CScene& scene = editor.m_Scene;

    ctx.pScene       = &scene;
    ctx.entityCount  = (int)scene.m_vEntities.size();
    ctx.pSelected    = &scene.m_Selected;
    ctx.pAssets      = &editor.m_Assets;
    ctx.pProjectPath = editor.m_ProjectPath.c_str();

    AssignUiCallbacks(&ctx);
    AssignEntityAndSceneCallbacks(&ctx);
    AssignPluginHostCallbacks(&ctx, editor, pluginManager);
}

void CPluginBridge::Initialize(CEditor& editor, CPluginManager& pluginManager)
{
    m_pContext = std::make_unique<SPluginContext>();
    SyncContext(editor, pluginManager);
}

void CPluginBridge::Update(CEditor& editor, CPluginManager& pluginManager)
{
    if (!m_pContext)
    {
        return;
    }

    m_pContext->deltaTime = GetFrameTime();
    SyncContext(editor, pluginManager);

    pluginManager.UpdateAll(*m_pContext);
    pluginManager.DrawUiAll(*m_pContext);
}

void CPluginBridge::Reset()
{
    m_pContext.reset();
}