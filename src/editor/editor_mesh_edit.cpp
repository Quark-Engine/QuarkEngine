#include "editor/editor_mesh_edit.h"

#include "editor/editor.h"
#include "editor/editor_scene_picker.h"
#include "editor/editor_state.h"
#include "editor/editor_utils.h"
#include "engine/transform.h"
#include "models.h"
#include "text_mesh.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <cmath>
#include <vector>
void CMeshEditor::SyncState(CEditor& editor)
{
    SVertexEditState& edit = editor.m_Ui.m_VertexEdit;

    if (editor.m_Scene.m_Selected != edit.EntityIndex)
    {
        edit.EntityIndex = editor.m_Scene.m_Selected;
        edit.MeshIndex = 0;
        edit.TriangleIndex = 0;
        edit.VertexCorner = 0;
        editor.m_Ui.m_Gizmo.WasUsingMeshEdit = false;
        edit.vSelectedVertices.clear();
    }
}

bool CMeshEditor::GetSelectedTriangleVertices(const CEntity& entity, int meshIndex,
    int triangleIndex, int aOutIndices[3])
{
    const CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    if (!pMeshComponent || !HasValidModelData(pMeshComponent->m_Model))
    {
        return false;
    }
    if (meshIndex < 0 || meshIndex >= pMeshComponent->m_Model.meshCount)
    {
        return false;
    }

    const Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
    if (triangleIndex < 0 || triangleIndex >= mesh.triangleCount)
    {
        return false;
    }
    return CMeshOverrideService::GetTriangleVertexIndices(mesh, triangleIndex, aOutIndices);
}

bool CMeshEditor::GetSelectedVertexIndex(const CEntity& entity, int meshIndex, int triangleIndex,
    int vertexCorner, int& outVertexIndex)
{
    int aTriangleVertices[3] = {};
    if (!GetSelectedTriangleVertices(entity, meshIndex, triangleIndex, aTriangleVertices))
    {
        return false;
    }
    if (vertexCorner < 0 || vertexCorner > 2)
    {
        return false;
    }
    outVertexIndex = aTriangleVertices[vertexCorner];
    return true;
}

Vec3 CMeshEditor::GetVertexLocalPosition(const CEntity& entity, int meshIndex, int vertexIndex)
{
    const CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    const Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
    return {
        mesh.vertices[vertexIndex * 3 + 0],
        mesh.vertices[vertexIndex * 3 + 1],
        mesh.vertices[vertexIndex * 3 + 2]
    };
}

Vec3 CMeshEditor::GetVertexWorldPosition(const CScene& scene, const CEntity& entity,
    int meshIndex, int vertexIndex)
{
    const Mat4 transform = quark::ComposeMeshWorld(scene, entity);
    return Vec3Transform(GetVertexLocalPosition(entity, meshIndex, vertexIndex), transform);
}

bool CMeshEditor::EnsureReady(CEntity& entity)
{
    if (CMeshOverrideService::Has(entity))
    {
        return true;
    }
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (pMesh && !pMesh->m_MeshTrianglesDetached)
    {
        CMeshOverrideService::DetachTriangles(entity);
    }
    CMeshOverrideService::CaptureFromModel(entity);
    return CMeshOverrideService::Has(entity);
}

bool CMeshEditor::SetVertexLocalPosition(CEntity& entity, int meshIndex, int vertexIndex,
    const Vec3& localPosition)
{
    CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    if (!pMeshComponent || !HasValidModelData(pMeshComponent->m_Model))
    {
        return false;
    }
    if (meshIndex < 0 || meshIndex >= pMeshComponent->m_Model.meshCount)
    {
        return false;
    }

    Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
    if (!mesh.vertices || vertexIndex < 0 || vertexIndex >= mesh.vertexCount)
    {
        return false;
    }
    if (!EnsureReady(entity))
    {
        return false;
    }
    if (meshIndex >= static_cast<int>(pMeshComponent->m_vMeshVertexOverrides.size()))
    {
        return false;
    }

    std::vector<float>& vMeshOverride = pMeshComponent->m_vMeshVertexOverrides[meshIndex];
    if (vMeshOverride.size() != static_cast<size_t>(mesh.vertexCount * 3))
    {
        return false;
    }

    vMeshOverride[vertexIndex * 3 + 0] = localPosition.x;
    vMeshOverride[vertexIndex * 3 + 1] = localPosition.y;
    vMeshOverride[vertexIndex * 3 + 2] = localPosition.z;

    const bool applied = CMeshOverrideService::Apply(entity);
    if (applied)
    {
        CEntityTextureService::MarkEntityBoundsDirty(&entity);
    }
    return applied;
}

bool CMeshEditor::SetVertexWorldPosition(const CScene& scene, CEntity& entity, int meshIndex,
    int vertexIndex, const Vec3& worldPosition)
{
    const Mat4 inverseTransform = quark::ComposeMeshWorld(scene, entity).inverted();
    const Vec3 localPosition = Vec3Transform(worldPosition, inverseTransform);
    return SetVertexLocalPosition(entity, meshIndex, vertexIndex, localPosition);
}

void CMeshEditor::ResetModel(CEntity& entity, const CFreetypeTextMesh& textMesh)
{
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return;
    }
    CMeshOverrideService::Clear(entity);

    if (pMesh->m_pAsset && pMesh->m_pAsset->m_IsProcedural)
    {
        CModelService::UpdateModel(&entity, textMesh);
    }
    else if (pMesh->m_pAsset)
    {
        CEntityTextureService::RestoreModelTextures(&entity);
        pMesh->ReleaseOwnedResources();

        if (!CModelService::LoadInstance(*pMesh->m_pAsset, pMesh->m_Model))
        {
            pMesh->m_pAsset = nullptr;
            pMesh->m_AssetName.clear();
            pMesh->m_OwnsModelInstance = false;
        }
        else
        {
            pMesh->m_OwnsModelInstance = true;
        }
    }

    if (pMesh->m_pAsset)
    {
        CEntityTextureService::StoreUV(&entity);
        CEntityTextureService::StoreMaterialTextures(&entity);
        pMesh->m_ShaderAssigned = false;
    }
}

void CMeshEditor::DrawOverlay(CEditor& editor, const Camera3D& camera)
{
    CViewportState& viewport = editor.m_Ui.m_Viewport;
    SVertexEditState& edit = editor.m_Ui.m_VertexEdit;

    SyncState(editor);
    if (!edit.Enabled)
    {
        return;
    }

    CEntity* pEntity = editor.m_Scene.GetSelected();
    CMeshComponent* pMesh = pEntity ? pEntity->GetMeshComponent() : nullptr;
    if (!pEntity || !pMesh || !HasValidModelData(pMesh->m_Model))
    {
        return;
    }
    if (edit.MeshIndex >= pMesh->m_Model.meshCount)
    {
        edit.MeshIndex = 0;
    }

    int aTriangleVertices[3] = {};
    if (!GetSelectedTriangleVertices(*pEntity, edit.MeshIndex, edit.TriangleIndex, aTriangleVertices))
    {
        return;
    }

    ImDrawList* pDrawList = ImGui::GetForegroundDrawList();
    Vec2 aScreenPoints[3] = {};

    for (int i = 0; i < 3; i++)
    {
        const Vec3 wp = GetVertexWorldPosition(editor.m_Scene, *pEntity, edit.MeshIndex, aTriangleVertices[i]);
        aScreenPoints[i] = CScenePicker::WorldToScreen(viewport, wp, camera);
    }

    for (int i = 0; i < 3; i++)
    {
        const int next = (i + 1) % 3;
        pDrawList->AddLine(
            ImVec2(aScreenPoints[i].x, aScreenPoints[i].y),
            ImVec2(aScreenPoints[next].x, aScreenPoints[next].y),
            IM_COL32(255, 210, 120, 220), 2.0f
        );
    }

    for (int i = 0; i < 3; i++)
    {
        const bool selected = edit.VertexCorner == i;
        const ImU32 fill = selected ? IM_COL32(255, 170, 64, 255) : IM_COL32(70, 180, 255, 240);
        const float radius = selected ? 8.0f : 6.0f;
        pDrawList->AddCircleFilled(ImVec2(aScreenPoints[i].x, aScreenPoints[i].y), radius, fill);
        pDrawList->AddCircle(ImVec2(aScreenPoints[i].x, aScreenPoints[i].y), radius, IM_COL32(20, 20, 20, 255), 0, 2.0f);
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
    {
        return;
    }
    if (!viewport.m_Hovered)
    {
        return;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const Vec2 mouse = GetMousePosition();
    float bestDistance = 18.0f;
    int bestCorner = -1;

    for (int i = 0; i < 3; i++)
    {
        const float dx = mouse.x - aScreenPoints[i].x;
        const float dy = mouse.y - aScreenPoints[i].y;
        if (sqrtf(dx*dx + dy*dy) < bestDistance)
        {
            bestDistance = sqrtf(dx*dx + dy*dy);
            bestCorner = i;
        }
    }

    if (bestCorner >= 0)
    {
        edit.VertexCorner = bestCorner;
        return;
    }

    int pickedTriangle = -1;
    int pickedCorner = 0;
    if (CScenePicker::PickMeshTriangle(editor.m_Scene, *pEntity, edit.MeshIndex,
            CScenePicker::ScreenToWorldRay(viewport, mouse, camera),
            pickedTriangle, pickedCorner))
            {
        edit.TriangleIndex = pickedTriangle;
        edit.VertexCorner = pickedCorner;
    }
}
