#include "editor/editor_polygon_edit.h"

#include "editor/editor.h"
#include "editor/editor_scene_picker.h"
#include "editor/editor_state.h"
#include "editable_mesh.h"
#include "engine/transform.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace qc;

bool CPolygonEditor::CreateVertex(const CScene& scene, CEntity& entity,
    const qc::Vec3& worldPosition)
{
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return false;
    }

    SEditableVertex vert;
    vert.Position = qc::Vec3Transform(worldPosition, quark::ComposeMeshWorld(scene, entity).inverted());
    pMesh->m_EditableMesh.m_vVertices.push_back(vert);
    return true;
}

void CPolygonEditor::CreateTriangle(CEntity& entity, int a, int b, int c)
{
    CMeshComponent* pMesh = entity.GetMeshComponent();
    pMesh->m_EditableMesh.m_vTriangles.push_back({a, b, c});

    RebuildMeshFromEditable(pMesh->m_Model, pMesh->m_EditableMesh);
}

void CPolygonEditor::Draw(CEditor& editor, const qc::Camera3D& camera)
{
    SGizmoState& gizmo = editor.m_Ui.m_Gizmo;
    CViewportState& viewport = editor.m_Ui.m_Viewport;
    SVertexEditState& edit = editor.m_Ui.m_VertexEdit;
    EPolygonEditMode& poly = editor.m_Ui.m_PolygonEditMode;

    CEntity* pEntity = editor.m_Scene.GetSelected();
    if (!pEntity)
    {
        return;
    }

    CMeshComponent* pMesh = pEntity->GetMeshComponent();
    if (!pMesh || !pMesh->m_VertexGizmo)
    {
        return;
    }

    CEditableMesh& eMesh = pMesh->m_EditableMesh;
    if (edit.vSelectedVertices.empty() && !eMesh.m_vVertices.empty())
    {
        edit.vSelectedVertices.push_back(0);
    }

    ImDrawList* pDraw = ImGui::GetForegroundDrawList();
    const qc::Mat4 meshWorldTransform = quark::ComposeMeshWorld(editor.m_Scene, *pEntity);
    const auto toWorld = [&meshWorldTransform](const qc::Vec3& localPosition)
    {
        return qc::Vec3Transform(localPosition, meshWorldTransform);
    };

    for (const auto& tri : eMesh.m_vTriangles)
    {
        if (tri.A >= (int)eMesh.m_vVertices.size())
        {
            continue;
        }
        if (tri.B >= (int)eMesh.m_vVertices.size())
        {
            continue;
        }
        if (tri.C >= (int)eMesh.m_vVertices.size())
        {
            continue;
        }

        qc::Vec2 p1 = CScenePicker::WorldToScreen(viewport, toWorld(eMesh.m_vVertices[tri.A].Position), camera);
        qc::Vec2 p2 = CScenePicker::WorldToScreen(viewport, toWorld(eMesh.m_vVertices[tri.B].Position), camera);
        qc::Vec2 p3 = CScenePicker::WorldToScreen(viewport, toWorld(eMesh.m_vVertices[tri.C].Position), camera);

        pDraw->AddLine({p1.x,p1.y}, {p2.x,p2.y}, IM_COL32(0,255,0,255), 2.0f);
        pDraw->AddLine({p2.x,p2.y}, {p3.x,p3.y}, IM_COL32(0,255,0,255), 2.0f);
        pDraw->AddLine({p3.x,p3.y}, {p1.x,p1.y}, IM_COL32(0,255,0,255), 2.0f);
    }

    for (int i = 0; i < (int)eMesh.m_vVertices.size(); i++)
    {
        const qc::Vec2 screen = CScenePicker::WorldToScreen(viewport, toWorld(eMesh.m_vVertices[i].Position), camera);

        bool selected = false;
        for (int si : edit.vSelectedVertices)
        {
            if (si == i)
            {
                selected = true;
                break;
            }
        }

        const ImU32 fill = selected ? IM_COL32(255,180,60,255) : IM_COL32(0,220,0,255);
        const float radius = selected ? 9.0f : 6.0f;
        pDraw->AddCircleFilled({screen.x,screen.y}, radius, fill);
        pDraw->AddCircle({screen.x,screen.y}, radius, IM_COL32(20,20,20,255), 0, 2.0f);
    }

    if (edit.vSelectedVertices.size() == 1 && poly != POLY_CREATE)
    {
        const int sel = edit.vSelectedVertices[0];
        if (sel >= 0 && sel < (int)eMesh.m_vVertices.size())
        {
            const qc::Vec3 worldPos = toWorld(eMesh.m_vVertices[sel].Position);

            float aViewMat4[16] = {};
            float aProjectionMat4[16] = {};
            float aTransformMat4[16] = {};

            const qc::Mat4 view = qc::Mat4::lookAt(camera.position, camera.target, camera.up);
            const qc::Mat4 projection = qc::Mat4::perspective(
                camera.fovy * DEG2RAD,
                viewport.m_WindowSize.x / viewport.m_WindowSize.y,
                0.1f,
                1000.0f
            );

            memcpy(aViewMat4, &view, sizeof(aViewMat4));
            memcpy(aProjectionMat4, &projection, sizeof(aProjectionMat4));

            float aT[3] = {worldPos.x, worldPos.y, worldPos.z};
            float aR[3] = {0,0,0};
            float aS[3] = {1,1,1};

            ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
            ImGuizmo::SetRect(viewport.m_WindowPos.x, viewport.m_WindowPos.y, viewport.m_WindowSize.x, viewport.m_WindowSize.y);
            ImGuizmo::RecomposeMatrixFromComponents(aT, aR, aS, aTransformMat4);

            const bool gizmoSnapEnabled = editor.m_Preferences.m_GizmoSnapEnabled ||
                IsKeyDown(KeyboardKey::LeftControl) || IsKeyDown(KeyboardKey::RightControl);
            const float aSnap[3] = {
                editor.m_Preferences.m_GizmoTranslationSnap,
                editor.m_Preferences.m_GizmoTranslationSnap,
                editor.m_Preferences.m_GizmoTranslationSnap
            };
            ImGuizmo::Manipulate(
                aViewMat4,
                aProjectionMat4,
                ImGuizmo::TRANSLATE,
                ImGuizmo::WORLD,
                aTransformMat4,
                nullptr,
                gizmoSnapEnabled ? aSnap : nullptr
            );

            if (ImGuizmo::IsUsing() && !gizmo.WasUsingPolygon)
            {
                editor.SaveState();
            }

            if (ImGuizmo::IsUsing())
            {
                float aNt[3]={}, nr[3]={}, ns[3]={};
                ImGuizmo::DecomposeMatrixToComponents(aTransformMat4, aNt, nr, ns);
                eMesh.m_vVertices[sel].Position = qc::Vec3Transform(
                    {aNt[0], aNt[1], aNt[2]},
                    qc::Mat4Invert(meshWorldTransform)
                );
                RebuildMeshFromEditable(pMesh->m_Model, eMesh);
                CEntityTextureService::MarkEntityBoundsDirty(pEntity);
            }

            gizmo.WasUsingPolygon = ImGuizmo::IsUsing();
        }
    }

    if (!viewport.m_Hovered)
    {
        return;
    }
    if (ImGuizmo::IsUsing())
    {
        return;
    }

    const qc::Vec2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && poly != POLY_CREATE)
    {
        float bestDist = 16.0f;
        int bestVert = -1;

        for (int i = 0; i < (int)eMesh.m_vVertices.size(); i++)
        {
            const qc::Vec2 sp = CScenePicker::WorldToScreen(viewport, toWorld(eMesh.m_vVertices[i].Position), camera);
            const float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
            const float d = sqrtf(dx*dx + dy*dy);
            if (d < bestDist)
            {
                bestDist = d;
                bestVert = i;
            }
        }

        if (bestVert >= 0)
        {
            const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            if (ctrl)
            {
                auto it = std::find(edit.vSelectedVertices.begin(), edit.vSelectedVertices.end(), bestVert);
                if (it != edit.vSelectedVertices.end())
                {
                    edit.vSelectedVertices.erase(it);
                }
                else
                {
                    edit.vSelectedVertices.push_back(bestVert);
                }
            }
            else
            {
                edit.vSelectedVertices = {bestVert};
            }
        }
        else
        {
            const qc::Ray ray = CScenePicker::ScreenToWorldRay(viewport, mouse, camera);
            float bestHitDist = FLT_MAX;
            int pickedTriangle = -1;
            int pickedCorner = 0;

            for (int t = 0; t < (int)eMesh.m_vTriangles.size(); t++)
            {
                const auto& tri = eMesh.m_vTriangles[t];
                if (tri.A >= (int)eMesh.m_vVertices.size())
                {
                    continue;
                }
                if (tri.B >= (int)eMesh.m_vVertices.size())
                {
                    continue;
                }
                if (tri.C >= (int)eMesh.m_vVertices.size())
                {
                    continue;
                }

                const qc::Vec3 va = toWorld(eMesh.m_vVertices[tri.A].Position);
                const qc::Vec3 vb = toWorld(eMesh.m_vVertices[tri.B].Position);
                const qc::Vec3 vc = toWorld(eMesh.m_vVertices[tri.C].Position);

                const qc::RayCollision hit = GetRayCollisionTriangle(ray, va, vb, vc);
                if (!hit.hit || hit.distance >= bestHitDist)
                {
                    continue;
                }
                bestHitDist = hit.distance;
                pickedTriangle = t;

                const float aCd[3] = {
                    qc::Vec3Distance(hit.point, va),
                    qc::Vec3Distance(hit.point, vb),
                    qc::Vec3Distance(hit.point, vc)
                };

                pickedCorner = (aCd[0]<aCd[1]) ? (aCd[0]<aCd[2]?0:2) : (aCd[1]<aCd[2]?1:2);
            }

            if (pickedTriangle >= 0)
            {
                const auto& tri = eMesh.m_vTriangles[pickedTriangle];
                const int aVerts[3] = {tri.A, tri.B, tri.C};
                const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

                if (!ctrl)
                {
                    edit.vSelectedVertices.clear();
                }
                const int pick = aVerts[pickedCorner];
                if (std::find(edit.vSelectedVertices.begin(), edit.vSelectedVertices.end(), pick) == edit.vSelectedVertices.end())
                {
                    edit.vSelectedVertices.push_back(pick);
                }

            }
            else if (!IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL))
            {
                edit.vSelectedVertices.clear();
            }
        }
    }

    if (poly == POLY_CREATE && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        const qc::Ray ray = CScenePicker::ScreenToWorldRay(viewport, mouse, camera);
        qc::Vec3 placePos = {};
        bool hitMesh = false;

        for (int t = 0; t < (int)eMesh.m_vTriangles.size(); t++)
        {
            const auto& tri = eMesh.m_vTriangles[t];
            if (tri.A >= (int)eMesh.m_vVertices.size())
            {
                continue;
            }
            if (tri.B >= (int)eMesh.m_vVertices.size())
            {
                continue;
            }
            if (tri.C >= (int)eMesh.m_vVertices.size())
            {
                continue;
            }

            const qc::RayCollision hit = GetRayCollisionTriangle(ray,
                toWorld(eMesh.m_vVertices[tri.A].Position),
                toWorld(eMesh.m_vVertices[tri.B].Position),
                toWorld(eMesh.m_vVertices[tri.C].Position));

            if (hit.hit)
            {
                placePos = hit.point;
                hitMesh = true;
                break;
            }
        }

        if (!hitMesh)
        {
            placePos = CScenePicker::RayPlaneHit(ray);
        }

        editor.SaveState();
        CreateVertex(editor.m_Scene, *pEntity, placePos);

        const int newIndex = (int)eMesh.m_vVertices.size() - 1;
        edit.vSelectedVertices.push_back(newIndex);

        if ((int)edit.vSelectedVertices.size() == 3)
        {
            CreateTriangle(*pEntity, edit.vSelectedVertices[0], edit.vSelectedVertices[1], edit.vSelectedVertices[2]);
            edit.vSelectedVertices.clear();
        }
    }
}
