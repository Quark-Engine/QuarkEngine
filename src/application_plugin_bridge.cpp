#include "QuarkCore/QuarkCore.hpp"

#include "application_plugin_bridge.h"

#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "editor/editor_hierarchy_utils.h"
#include "editor/editor_viewers.h"
#include "camera.h"
#include "engine/transform.h"
#include "project.h"

#include "imgui.h"

#include <algorithm>

using namespace qc;

namespace
{
CEditor* s_pEditor = nullptr;
CFlyCamera* s_pCamera = nullptr;
SPluginContext* s_pPluginContext = nullptr;

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
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Position = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, index);
    }
}

void EntitySetRotation(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Rotation = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, index);
    }
}

void EntitySetScale(CScene* pScene, int index, float x, float y, float z)
{
    if (auto* pTransform = pScene->m_vEntities[index].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Scale = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, index);
    }
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

const char* EntityGetAssetName(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return nullptr;
    }
    const CMeshComponent* pMesh = pScene->m_vEntities[index].GetMeshComponent();
    if (!pMesh || pMesh->m_AssetName.empty())
    {
        return nullptr;
    }
    return pMesh->m_AssetName.c_str();
}

void RegisterEventCallbackForPlugins(SPluginContext* pCtx, EPluginEvent event,
                                     FPluginEventCallback pfnCallback)
{
    if (pCtx && pCtx->pPluginManager)
    {
        pCtx->pPluginManager->RegisterEventCallback(event, pfnCallback);
    }
}

void UnregisterEventCallbackForPlugins(SPluginContext* pCtx, EPluginEvent event,
                                       FPluginEventCallback pfnCallback)
{
    if (pCtx && pCtx->pPluginManager)
    {
        pCtx->pPluginManager->UnregisterEventCallback(event, pfnCallback);
    }
}

int EntityGetParent(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return -1;
    }
    return pScene->m_vEntities[index].m_ParentId;
}

bool EntitySetParent(CScene* pScene, int index, int parentIndex)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        parentIndex < -1 || parentIndex >= static_cast<int>(pScene->m_vEntities.size()) ||
        parentIndex == index)
    {
        return false;
    }

    const int previousParent = pScene->m_vEntities[index].m_ParentId;
    MoveEntityToParent(*pScene, index, parentIndex);
    return pScene->m_vEntities[index].m_ParentId != previousParent;
}

IComponent* FindEntityComponent(CScene* pScene, int index, const char* pTypeName)
{
    if (!pScene || !pTypeName || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return nullptr;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return nullptr;
    }

    for (const std::shared_ptr<IComponent>& pComponent : pComponents->GetAllComponents())
    {
        if (pComponent && pComponent->GetTypeName() == pTypeName)
        {
            return pComponent.get();
        }
    }
    return nullptr;
}

int EntityGetComponentCount(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return 0;
    }
    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    return pComponents ? static_cast<int>(pComponents->GetComponentCount()) : 0;
}

const char* EntityGetComponentType(CScene* pScene, int index, int componentIndex)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        componentIndex < 0)
    {
        return nullptr;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents || componentIndex >= static_cast<int>(pComponents->GetComponentCount()))
    {
        return nullptr;
    }

    const std::shared_ptr<IComponent> pComponent = pComponents->GetComponent(componentIndex);
    if (!pComponent)
    {
        return nullptr;
    }

    static thread_local std::string s_TypeName;
    s_TypeName = pComponent->GetTypeName();
    return s_TypeName.c_str();
}

bool EntityHasComponent(CScene* pScene, int index, const char* pTypeName)
{
    return FindEntityComponent(pScene, index, pTypeName) != nullptr;
}

bool EntityAddComponent(CScene* pScene, int index, const char* pTypeName)
{
    if (!pScene || !pScene->m_pComponentRegistry || !pTypeName ||
        FindEntityComponent(pScene, index, pTypeName))
    {
        return false;
    }

    if (index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    std::shared_ptr<IComponent> pComponent = pScene->m_pComponentRegistry->Create(pTypeName);
    if (!pComponent)
    {
        return false;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return false;
    }
    pComponents->AddComponent(std::move(pComponent));
    return true;
}

bool EntityRemoveComponent(CScene* pScene, int index, const char* pTypeName)
{
    if (!pScene || !pTypeName || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    CComponentManager* pComponents = pScene->m_vEntities[index].GetComponents();
    if (!pComponents)
    {
        return false;
    }

    for (size_t componentIndex = 0; componentIndex < pComponents->GetComponentCount(); ++componentIndex)
    {
        const std::shared_ptr<IComponent> pComponent = pComponents->GetComponent(componentIndex);
        if (pComponent && pComponent->GetTypeName() == pTypeName)
        {
            pComponents->RemoveComponent(componentIndex);
            return true;
        }
    }
    return false;
}

bool EntitySetComponentEnabled(CScene* pScene, int index, const char* pTypeName, bool enabled)
{
    IComponent* pComponent = FindEntityComponent(pScene, index, pTypeName);
    if (!pComponent)
    {
        return false;
    }
    pComponent->m_Enabled = enabled;
    return true;
}

int EntityGetTagCount(CScene* pScene, int index)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return 0;
    }
    return static_cast<int>(pScene->m_vEntities[index].m_vTags.size());
}

const char* EntityGetTag(CScene* pScene, int index, int tagIndex)
{
    if (!pScene || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        tagIndex < 0 || tagIndex >= static_cast<int>(pScene->m_vEntities[index].m_vTags.size()))
    {
        return nullptr;
    }
    return pScene->m_vEntities[index].m_vTags[tagIndex].c_str();
}

bool EntityHasTag(CScene* pScene, int index, const char* pTag)
{
    if (!pScene || !pTag || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    const auto& vTags = pScene->m_vEntities[index].m_vTags;
    return std::find(vTags.begin(), vTags.end(), pTag) != vTags.end();
}

bool EntityAddTag(CScene* pScene, int index, const char* pTag)
{
    if (!pScene || !pTag || pTag[0] == '\0' ||
        index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()) ||
        EntityHasTag(pScene, index, pTag))
    {
        return false;
    }

    pScene->m_vEntities[index].m_vTags.emplace_back(pTag);
    return true;
}

bool EntityRemoveTag(CScene* pScene, int index, const char* pTag)
{
    if (!pScene || !pTag || index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return false;
    }

    auto& vTags = pScene->m_vEntities[index].m_vTags;
    const auto found = std::find(vTags.begin(), vTags.end(), pTag);
    if (found == vTags.end())
    {
        return false;
    }

    vTags.erase(found);
    return true;
}

void SceneSave(const char* pProjectPath, CScene* pScene)
{
    if (!pProjectPath || !pScene)
    {
        return;
    }
    CProjectService::Save(pProjectPath, *pScene);
    DispatchPluginEvent(PLUGIN_EVENT_SCENE_SAVED);
}

int AssetGetCount(CAssetLibrary* pAssets)
{
    return pAssets ? static_cast<int>(pAssets->ModelCount()) : 0;
}

const char* AssetGetName(CAssetLibrary* pAssets, int index)
{
    if (!pAssets || index < 0 || index >= static_cast<int>(pAssets->ModelCount()))
    {
        return nullptr;
    }
    return pAssets->Models()[index].m_Name.c_str();
}

int AssetGetType(CAssetLibrary* pAssets, int index)
{
    if (!pAssets || index < 0 || index >= static_cast<int>(pAssets->ModelCount()))
    {
        return -1;
    }
    return static_cast<int>(pAssets->Models()[index].m_Type);
}

bool AssetExists(CAssetLibrary* pAssets, const char* pName)
{
    return pAssets && pName && pAssets->FindModelByName(pName) != nullptr;
}

int SceneSpawn(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName)
{
    if (!pAssets || !pScene || !pAssetName)
    {
        return -1;
    }

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
            const int entityIndex = static_cast<int>(pScene->m_vEntities.size()) - 1;
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
            return entityIndex;
        }
    }
    return -1;
}

int SceneSpawnEx(CAssetLibrary* pAssets, CScene* pScene, const char* pAssetName,
                float x, float y, float z)
{
    const int entityIndex = SceneSpawn(pAssets, pScene, pAssetName);
    if (entityIndex < 0)
    {
        return -1;
    }

    if (CTransformComponent* pTransform = pScene->m_vEntities[entityIndex].GetTransformComponent())
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Position = Vec3(x, y, z);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, entityIndex);
    }
    return entityIndex;
}

void SceneDelete(CScene* pScene, int index)
{
    if (index < 0 || index >= (int)pScene->m_vEntities.size())
    {
        return;
    }
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_DELETED, index);
    pScene->m_vEntities.erase(pScene->m_vEntities.begin() + index);
    if (pScene->m_Selected >= (int)pScene->m_vEntities.size())
    {
        pScene->m_Selected = -1;
    }
}

int SceneGetSelected(CScene* pScene)
{
    return pScene ? pScene->m_Selected : -1;
}

void SceneSetSelected(CScene* pScene, int index, bool additive)
{
    if (!pScene)
    {
        return;
    }
    if (index == -1 && !additive)
    {
        pScene->m_Selected = -1;
        pScene->m_vSelectedEntities.clear();
        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_SELECTED, -1);
        return;
    }
    if (index < 0 || index >= static_cast<int>(pScene->m_vEntities.size()))
    {
        return;
    }
    pScene->SelectEntity(index, additive);
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_SELECTED, index);
}

int SceneGetSelectionCount(CScene* pScene)
{
    return pScene ? static_cast<int>(pScene->m_vSelectedEntities.size()) : 0;
}

int SceneGetSelectedAt(CScene* pScene, int selectionIndex)
{
    if (!pScene || selectionIndex < 0 ||
        selectionIndex >= static_cast<int>(pScene->m_vSelectedEntities.size()))
    {
        return -1;
    }
    return pScene->m_vSelectedEntities[selectionIndex];
}

void SceneBeginCommand(CScene* pScene, const char* pDescription)
{
    if (s_pEditor && pScene == &s_pEditor->m_Scene)
    {
        s_pEditor->BeginPluginCommand(pDescription);
    }
}

void SceneEndCommand(CScene* pScene)
{
    if (s_pEditor && pScene == &s_pEditor->m_Scene)
    {
        s_pEditor->EndPluginCommand();
    }
}

bool SceneUndo()
{
    if (!s_pEditor || s_pEditor->m_UndoStack.empty())
    {
        return false;
    }
    s_pEditor->Undo();
    return true;
}

bool SceneRedo()
{
    if (!s_pEditor || s_pEditor->m_RedoStack.empty())
    {
        return false;
    }
    s_pEditor->Redo();
    return true;
}

bool SceneIsDirty(CScene* pScene)
{
    return s_pEditor && pScene == &s_pEditor->m_Scene && s_pEditor->m_SceneDirty;
}

void EditorFocusEntity(int index)
{
    if (!s_pEditor || !s_pCamera || index < 0 ||
        index >= static_cast<int>(s_pEditor->m_Scene.m_vEntities.size()))
    {
        return;
    }

    s_pEditor->m_Scene.SelectEntity(index, false);
    const qc::Mat4 world = quark::ComposeWorld(s_pEditor->m_Scene, index);
    const qc::Vec3 position = qc::Vec3(world * qc::Vec3{0.0f, 0.0f, 0.0f});
    s_pCamera->FocusOn(position);
}

void EditorSetStatusMessage(const char* pMessage)
{
    if (s_pEditor)
    {
        s_pEditor->SetStatusMessage(pMessage);
    }
}

void EditorRequestSceneRedraw()
{
    if (s_pEditor)
    {
        s_pEditor->RequestSceneRedraw();
    }
}

void EditorOpenAsset(const char* pAssetName)
{
    if (!s_pEditor || !pAssetName || pAssetName[0] == '\0')
    {
        return;
    }

    s_pEditor->m_SelectedAssetName = pAssetName;
    s_pEditor->m_SelectedAssetIndex = -1;
    if (CModelAsset* pAsset = s_pEditor->m_Assets.FindModelByName(pAssetName))
    {
        OpenModelViewerForAsset(s_pEditor->m_Ui.m_ModelViewer, *pAsset);
    }
}

void AssignEntityAndSceneCallbacks(SPluginContext* pCtx)
{
    pCtx->pfnEntityGetName     = EntityGetName;
    pCtx->pfnEntityGetPosition = EntityGetPosition;
    pCtx->pfnEntityGetRotation = EntityGetRotation;
    pCtx->pfnEntityGetScale    = EntityGetScale;
    pCtx->pfnEntityGetAssetName = EntityGetAssetName;
    pCtx->pfnEntityGetColor    = EntityGetColor;

    pCtx->pfnEntitySetPosition = EntitySetPosition;
    pCtx->pfnEntitySetRotation = EntitySetRotation;
    pCtx->pfnEntitySetScale    = EntitySetScale;
    pCtx->pfnEntitySetColor    = EntitySetColor;
    pCtx->pfnEntitySetName     = EntitySetName;
    pCtx->pfnEntityGetParent   = EntityGetParent;
    pCtx->pfnEntitySetParent   = EntitySetParent;
    pCtx->pfnEntityGetComponentCount = EntityGetComponentCount;
    pCtx->pfnEntityGetComponentType = EntityGetComponentType;
    pCtx->pfnEntityHasComponent = EntityHasComponent;
    pCtx->pfnEntityAddComponent = EntityAddComponent;
    pCtx->pfnEntityRemoveComponent = EntityRemoveComponent;
    pCtx->pfnEntitySetComponentEnabled = EntitySetComponentEnabled;
    pCtx->pfnEntityGetTagCount = EntityGetTagCount;
    pCtx->pfnEntityGetTag = EntityGetTag;
    pCtx->pfnEntityHasTag = EntityHasTag;
    pCtx->pfnEntityAddTag = EntityAddTag;
    pCtx->pfnEntityRemoveTag = EntityRemoveTag;

    pCtx->pfnSceneSave = SceneSave;
    pCtx->pfnAssetGetCount = AssetGetCount;
    pCtx->pfnAssetGetName = AssetGetName;
    pCtx->pfnAssetGetType = AssetGetType;
    pCtx->pfnAssetExists = AssetExists;
    pCtx->pfnSceneSpawn = SceneSpawn;
    pCtx->pfnSceneSpawnEx = SceneSpawnEx;
    pCtx->pfnSceneDelete = SceneDelete;
    pCtx->pfnSceneGetSelected = SceneGetSelected;
    pCtx->pfnSceneSetSelected = SceneSetSelected;
    pCtx->pfnSceneGetSelectionCount = SceneGetSelectionCount;
    pCtx->pfnSceneGetSelectedAt = SceneGetSelectedAt;
    pCtx->pfnSceneBeginCommand = SceneBeginCommand;
    pCtx->pfnSceneEndCommand = SceneEndCommand;
    pCtx->pfnSceneUndo = SceneUndo;
    pCtx->pfnSceneRedo = SceneRedo;
    pCtx->pfnSceneIsDirty = SceneIsDirty;
    pCtx->pfnEditorFocusEntity = EditorFocusEntity;
    pCtx->pfnEditorSetStatusMessage = EditorSetStatusMessage;
    pCtx->pfnEditorRequestSceneRedraw = EditorRequestSceneRedraw;
    pCtx->pfnEditorOpenAsset = EditorOpenAsset;
}

void AssignPluginHostCallbacks(SPluginContext* pCtx, CEditor& editor, CPluginManager& pluginManager)
{
    s_pEditor = &editor;
    pCtx->pfnRegisterUICallback = RegisterUiCallbackForPlugins;
    pCtx->pfnRegisterEventCallback = RegisterEventCallbackForPlugins;
    pCtx->pfnUnregisterEventCallback = UnregisterEventCallbackForPlugins;
    pCtx->pfnRegisterComponentFactory = RegisterComponentFactoryForPlugins;
    pCtx->pfnUnregisterComponentFactory = UnregisterComponentFactoryForPlugins;
    editor.m_Scene.m_pComponentRegistry = &editor.m_ComponentFactories;
    pCtx->pComponentRegistry = &editor.m_ComponentFactories;
    pCtx->pPluginManager = &pluginManager;
}
}

void DispatchPluginEvent(EPluginEvent event, int entityIndex)
{
    if (s_pEditor && s_pEditor->m_pPluginManager && s_pPluginContext)
    {
        s_pEditor->m_pPluginManager->DispatchEvent(event, *s_pPluginContext, entityIndex);
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
    s_pPluginContext = m_pContext.get();
    SyncContext(editor, pluginManager);
}

void CPluginBridge::SetCamera(CFlyCamera& camera)
{
    m_pCamera = &camera;
    s_pCamera = &camera;
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
    s_pCamera = nullptr;
    s_pEditor = nullptr;
    s_pPluginContext = nullptr;
    m_pCamera = nullptr;
    m_pContext.reset();
}
