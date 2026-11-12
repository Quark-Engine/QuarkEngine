#include "editor/editor_gizmo.h"

#include "editor/editor.h"
#include "application_plugin_bridge.h"
#include "editor/editor_mesh_edit.h"
#include "editor/editor_state.h"
#include "editor/editor_utils.h"
#include "camera.h"
#include "engine/transform.h"
#include "engine/canvas_system.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <cstring>

namespace
{
    Vec3 WorldToCanvasLocal(const Mat4& canvasWorld, const Vec3& worldPos)
    {
        const Vec3 canvasPos = { canvasWorld.m[12], canvasWorld.m[13], canvasWorld.m[14] };
        const Vec3 diff = worldPos - canvasPos;

        const Vec3 col0 = { canvasWorld.m[0], canvasWorld.m[1], canvasWorld.m[2] };
        const Vec3 col1 = { canvasWorld.m[4], canvasWorld.m[5], canvasWorld.m[6] };
        const Vec3 col2 = { canvasWorld.m[8], canvasWorld.m[9], canvasWorld.m[10] };

        const float scaleX2 = col0.dot(col0);
        const float scaleY2 = col1.dot(col1);
        const float scaleZ2 = col2.dot(col2);

        if (scaleX2 < 1e-12f || scaleY2 < 1e-12f || scaleZ2 < 1e-12f)
        {
            return { 0.0f, 0.0f, 0.0f };
        }

        return Vec3{
            diff.dot(col0) / scaleX2,
            diff.dot(col1) / scaleY2,
            diff.dot(col2) / scaleZ2
        };
    }

    Vec2 s_UiDragStartAnchoredPos = { 0.0f, 0.0f };
    Vec2 s_UiDragStartCenter = { 0.0f, 0.0f };
    int s_UiDragEntityIndex = -1;
    bool s_UiDragging = false;
}

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

            float minX = 0.0f;
            float minY = 0.0f;
            float maxX = 0.0f;
            float maxY = 0.0f;

            const int candCanvasIdx = quark::ui::CCanvasSystem::Get().FindCanvasForEntity(editor.m_Scene, entityIndex);
            if (candCanvasIdx >= 0 && candCanvasIdx < static_cast<int>(editor.m_Scene.m_vEntities.size()))
            {
                const CCanvasComponent* pCandCanvas = editor.m_Scene.m_vEntities[candCanvasIdx].GetCanvasComponent();
                const float cW = pCandCanvas ? pCandCanvas->m_Width : 800.0f;
                const float cH = pCandCanvas ? pCandCanvas->m_Height : 600.0f;
                const float canvasX = (viewport.m_WindowSize.x - cW) * 0.5f;
                const float canvasY = (viewport.m_WindowSize.y - cH) * 0.5f;
                const Rectangle elemRect = quark::ui::CCanvasSystem::Get().ComputeElementCanvasRect(
                    editor.m_Scene, entityIndex, { cW, cH });
                const float screenY = canvasY + (cH - (elemRect.y + elemRect.height));
                minX = canvasX + elemRect.x;
                minY = screenY;
                maxX = minX + elemRect.width;
                maxY = minY + elemRect.height;
            }
            else
            {
                minX = pCandidateRect->m_Position.x;
                minY = pCandidateRect->m_Position.y;
                maxX = minX + std::max(1.0f, pCandidateRect->m_Size.x);
                maxY = minY + std::max(1.0f, pCandidateRect->m_Size.y);
            }

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

    const int selectedEntityIndex = editor.m_Scene.m_Selected;
    const int canvasIndex = pRectTransform
        ? quark::ui::CCanvasSystem::Get().FindCanvasForEntity(editor.m_Scene, selectedEntityIndex)
        : -1;
    const CCanvasComponent* pCanvas = (canvasIndex >= 0 && canvasIndex < static_cast<int>(editor.m_Scene.m_vEntities.size()))
        ? editor.m_Scene.m_vEntities[canvasIndex].GetCanvasComponent()
        : nullptr;
    const bool isWorldSpaceCanvasElement = (!editor.m_Scene2DMode && pRectTransform != nullptr &&
                                            pCanvas != nullptr && pCanvas->m_RenderMode == ECanvasRenderMode::WORLD_SPACE &&
                                            selectedEntityIndex != canvasIndex);

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewport.m_WindowPos.x, viewport.m_WindowPos.y, viewport.m_WindowSize.x, viewport.m_WindowSize.y);
    ImGuizmo::SetOrthographic(editor.m_Scene2DMode || (pRectTransform != nullptr && !isWorldSpaceCanvasElement && (selectedEntityIndex != canvasIndex || !pTransform)));
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

    if (isWorldSpaceCanvasElement)
    {
        ImGuizmo::SetOrthographic(false);
        const Mat4 view = Mat4::lookAt(
            camera.GetCamera().position,
            camera.GetCamera().target,
            camera.GetCamera().up);
        const Mat4 projection = Mat4::perspective(
            camera.GetCamera().fovy * DEG2RAD,
            viewport.m_WindowSize.x / viewport.m_WindowSize.y,
            0.1f,
            1000.0f);

        float aViewMat4[16] = {};
        float aProjectionMat4[16] = {};
        memcpy(aViewMat4, &view, sizeof(aViewMat4));
        memcpy(aProjectionMat4, &projection, sizeof(aProjectionMat4));

        const Mat4 canvasWorld = quark::ui::CCanvasSystem::Get().GetCanvasWorldMatrix(editor.m_Scene, canvasIndex);
        const float canvasW = pCanvas->m_Width;
        const float canvasH = pCanvas->m_Height;
        const Rectangle elemRect = quark::ui::CCanvasSystem::Get().ComputeElementCanvasRect(
            editor.m_Scene, selectedEntityIndex, { canvasW, canvasH });

        const float localCenterX = elemRect.x + elemRect.width * 0.5f - canvasW * 0.5f;
        const float localCenterY = elemRect.y + elemRect.height * 0.5f - canvasH * 0.5f;
        const float localCenterZ = 0.002f;

        const Mat4 elemWorld = canvasWorld *
            Mat4::translation(localCenterX, localCenterY, localCenterZ) *
            Mat4::rotationZ(pRectTransform->m_Rotation2D * DEG2RAD) *
            Mat4::scale(pRectTransform->m_Scale2D.x, pRectTransform->m_Scale2D.y, 1.0f);

        float aElemTransformMat4[16] = {};
        memcpy(aElemTransformMat4, &elemWorld, sizeof(aElemTransformMat4));

        const ImGuizmo::OPERATION op = (editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::SCALE)
            ? ImGuizmo::SCALE
            : ((editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::ROTATE) ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE);

        ImGuizmo::Manipulate(
            aViewMat4,
            aProjectionMat4,
            op,
            ImGuizmo::LOCAL,
            aElemTransformMat4,
            nullptr,
            gizmoSnapEnabled
                ? (op == ImGuizmo::SCALE ? aScaleSnap : (op == ImGuizmo::ROTATE ? aRotationSnap : aTranslationSnap))
                : nullptr);

        if (ImGuizmo::IsUsing() && (!s_UiDragging || s_UiDragEntityIndex != selectedEntityIndex))
        {
            editor.SaveState();
            s_UiDragStartAnchoredPos = pRectTransform->m_AnchoredPosition;
            s_UiDragStartCenter = { localCenterX, localCenterY };
            s_UiDragEntityIndex = selectedEntityIndex;
            s_UiDragging = true;
        }

        if (ImGuizmo::IsUsing() && s_UiDragging && s_UiDragEntityIndex == selectedEntityIndex)
        {
            float aNextTranslation[3] = {};
            float aNextRotation[3] = {};
            float aNextScale[3] = {};
            ImGuizmo::DecomposeMatrixToComponents(
                aElemTransformMat4,
                aNextTranslation,
                aNextRotation,
                aNextScale);

            const Vec3 newWorldPos = { aNextTranslation[0], aNextTranslation[1], aNextTranslation[2] };
            const Vec3 newLocalPos = WorldToCanvasLocal(canvasWorld, newWorldPos);

            const float deltaLocalX = newLocalPos.x - s_UiDragStartCenter.x;
            const float deltaLocalY = newLocalPos.y - s_UiDragStartCenter.y;

            pRectTransform->m_AnchoredPosition.x = s_UiDragStartAnchoredPos.x + deltaLocalX;
            pRectTransform->m_AnchoredPosition.y = s_UiDragStartAnchoredPos.y + deltaLocalY;
            pRectTransform->m_Position = pRectTransform->m_AnchoredPosition;
            if (op == ImGuizmo::ROTATE)
            {
                pRectTransform->m_Rotation2D = aNextRotation[2];
            }
            if (op == ImGuizmo::SCALE)
            {
                pRectTransform->m_Scale2D = {
                    std::max(0.01f, std::fabs(aNextScale[0])),
                    std::max(0.01f, std::fabs(aNextScale[1]))
                };
            }

            if (CUiComponent* pUi = pEntity->GetUiComponent())
            {
                auto pText = pComponents->GetComponentOfType<CTextComponent>();
                pUi->SyncToRuntimeDesc(pRectTransform.get(), pText.get());
            }

            editor.m_SceneDirty = true;
            DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, selectedEntityIndex);
        }
        else if (!ImGuizmo::IsUsing())
        {
            s_UiDragging = false;
            s_UiDragEntityIndex = -1;
        }

        gizmo.m_WasUsingTransform = ImGuizmo::IsUsing();
        gizmo.m_WasUsingMeshEdit = false;
        return;
    }

    if (pRectTransform && (editor.m_Scene2DMode || (pCanvas != nullptr && selectedEntityIndex != canvasIndex)))
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

        Vec2 realCenter = { 0.0f, 0.0f };
        Vec2 realSize = { 100.0f, 100.0f };

        if (pCanvas != nullptr)
        {
            const float canvasW = pCanvas->m_Width;
            const float canvasH = pCanvas->m_Height;
            const float canvasX = (viewport.m_WindowSize.x - canvasW) * 0.5f;
            const float canvasY = (viewport.m_WindowSize.y - canvasH) * 0.5f;
            const Rectangle elemRect = quark::ui::CCanvasSystem::Get().ComputeElementCanvasRect(
                editor.m_Scene, selectedEntityIndex, { canvasW, canvasH });
            const float screenY = canvasY + (canvasH - (elemRect.y + elemRect.height));
            realCenter = { canvasX + elemRect.x + elemRect.width * 0.5f, screenY + elemRect.height * 0.5f };
            realSize = { elemRect.width, elemRect.height };
        }
        else
        {
            realSize = {
                std::max(1.0f, pRectTransform->m_Size.x),
                std::max(1.0f, pRectTransform->m_Size.y)
            };
            realCenter = {
                pRectTransform->m_Position.x + realSize.x * 0.5f,
                pRectTransform->m_Position.y + realSize.y * 0.5f
            };
        }

        if (editor.m_Preferences.m_ShowSelectionVisualization)
        {
            const ImVec2 rectMin(
                viewport.m_WindowPos.x + viewport.m_WindowSize.x * 0.5f + (realCenter.x - realSize.x * 0.5f - targetX) * zoom2D,
                viewport.m_WindowPos.y + viewport.m_WindowSize.y * 0.5f + (realCenter.y - realSize.y * 0.5f - targetY) * zoom2D);
            const ImVec2 rectMax(
                rectMin.x + realSize.x * zoom2D,
                rectMin.y + realSize.y * zoom2D);
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

        float aUiTranslation[3] = { realCenter.x, -realCenter.y, 0.0f };
        float aUiRotation[3] = { 0.0f, 0.0f, pRectTransform->m_Rotation2D };
        float aUiScale[3] = { realSize.x, realSize.y, 1.0f };
        ImGuizmo::RecomposeMatrixFromComponents(
            aUiTranslation,
            aUiRotation,
            aUiScale,
            aUiTransformMat4);

        const ImGuizmo::OPERATION uiOperation =
            (editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::SCALE)
                ? ImGuizmo::SCALE
                : ((editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::ROTATE) ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE);

        ImGuizmo::Manipulate(
            aUiViewMat4,
            aUiProjectionMat4,
            uiOperation,
            ImGuizmo::LOCAL,
            aUiTransformMat4,
            nullptr,
            gizmoSnapEnabled
                ? (uiOperation == ImGuizmo::SCALE ? aScaleSnap : (uiOperation == ImGuizmo::ROTATE ? aRotationSnap : aTranslationSnap))
                : nullptr);

        if (ImGuizmo::IsUsing() && (!s_UiDragging || s_UiDragEntityIndex != selectedEntityIndex))
        {
            editor.SaveState();
            s_UiDragStartAnchoredPos = pRectTransform->m_AnchoredPosition;
            s_UiDragStartCenter = realCenter;
            s_UiDragEntityIndex = selectedEntityIndex;
            s_UiDragging = true;
        }

        if (ImGuizmo::IsUsing() && s_UiDragging && s_UiDragEntityIndex == selectedEntityIndex)
        {
            float aNextTranslation[3] = {};
            float aNextRotation[3] = {};
            float aNextScale[3] = {};
            ImGuizmo::DecomposeMatrixToComponents(
                aUiTransformMat4,
                aNextTranslation,
                aNextRotation,
                aNextScale);

            const Vec2 newCenter = {
                aNextTranslation[0],
                -aNextTranslation[1]
            };
            const float deltaX = newCenter.x - s_UiDragStartCenter.x;
            const float deltaY = -(newCenter.y - s_UiDragStartCenter.y);

            pRectTransform->m_AnchoredPosition.x = s_UiDragStartAnchoredPos.x + deltaX;
            pRectTransform->m_AnchoredPosition.y = s_UiDragStartAnchoredPos.y + deltaY;
            pRectTransform->m_Position = pRectTransform->m_AnchoredPosition;
            if (uiOperation == ImGuizmo::ROTATE)
            {
                pRectTransform->m_Rotation2D = aNextRotation[2];
            }
            if (uiOperation == ImGuizmo::SCALE)
            {
                pRectTransform->m_Size = {
                    std::max(1.0f, std::fabs(aNextScale[0])),
                    std::max(1.0f, std::fabs(aNextScale[1]))
                };
                if (selectedEntityIndex == canvasIndex && pCanvas)
                {
                    const_cast<CCanvasComponent*>(pCanvas)->m_Width = pRectTransform->m_Size.x;
                    const_cast<CCanvasComponent*>(pCanvas)->m_Height = pRectTransform->m_Size.y;
                }
            }

            if (CUiComponent* pUi = pEntity->GetUiComponent())
            {
                auto pText = pComponents->GetComponentOfType<CTextComponent>();
                pUi->SyncToRuntimeDesc(pRectTransform.get(), pText.get());
            }

            editor.m_SceneDirty = true;
            DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, selectedEntityIndex);
        }
        else if (!ImGuizmo::IsUsing())
        {
            s_UiDragging = false;
            s_UiDragEntityIndex = -1;
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
