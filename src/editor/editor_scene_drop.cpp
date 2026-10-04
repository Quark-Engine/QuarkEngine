#include "editor/editor_scene_drop.h"

#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "editor/editor_scene_picker.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "engine/transform.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <cstring>
#include <string>

using namespace qc;

void CSceneAssetDrop::Handle(CEditor& editor, const qc::Camera3D& camera)
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
        const qc::Ray ray = GetScreenToWorldRay({ mouse.x, mouse.y }, camera);

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
        CEntity e = CEntityFactory::FromPrefab(editor.m_Scene, editor.m_Assets, assetName, editor.m_ComponentFactories);

        CMeshComponent* pMesh = e.GetMeshComponent();
        CTransformComponent* pTransform = e.GetTransformComponent();

        if (!pMesh || !pTransform || !HasValidModelData(pMesh->m_Model))
        {
            return;
        }

        editor.SaveState();
        pTransform->m_Position = GetSceneDropPosition(camera);

        editor.m_Scene.m_vEntities.push_back(std::move(e));
        editor.m_Scene.m_Selected = (int)editor.m_Scene.m_vEntities.size() - 1;

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
    pTransform->m_Position = GetSceneDropPosition(camera);

    editor.m_Scene.m_vEntities.push_back(std::move(entity));
    editor.m_Scene.m_Selected = (int)editor.m_Scene.m_vEntities.size() - 1;
}
