#include "editor/editor_gizmo.h"

#include "editor/editor.h"
#include "application_plugin_bridge.h"
#include "editor/editor_mesh_edit.h"
#include "editor/editor_state.h"
#include "editor/editor_utils.h"
#include "camera.h"
#include "engine/transform.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <cstring>
void CGizmoController::Draw(CEditor& editor, CFlyCamera& camera)
{
    SGizmoState& gizmo = editor.m_Ui.m_Gizmo;
    CViewportState& viewport = editor.m_Ui.m_Viewport;
    SVertexEditState& edit = editor.m_Ui.m_VertexEdit;

    CMeshEditor::SyncState(editor);

    if (camera.m_Active)
    {
        return;
    }

    if (viewport.m_WindowSize.x <= 0 || viewport.m_WindowSize.y <= 0)
    {
        return;
    }

    if (editor.m_Scene2DMode && !editor.m_IsPlaying && viewport.m_Hovered &&
        !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const float zoom2D = std::max(0.01f, editor.m_Scene2DZoom);
        const float targetX = viewport.m_WindowSize.x * 0.5f + editor.m_Scene2DPan.x;
        const float targetY = viewport.m_WindowSize.y * 0.5f + editor.m_Scene2DPan.y;
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const float localMouseX = mousePos.x - viewport.m_WindowPos.x;
        const float localMouseY = mousePos.y - viewport.m_WindowPos.y;
        const float worldMouseX = (localMouseX - viewport.m_WindowSize.x * 0.5f) / zoom2D + targetX;
        const float worldMouseY = (localMouseY - viewport.m_WindowSize.y * 0.5f) / zoom2D + targetY;

        for (int entityIndex = static_cast<int>(editor.m_Scene.m_vEntities.size()) - 1;
             entityIndex >= 0;
             --entityIndex)
        {
            CEntity& candidate = editor.m_Scene.m_vEntities[entityIndex];
            CComponentManager* pCandidateComponents = candidate.GetComponents();
            auto pCandidateRect = pCandidateComponents
                ? pCandidateComponents->GetComponentOfType<CUiRectTransformComponent>()
                : nullptr;
            if (!pCandidateRect || !pCandidateRect->m_Enabled)
            {
                continue;
            }
            const float minX = pCandidateRect->m_Position.x;
            const float minY = pCandidateRect->m_Position.y;
            const float maxX = minX + std::max(1.0f, pCandidateRect->m_Size.x);
            const float maxY = minY + std::max(1.0f, pCandidateRect->m_Size.y);
            if (worldMouseX >= minX && worldMouseX <= maxX &&
                worldMouseY >= minY && worldMouseY <= maxY)
            {
                const bool additive = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                editor.m_Scene.SelectEntity(entityIndex, additive);
                break;
            }
        }
    }

    CEntity* pEntity = editor.m_Scene.GetSelected();
    if (!pEntity)
    {
        return;
    }
    CComponentManager* pComponents = pEntity->GetComponents();
    auto pRectTransform = pComponents ? pComponents->GetComponentOfType<CUiRectTransformComponent>() : nullptr;
    CTransformComponent* pTransform = pEntity->GetTransformComponent();
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    if (!pTransform && !pRectTransform)
    {
        return;
    }
    if (pMesh && pMesh->m_VertexGizmo)
    {
        return;
    }

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewport.m_WindowPos.x, viewport.m_WindowPos.y, viewport.m_WindowSize.x, viewport.m_WindowSize.y);
    ImGuizmo::SetOrthographic(editor.m_Scene2DMode || (pRectTransform != nullptr && pTransform == nullptr));
    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::Enable(viewport.m_Hovered || ImGuizmo::IsUsing());

    float aTranslationSnap[3] = {
        editor.m_Preferences.m_GizmoTranslationSnap,
        editor.m_Preferences.m_GizmoTranslationSnap,
        editor.m_Preferences.m_GizmoTranslationSnap
    };
    float aRotationSnap[3] = {
        editor.m_Preferences.m_GizmoRotationSnap,
        editor.m_Preferences.m_GizmoRotationSnap,
        editor.m_Preferences.m_GizmoRotationSnap
    };
    float aScaleSnap[3] = {
        editor.m_Preferences.m_GizmoScaleSnap,
        editor.m_Preferences.m_GizmoScaleSnap,
        editor.m_Preferences.m_GizmoScaleSnap
    };
    const bool gizmoSnapEnabled = editor.m_Preferences.m_GizmoSnapEnabled ||
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (pRectTransform && (editor.m_Scene2DMode || !pTransform))
    {
        const float zoom2D = editor.m_Scene2DMode ? std::max(0.01f, editor.m_Scene2DZoom) : 1.0f;
        const Vec2 pan2D = editor.m_Scene2DMode ? editor.m_Scene2DPan : Vec2{ 0.0f, 0.0f };
        const float targetX = viewport.m_WindowSize.x * 0.5f + pan2D.x;
        const float targetY = viewport.m_WindowSize.y * 0.5f + pan2D.y;
        const float halfWidth = (viewport.m_WindowSize.x / zoom2D) * 0.5f;
        const float halfHeight = (viewport.m_WindowSize.y / zoom2D) * 0.5f;

        const Mat4 uiView = Mat4::lookAt(
            { targetX, -targetY, 100.0f },
            { targetX, -targetY, 0.0f },
            { 0.0f, 1.0f, 0.0f });
        const Mat4 uiProjection = Mat4::ortho(
            -halfWidth,
            halfWidth,
            -halfHeight,
            halfHeight,
            0.1f,
            1000.0f);

        const Vec2 oldSize = {
            std::max(1.0f, pRectTransform->m_Size.x),
            std::max(1.0f, pRectTransform->m_Size.y)
        };
        const Vec2 oldCenter = {
            pRectTransform->m_Position.x + oldSize.x * 0.5f,
            pRectTransform->m_Position.y + oldSize.y * 0.5f
        };

        if (editor.m_Preferences.m_ShowSelectionVisualization)
        {
            const ImVec2 rectMin(
                viewport.m_WindowPos.x + viewport.m_WindowSize.x * 0.5f + (pRectTransform->m_Position.x - targetX) * zoom2D,
                viewport.m_WindowPos.y + viewport.m_WindowSize.y * 0.5f + (pRectTransform->m_Position.y - targetY) * zoom2D);
            const ImVec2 rectMax(
                rectMin.x + oldSize.x * zoom2D,
                rectMin.y + oldSize.y * zoom2D);
            ImGui::GetWindowDrawList()->AddRect(
                rectMin,
                rectMax,
                IM_COL32(
                    editor.m_Preferences.m_SelectionRed,
                    editor.m_Preferences.m_SelectionGreen,
                    editor.m_Preferences.m_SelectionBlue,
                    255),
                0.0f,
                0,
                1.5f);
        }

        float aUiViewMat4[16] = {};
        float aUiProjectionMat4[16] = {};
        float aUiTransformMat4[16] = {};
        memcpy(aUiViewMat4, &uiView, sizeof(aUiViewMat4));
        memcpy(aUiProjectionMat4, &uiProjection, sizeof(aUiProjectionMat4));

        float aUiTranslation[3] = { oldCenter.x, -oldCenter.y, 0.0f };
        float aUiRotation[3] = { 0.0f, 0.0f, 0.0f };
        float aUiScale[3] = { oldSize.x, oldSize.y, 1.0f };
        ImGuizmo::RecomposeMatrixFromComponents(
            aUiTranslation,
            aUiRotation,
            aUiScale,
            aUiTransformMat4);

        const ImGuizmo::OPERATION uiOperation =
            (editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::SCALE)
                ? ImGuizmo::SCALE
                : ImGuizmo::TRANSLATE;

        ImGuizmo::Manipulate(
            aUiViewMat4,
            aUiProjectionMat4,
            uiOperation,
            ImGuizmo::LOCAL,
            aUiTransformMat4,
            nullptr,
            gizmoSnapEnabled
                ? (uiOperation == ImGuizmo::SCALE ? aScaleSnap : aTranslationSnap)
                : nullptr);

        if (ImGuizmo::IsUsing() && !gizmo.m_WasUsingTransform)
        {
            editor.SaveState();
        }

        if (ImGuizmo::IsUsing())
        {
            float aNextTranslation[3] = {};
            float aNextRotation[3] = {};
            float aNextScale[3] = {};
            ImGuizmo::DecomposeMatrixToComponents(
                aUiTransformMat4,
                aNextTranslation,
                aNextRotation,
                aNextScale);

            const Vec2 newSize = {
                std::max(1.0f, std::fabs(aNextScale[0])),
                std::max(1.0f, std::fabs(aNextScale[1]))
            };
            const Vec2 newCenter = {
                aNextTranslation[0],
                -aNextTranslation[1]
            };
            const Vec2 newPos = {
                newCenter.x - newSize.x * 0.5f,
                newCenter.y - newSize.y * 0.5f
            };
            const Vec2 posDelta = {
                newPos.x - pRectTransform->m_Position.x,
                newPos.y - pRectTransform->m_Position.y
            };

            pRectTransform->m_Position = newPos;
            pRectTransform->m_Size = newSize;

            if (CUiComponent* pUi = pEntity->GetUiComponent())
            {
                auto pText = pComponents->GetComponentOfType<CTextComponent>();
                pUi->SyncToRuntimeDesc(pRectTransform.get(), pText.get());
            }

            if (editor.m_Scene.m_vSelectedEntities.size() > 1)
            {
                for (int selectedIndex : editor.m_Scene.m_vSelectedEntities)
                {
                    if (selectedIndex == editor.m_Scene.m_Selected ||
                        selectedIndex < 0 || selectedIndex >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
                    {
                        continue;
                    }
                    CEntity& selectedEntity = editor.m_Scene.m_vEntities[selectedIndex];
                    CComponentManager* pSelectedComponents = selectedEntity.GetComponents();
                    auto pSelectedRect = pSelectedComponents
                        ? pSelectedComponents->GetComponentOfType<CUiRectTransformComponent>()
                        : nullptr;
                    if (!pSelectedRect)
                    {
                        continue;
                    }
                    pSelectedRect->m_Position.x += posDelta.x;
                    pSelectedRect->m_Position.y += posDelta.y;
                    if (CUiComponent* pSelectedUi = selectedEntity.GetUiComponent())
                    {
                        auto pSelectedText = pSelectedComponents->GetComponentOfType<CTextComponent>();
                        pSelectedUi->SyncToRuntimeDesc(pSelectedRect.get(), pSelectedText.get());
                    }
                }
            }

            editor.m_SceneDirty = true;
            DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED,
                                static_cast<int>(pEntity - editor.m_Scene.m_vEntities.data()));
        }

        gizmo.m_WasUsingTransform = ImGuizmo::IsUsing();
        gizmo.m_WasUsingMeshEdit = false;
        return;
    }

    if (!pTransform)
    {
        return;
    }

    const Mat4 view = Mat4::lookAt(
        camera.GetCamera().position,
        camera.GetCamera().target,
        camera.GetCamera().up);

    const Mat4 projection = editor.m_Scene2DMode
        ? Mat4::ortho(
              -camera.GetCamera().fovy * viewport.m_WindowSize.x / viewport.m_WindowSize.y * 0.5f,
              camera.GetCamera().fovy * viewport.m_WindowSize.x / viewport.m_WindowSize.y * 0.5f,
              -camera.GetCamera().fovy * 0.5f,
              camera.GetCamera().fovy * 0.5f,
              0.1f,
              1000.0f)
        : Mat4::perspective(
              camera.GetCamera().fovy * DEG2RAD,
              viewport.m_WindowSize.x / viewport.m_WindowSize.y,
              0.1f,
              1000.0f);

    float aViewMat4[16] = {};
    float aProjectionMat4[16] = {};
    float aTransformMat4[16] = {};

    memcpy(aViewMat4, &view, sizeof(aViewMat4));
    memcpy(aProjectionMat4, &projection, sizeof(aProjectionMat4));

    CMeshEditor::DrawOverlay(editor, camera.GetCamera());

    if (pMesh && edit.m_Enabled && HasValidModelData(pMesh->m_Model))
    {
        if (edit.m_MeshIndex >= pMesh->m_Model.meshCount)
        {
            edit.m_MeshIndex = 0;
        }

        int vertexIndex = -1;
        if (CMeshEditor::GetSelectedVertexIndex(
                *pEntity,
                edit.m_MeshIndex,
                edit.m_TriangleIndex,
                edit.m_VertexCorner,
                vertexIndex))
        {
            const Vec3 vertexWorld = CMeshEditor::GetVertexWorldPosition(
                editor.m_Scene,
                *pEntity,
                edit.m_MeshIndex,
                vertexIndex);

            float aVertexTranslation[3] = { vertexWorld.x, vertexWorld.y, vertexWorld.z };
            float aVertexRotation[3] = { 0.0f, 0.0f, 0.0f };
            float aVertexScale[3] = { 1.0f, 1.0f, 1.0f };

            ImGuizmo::RecomposeMatrixFromComponents(
                aVertexTranslation,
                aVertexRotation,
                aVertexScale,
                aTransformMat4);

            ImGuizmo::Manipulate(
                aViewMat4,
                aProjectionMat4,
                ImGuizmo::TRANSLATE,
                ImGuizmo::WORLD,
                aTransformMat4,
                nullptr,
                gizmoSnapEnabled ? aTranslationSnap : nullptr);

            if (ImGuizmo::IsUsing() && !gizmo.m_WasUsingMeshEdit)
            {
                editor.SaveState();
            }

            if (ImGuizmo::IsUsing())
            {
                float aNextTranslation[3] = {};
                float aNextRotation[3] = {};
                float aNextScale[3] = {};
                ImGuizmo::DecomposeMatrixToComponents(
                    aTransformMat4,
                    aNextTranslation,
                    aNextRotation,
                    aNextScale);

                CMeshEditor::SetVertexWorldPosition(
                    editor.m_Scene,
                    *pEntity,
                    edit.m_MeshIndex,
                    vertexIndex,
                    { aNextTranslation[0], aNextTranslation[1], aNextTranslation[2] });
            }

            gizmo.m_WasUsingMeshEdit = ImGuizmo::IsUsing();
            return;
        }
    }

    float aTranslation[3] = { pTransform->m_Position.x, pTransform->m_Position.y, pTransform->m_Position.z };

    const Mat4 gizmoTransform = quark::ComposeWorld(editor.m_Scene, *pEntity);
    memcpy(aTransformMat4, &gizmoTransform, sizeof(aTransformMat4));

    ImGuizmo::Manipulate(
        aViewMat4,
        aProjectionMat4,
        editor.m_Ui.m_MeshEdit.m_GizmoMode,
        ImGuizmo::WORLD,
        aTransformMat4,
        nullptr,
        gizmoSnapEnabled
            ? (editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::TRANSLATE ? aTranslationSnap : editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::ROTATE ? aRotationSnap
                                                                                                                                                     : aScaleSnap)
            : nullptr);

    if (ImGuizmo::IsUsing() && !gizmo.m_WasUsingTransform)
    {
        editor.SaveState();
    }

    if (ImGuizmo::IsUsing())
    {
        Mat4 worldTransform = Mat4::identity();
        memcpy(&worldTransform, aTransformMat4, sizeof(aTransformMat4));
        quark::DecomposeLocal(
            quark::ParentWorld(editor.m_Scene, *pEntity),
            worldTransform,
            *pTransform);
        Mat4 inverseParent;
        if (quark::TryInvertAffine(quark::ParentWorld(editor.m_Scene, *pEntity), inverseParent))
        {
            pTransform->SetLocalMatrixOverride(inverseParent * worldTransform);
        }

        const Vec3 positionDelta = pTransform->m_Position - Vec3{ aTranslation[0], aTranslation[1], aTranslation[2] };
        if (editor.m_Scene.m_vSelectedEntities.size() > 1)
        {
            for (int selectedIndex : editor.m_Scene.m_vSelectedEntities)
            {
                if (selectedIndex == editor.m_Scene.m_Selected ||
                    selectedIndex < 0 || selectedIndex >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
                {
                    continue;
                }
                CEntity& selectedEntity = editor.m_Scene.m_vEntities[selectedIndex];
                CTransformComponent* pSelectedTransform = selectedEntity.GetTransformComponent();
                if (!pSelectedTransform)
                {
                    continue;
                }
                pSelectedTransform->m_Position = pSelectedTransform->m_Position + positionDelta;
                if (pSelectedTransform->m_HasLocalMatrixOverride)
                {
                    pSelectedTransform->m_LocalMatrixOverride.m[12] += positionDelta.x;
                    pSelectedTransform->m_LocalMatrixOverride.m[13] += positionDelta.y;
                    pSelectedTransform->m_LocalMatrixOverride.m[14] += positionDelta.z;
                }
                DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, selectedIndex);
                CEntityTextureService::MarkEntityBoundsDirty(&selectedEntity);
                if (CMaterialComponent* pSelectedMaterial = selectedEntity.GetMaterialComponent();
                    pSelectedMaterial && !pSelectedMaterial->m_TextureStretch)
                {
                    CEntityTextureService::MarkEntityUVDirty(&selectedEntity);
                }
            }
        }

        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED,
                            static_cast<int>(pEntity - editor.m_Scene.m_vEntities.data()));

        CMaterialComponent* pMaterial = pEntity->GetMaterialComponent();
        if (pMaterial && !pMaterial->m_TextureStretch)
        {
            CEntityTextureService::MarkEntityUVDirty(pEntity);
        }
    }

    gizmo.m_WasUsingTransform = ImGuizmo::IsUsing();
    gizmo.m_WasUsingMeshEdit = false;
}
