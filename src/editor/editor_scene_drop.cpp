#include "editor/editor_scene_drop.h"

#include "editor/editor.h"
#include "application_plugin_bridge.h"
#include "editor/editor_entity.h"
#include "editor/editor_scene_picker.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "engine/scene_document.h"
#include "engine/transform.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace
{

fs::path ResolvePrefabDropPath(const CEditor& editor, const std::string& assetName)
{
    std::error_code ec;
    const fs::path directPath(assetName);
    if (fs::is_regular_file(directPath, ec) && !ec)
    {
        return directPath;
    }

    if (!editor.m_CurrentAssetPath.empty())
    {
        const fs::path currentDirCandidate = editor.m_CurrentAssetPath / directPath;
        if (fs::is_regular_file(currentDirCandidate, ec) && !ec)
        {
            return currentDirCandidate;
        }
    }

    if (!editor.m_ProjectPath.empty())
    {
        const fs::path resourcesCandidate = fs::path(editor.m_ProjectPath) / "resources" / directPath;
        if (fs::is_regular_file(resourcesCandidate, ec) && !ec)
        {
            return resourcesCandidate;
        }
    }

    return directPath;
}

bool CanParsePrefabFile(const fs::path& prefabPath)
{
    std::ifstream file(prefabPath);
    if (!file.is_open())
    {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return quark::CSceneDocument::ParsePrefab(buffer.str()).IsValid;
}

} // anonymous

void CSceneAssetDrop::Handle(CEditor& editor, const Camera3D& camera)
{
    CViewportState& viewport = editor.m_Ui.m_Viewport;
    if (!editor.m_Ui.m_Layout.SceneAssetDragging)
    {
        return;
    }
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const std::string assetName = editor.m_Ui.m_Layout.DraggedSceneAssetName;
    editor.m_Ui.m_Layout.SceneAssetDragging = false;
    editor.m_Ui.m_Layout.DraggedSceneAssetName.clear();

    if (ImGuizmo::IsUsing())
    {
        return;
    }

    const ImVec2 mouse = { (float)GetMouseX(), (float)GetMouseY() };

    const bool mouseOverScene =
        mouse.x >= viewport.m_WindowPos.x && mouse.x <= viewport.m_WindowPos.x + viewport.m_WindowSize.x &&
        mouse.y >= viewport.m_WindowPos.y && mouse.y <= viewport.m_WindowPos.y + viewport.m_WindowSize.y;

    if (!mouseOverScene)
    {
        return;
    }

    const bool isMaterial =
        assetName.size() >= 4 &&
        assetName.substr(assetName.size() - 4) == ".mtl";

    if (isMaterial)
    {
        const Ray ray = GetScreenToWorldRay({ mouse.x, mouse.y }, camera);

        CEntity* pHitEntity = nullptr;
        float bestDistance = FLT_MAX;

        for (CEntity& entity : editor.m_Scene.m_vEntities)
        {
            float entityDistance = FLT_MAX;
            if (!CScenePicker::RaycastEntity(editor.m_Scene, entity, ray, entityDistance))
            {
                continue;
            }
            if (entityDistance < bestDistance)
            {
                bestDistance = entityDistance;
                pHitEntity = &entity;
            }
        }

        if (pHitEntity && pHitEntity->GetMaterialComponent())
        {
            editor.SaveState();
            LoadMaterialToEntity(pHitEntity, assetName);
            CEntityTextureService::MarkEntityUVDirty(pHitEntity);
        }

        return;
    }

    if (assetName.size() >= 7 && assetName.substr(assetName.size() - 7) == ".prefab")
    {
        const fs::path prefabPath = ResolvePrefabDropPath(editor, assetName);
        if (!CanParsePrefabFile(prefabPath))
        {
            return;
        }

        editor.SaveState();
        const std::vector<int> vCreatedIndices = CEntityFactory::InstantiatePrefab(
            editor.m_Scene, editor.m_Assets, prefabPath, editor.m_ComponentFactories, -1);
        if (vCreatedIndices.empty())
        {
            return;
        }

        const int rootIndex = vCreatedIndices.front();
        CEntity& rootEntity = editor.m_Scene.m_vEntities[rootIndex];
        CTransformComponent* pRootTransform = rootEntity.GetTransformComponent();
        const Vec3 dropPosition = GetSceneDropPosition(camera);

        if (pRootTransform)
        {
            if (rootEntity.m_IsGroup && vCreatedIndices.size() > 1 &&
                !pRootTransform->m_HasLocalMatrixOverride &&
                std::fabs(pRootTransform->m_Position.x) <= 0.0001f &&
                std::fabs(pRootTransform->m_Position.y) <= 0.0001f &&
                std::fabs(pRootTransform->m_Position.z) <= 0.0001f)
            {
                Vec3 childPivot = { 0.0f, 0.0f, 0.0f };
                int directChildCount = 0;
                for (size_t i = 1; i < vCreatedIndices.size(); ++i)
                {
                    const CEntity& child = editor.m_Scene.m_vEntities[vCreatedIndices[i]];
                    if (child.m_ParentId != rootIndex)
                    {
                        continue;
                    }
                    if (const CTransformComponent* pChildTransform = child.GetTransformComponent())
                    {
                        childPivot = childPivot + pChildTransform->m_Position;
                        ++directChildCount;
                    }
                }
                if (directChildCount > 0)
                {
                    childPivot = childPivot * (1.0f / static_cast<float>(directChildCount));
                    const Vec3 horizontalOffset = { childPivot.x, 0.0f, childPivot.z };
                    for (size_t i = 1; i < vCreatedIndices.size(); ++i)
                    {
                        CEntity& child = editor.m_Scene.m_vEntities[vCreatedIndices[i]];
                        if (child.m_ParentId != rootIndex)
                        {
                            continue;
                        }
                        if (CTransformComponent* pChildTransform = child.GetTransformComponent())
                        {
                            pChildTransform->m_Position = pChildTransform->m_Position - horizontalOffset;
                            if (pChildTransform->m_HasLocalMatrixOverride)
                            {
                                pChildTransform->m_LocalMatrixOverride.m[12] -= horizontalOffset.x;
                                pChildTransform->m_LocalMatrixOverride.m[14] -= horizontalOffset.z;
                            }
                        }
                    }
                }
            }

            pRootTransform->ClearLocalMatrixOverride();
            pRootTransform->m_Position = dropPosition;
        }

        editor.m_Scene.SelectEntity(rootIndex, false);
        for (int createdIndex : vCreatedIndices)
        {
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, createdIndex);
        }
        editor.m_SceneDirty = true;
        editor.RequestSceneRedraw();

        return;
    }

    CModelAsset* pAsset = editor.m_Assets.FindModelByName(assetName);
    if (!pAsset)
    {
        return;
    }

    CEntity entity = CEntityFactory::FromAsset(editor.m_Scene, *pAsset);
    CMeshComponent* pMesh = entity.GetMeshComponent();
    CTransformComponent* pTransform = entity.GetTransformComponent();

    if (!pMesh || !pTransform || !HasValidModelData(pMesh->m_Model))
    {
        return;
    }

    editor.SaveState();
    pTransform->ClearLocalMatrixOverride();
    pTransform->m_Position = GetSceneDropPosition(camera);

    editor.m_Scene.m_vEntities.push_back(std::move(entity));
    const int entityIndex = static_cast<int>(editor.m_Scene.m_vEntities.size()) - 1;
    editor.m_Scene.m_Selected = entityIndex;
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
}
