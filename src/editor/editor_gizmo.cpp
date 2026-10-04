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

using namespace qc;

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

    CEntity* pEntity = editor.m_Scene.GetSelected();
    if (!pEntity)
    {
        return;
    }
    CTransformComponent* pTransform = pEntity->GetTransformComponent();
    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    if (!pTransform)
    {
        return;
    }
    if (pMesh && pMesh->m_VertexGizmo)
    {
        return;
    }
    if (viewport.m_WindowSize.x <= 0 || viewport.m_WindowSize.y <= 0)
    {
        return;
    }

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewport.m_WindowPos.x, viewport.m_WindowPos.y, viewport.m_WindowSize.x, viewport.m_WindowSize.y);
    ImGuizmo::AllowAxisFlip(false);

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
        IsKeyDown(KeyboardKey::LeftControl) || IsKeyDown(KeyboardKey::RightControl);

    const qc::Mat4 view = qc::Mat4::lookAt(
        camera.GetCamera().position,
        camera.GetCamera().target,
        camera.GetCamera().up
    );

    const qc::Mat4 projection = qc::Mat4::perspective(
        camera.GetCamera().fovy * DEG2RAD,
        viewport.m_WindowSize.x / viewport.m_WindowSize.y,
        0.1f,
        1000.0f
    );

    float aViewMat4[16] = {};
    float aProjectionMat4[16] = {};
    float aTransformMat4[16] = {};

    memcpy(aViewMat4, &view, sizeof(aViewMat4));
    memcpy(aProjectionMat4, &projection, sizeof(aProjectionMat4));

    CMeshEditor::DrawOverlay(editor, camera.GetCamera());

    if (pMesh && edit.Enabled && HasValidModelData(pMesh->m_Model))
    {
        if (edit.MeshIndex >= pMesh->m_Model.meshCount)
        {
            edit.MeshIndex = 0;
        }

        int vertexIndex = -1;
        if (CMeshEditor::GetSelectedVertexIndex(
            *pEntity,
            edit.MeshIndex,
            edit.TriangleIndex,
            edit.VertexCorner,
            vertexIndex))
        {
            const qc::Vec3 vertexWorld = CMeshEditor::GetVertexWorldPosition(
                editor.m_Scene,
                *pEntity,
                edit.MeshIndex,
                vertexIndex
            );

            float aVertexTranslation[3] = { vertexWorld.x, vertexWorld.y, vertexWorld.z };
            float aVertexRotation[3] = { 0.0f, 0.0f, 0.0f };
            float aVertexScale[3] = { 1.0f, 1.0f, 1.0f };

            ImGuizmo::RecomposeMatrixFromComponents(
                aVertexTranslation,
                aVertexRotation,
                aVertexScale,
                aTransformMat4
            );

            ImGuizmo::Manipulate(
                aViewMat4,
                aProjectionMat4,
                ImGuizmo::TRANSLATE,
                ImGuizmo::WORLD,
                aTransformMat4,
                nullptr,
                gizmoSnapEnabled ? aTranslationSnap : nullptr
            );

            if (ImGuizmo::IsUsing() && !gizmo.WasUsingMeshEdit)
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
                    aNextScale
                );

                CMeshEditor::SetVertexWorldPosition(
                    editor.m_Scene,
                    *pEntity,
                    edit.MeshIndex,
                    vertexIndex,
                    { aNextTranslation[0], aNextTranslation[1], aNextTranslation[2] }
                );
            }

            gizmo.WasUsingMeshEdit = ImGuizmo::IsUsing();
            return;
        }
    }

    float aTranslation[3] = { pTransform->m_Position.x, pTransform->m_Position.y, pTransform->m_Position.z };

    const qc::Mat4 gizmoTransform = quark::ComposeWorld(editor.m_Scene, *pEntity);
    memcpy(aTransformMat4, &gizmoTransform, sizeof(aTransformMat4));

    ImGuizmo::Manipulate(
        aViewMat4,
        aProjectionMat4,
        editor.m_Ui.m_MeshEdit.GizmoMode,
        ImGuizmo::WORLD,
        aTransformMat4,
        nullptr,
        gizmoSnapEnabled
            ? (editor.m_Ui.m_MeshEdit.GizmoMode == ImGuizmo::TRANSLATE ? aTranslationSnap :
               editor.m_Ui.m_MeshEdit.GizmoMode == ImGuizmo::ROTATE ? aRotationSnap : aScaleSnap)
            : nullptr
    );

    if (ImGuizmo::IsUsing() && !gizmo.WasUsingTransform)
    {
        editor.SaveState();
    }

    if (ImGuizmo::IsUsing())
    {
        qc::Mat4 worldTransform = qc::Mat4::identity();
        memcpy(&worldTransform, aTransformMat4, sizeof(aTransformMat4));
        quark::DecomposeLocal(
            quark::ParentWorld(editor.m_Scene, *pEntity),
            worldTransform,
            *pTransform);
        qc::Mat4 inverseParent;
        if (quark::TryInvertAffine(quark::ParentWorld(editor.m_Scene, *pEntity), inverseParent))
        {
            pTransform->SetLocalMatrixOverride(inverseParent * worldTransform);
        }

        const qc::Vec3 positionDelta = pTransform->m_Position - qc::Vec3{aTranslation[0], aTranslation[1], aTranslation[2]};
        if (editor.m_Scene.m_vSelectedEntities.size() > 1)
        {
            for (int selected_index : editor.m_Scene.m_vSelectedEntities)
            {
                if (selected_index == editor.m_Scene.m_Selected ||
                    selected_index < 0 || selected_index >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
                {
                    continue;
                }
                CEntity& selectedEntity = editor.m_Scene.m_vEntities[selected_index];
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
                DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED, selected_index);
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

    gizmo.WasUsingTransform = ImGuizmo::IsUsing();
    gizmo.WasUsingMeshEdit = false;
}
