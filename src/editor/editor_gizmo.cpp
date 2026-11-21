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

    int s_ActiveUiHandle = -1;
    int s_ActiveUiHandleEntity = -1;
    bool s_HoveredUiHandle = false;
    int s_ActiveColliderHandle = -1;
    int s_ActiveColliderEntity = -1;
    bool s_HoveredColliderHandle = false;
    float s_ColliderDragStartExtent = 0.5f;
    Vec3 s_ColliderDragStartCenter = { 0.0f, 0.0f, 0.0f };
    ImVec2 s_ColliderDragStartMouse = { 0.0f, 0.0f };

    bool ProjectWorldPointToViewport(const Vec3& worldPos, const Mat4& view, const Mat4& projection,
                                     const CViewportState& viewport, ImVec2& outScreenPos)
    {
        const Vec4 clip = projection * (view * Vec4{ worldPos.x, worldPos.y, worldPos.z, 1.0f });
        if (clip.w <= 1e-5f)
        {
            return false;
        }
        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        outScreenPos.x = viewport.m_WindowPos.x + (ndcX * 0.5f + 0.5f) * viewport.m_WindowSize.x;
        outScreenPos.y = viewport.m_WindowPos.y + (-ndcY * 0.5f + 0.5f) * viewport.m_WindowSize.y;
        return true;
    }

    bool DrawInteractiveUiRectAndAnchorsGizmo(
        CEditor& editor, int selectedEntityIndex, int canvasIndex,
        const CCanvasComponent* pCanvas, CUiRectTransformComponent* pRectTransform,
        const CViewportState& viewport, float zoom2D, float targetX, float targetY)
    {
        if (!pCanvas || !pRectTransform || selectedEntityIndex == canvasIndex || editor.m_IsPlaying)
        {
            return false;
        }

        const float canvasW = pCanvas->m_Width;
        const float canvasH = pCanvas->m_Height;
        const float canvasX = (viewport.m_WindowSize.x - canvasW) * 0.5f;
        const float canvasY = (viewport.m_WindowSize.y - canvasH) * 0.5f;

        auto canvasToScreen = [&](float cx, float cy) -> ImVec2
        {
            const float wx = canvasX + cx;
            const float wy = canvasY + (canvasH - cy);
            return ImVec2(
                viewport.m_WindowPos.x + viewport.m_WindowSize.x * 0.5f + (wx - targetX) * zoom2D,
                viewport.m_WindowPos.y + viewport.m_WindowSize.y * 0.5f + (wy - targetY) * zoom2D);
        };

        auto screenToCanvas = [&](ImVec2 screenPt) -> Vec2
        {
            const float wx = (screenPt.x - viewport.m_WindowPos.x - viewport.m_WindowSize.x * 0.5f) / zoom2D + targetX;
            const float wy = (screenPt.y - viewport.m_WindowPos.y - viewport.m_WindowSize.y * 0.5f) / zoom2D + targetY;
            return Vec2{
                wx - canvasX,
                canvasH - (wy - canvasY)
            };
        };

        const CEntity& entity = editor.m_Scene.m_vEntities[selectedEntityIndex];
        Rectangle parentCanvasRect = { 0.0f, 0.0f, canvasW, canvasH };
        if (entity.m_ParentId >= 0)
        {
            int parentIdx = -1;
            if (entity.m_ParentId < static_cast<int>(editor.m_Scene.m_vEntities.size()) &&
                editor.m_Scene.m_vEntities[entity.m_ParentId].m_Id == entity.m_ParentId)
            {
                parentIdx = entity.m_ParentId;
            }
            else
            {
                for (int i = 0; i < static_cast<int>(editor.m_Scene.m_vEntities.size()); ++i)
                {
                    if (editor.m_Scene.m_vEntities[i].m_Id == entity.m_ParentId)
                    {
                        parentIdx = i;
                        break;
                    }
                }
            }
            if (parentIdx >= 0 && parentIdx != canvasIndex)
            {
                parentCanvasRect = quark::ui::CCanvasSystem::Get().ComputeElementCanvasRect(
                    editor.m_Scene, parentIdx, { canvasW, canvasH });
            }
        }

        const Rectangle elemRect = quark::ui::CCanvasSystem::Get().ComputeElementCanvasRect(
            editor.m_Scene, selectedEntityIndex, { canvasW, canvasH });

        const Vec2 aAnchorCanvasPts[4] = {
            {
                parentCanvasRect.x + parentCanvasRect.width * pRectTransform->m_AnchorMin.x,
                parentCanvasRect.y + parentCanvasRect.height * pRectTransform->m_AnchorMin.y
            },
            {
                parentCanvasRect.x + parentCanvasRect.width * pRectTransform->m_AnchorMax.x,
                parentCanvasRect.y + parentCanvasRect.height * pRectTransform->m_AnchorMin.y
            },
            {
                parentCanvasRect.x + parentCanvasRect.width * pRectTransform->m_AnchorMax.x,
                parentCanvasRect.y + parentCanvasRect.height * pRectTransform->m_AnchorMax.y
            },
            {
                parentCanvasRect.x + parentCanvasRect.width * pRectTransform->m_AnchorMin.x,
                parentCanvasRect.y + parentCanvasRect.height * pRectTransform->m_AnchorMax.y
            }
        };

        const Vec2 aCornerCanvasPts[4] = {
            { elemRect.x, elemRect.y },
            { elemRect.x + elemRect.width, elemRect.y },
            { elemRect.x + elemRect.width, elemRect.y + elemRect.height },
            { elemRect.x, elemRect.y + elemRect.height }
        };

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();
        const ImVec2 anchorScreenMin = canvasToScreen(aAnchorCanvasPts[0].x, aAnchorCanvasPts[2].y);
        const ImVec2 anchorScreenMax = canvasToScreen(aAnchorCanvasPts[2].x, aAnchorCanvasPts[0].y);
        pDrawList->AddRect(anchorScreenMin, anchorScreenMax, IM_COL32(0, 220, 255, 130), 0.0f, 0, 1.0f);

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        int hoveredHandle = -1;
        constexpr float kHitRadius = 8.0f;

        for (int i = 0; i < 4; ++i)
        {
            const ImVec2 sp = canvasToScreen(aAnchorCanvasPts[i].x, aAnchorCanvasPts[i].y);
            const float dx = mousePos.x - sp.x;
            const float dy = mousePos.y - sp.y;
            if (dx * dx + dy * dy <= kHitRadius * kHitRadius)
            {
                hoveredHandle = i;
            }
        }
        for (int i = 0; i < 4; ++i)
        {
            const ImVec2 sp = canvasToScreen(aCornerCanvasPts[i].x, aCornerCanvasPts[i].y);
            const float dx = mousePos.x - sp.x;
            const float dy = mousePos.y - sp.y;
            if (dx * dx + dy * dy <= kHitRadius * kHitRadius)
            {
                hoveredHandle = 4 + i;
            }
        }

        for (int i = 0; i < 4; ++i)
        {
            const ImVec2 sp = canvasToScreen(aAnchorCanvasPts[i].x, aAnchorCanvasPts[i].y);
            const bool activeOrHovered = (s_ActiveUiHandle == i || hoveredHandle == i);
            const ImU32 fillCol = activeOrHovered ? IM_COL32(255, 240, 80, 255) : IM_COL32(0, 215, 255, 230);
            const float r = activeOrHovered ? 7.0f : 5.5f;
            pDrawList->AddQuadFilled(
                ImVec2(sp.x, sp.y - r),
                ImVec2(sp.x + r, sp.y),
                ImVec2(sp.x, sp.y + r),
                ImVec2(sp.x - r, sp.y),
                fillCol);
            pDrawList->AddQuad(
                ImVec2(sp.x, sp.y - r),
                ImVec2(sp.x + r, sp.y),
                ImVec2(sp.x, sp.y + r),
                ImVec2(sp.x - r, sp.y),
                IM_COL32(20, 30, 40, 255), 1.5f);
        }

        for (int i = 0; i < 4; ++i)
        {
            const ImVec2 sp = canvasToScreen(aCornerCanvasPts[i].x, aCornerCanvasPts[i].y);
            const bool activeOrHovered = (s_ActiveUiHandle == (4 + i) || hoveredHandle == (4 + i));
            const ImU32 fillCol = activeOrHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 200, 50, 240);
            const float r = activeOrHovered ? 5.5f : 4.5f;
            pDrawList->AddRectFilled(ImVec2(sp.x - r, sp.y - r), ImVec2(sp.x + r, sp.y + r), fillCol);
            pDrawList->AddRect(ImVec2(sp.x - r, sp.y - r), ImVec2(sp.x + r, sp.y + r), IM_COL32(25, 25, 30, 255), 0.0f, 0, 1.2f);
        }

        s_HoveredUiHandle = (hoveredHandle >= 0);

        if (viewport.m_Hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoveredHandle >= 0)
        {
            editor.SaveState();
            s_ActiveUiHandle = hoveredHandle;
            s_ActiveUiHandleEntity = selectedEntityIndex;
        }

        if (s_ActiveUiHandle >= 0 && s_ActiveUiHandleEntity == selectedEntityIndex)
        {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                const Vec2 mouseCanvas = screenToCanvas(mousePos);
                if (s_ActiveUiHandle < 4)
                {
                    const float normX = std::clamp(
                        (mouseCanvas.x - parentCanvasRect.x) / std::max(1.0f, parentCanvasRect.width),
                        0.0f, 1.0f);
                    const float normY = std::clamp(
                        (mouseCanvas.y - parentCanvasRect.y) / std::max(1.0f, parentCanvasRect.height),
                        0.0f, 1.0f);

                    if (s_ActiveUiHandle == 0 || s_ActiveUiHandle == 3)
                    {
                        pRectTransform->m_AnchorMin.x = std::min(normX, pRectTransform->m_AnchorMax.x);
                    }
                    else
                    {
                        pRectTransform->m_AnchorMax.x = std::max(normX, pRectTransform->m_AnchorMin.x);
                    }

                    if (s_ActiveUiHandle == 0 || s_ActiveUiHandle == 1)
                    {
                        pRectTransform->m_AnchorMin.y = std::min(normY, pRectTransform->m_AnchorMax.y);
                    }
                    else
                    {
                        pRectTransform->m_AnchorMax.y = std::max(normY, pRectTransform->m_AnchorMin.y);
                    }
                }
                else
                {
                    const int cornerIdx = s_ActiveUiHandle - 4;
                    float left = elemRect.x;
                    float bottom = elemRect.y;
                    float right = elemRect.x + elemRect.width;
                    float top = elemRect.y + elemRect.height;

                    if (cornerIdx == 0 || cornerIdx == 3)
                    {
                        left = std::min(mouseCanvas.x, right - 4.0f);
                    }
                    else
                    {
                        right = std::max(mouseCanvas.x, left + 4.0f);
                    }

                    if (cornerIdx == 0 || cornerIdx == 1)
                    {
                        bottom = std::min(mouseCanvas.y, top - 4.0f);
                    }
                    else
                    {
                        top = std::max(mouseCanvas.y, bottom + 4.0f);
                    }

                    const float newW = std::max(4.0f, right - left);
                    const float newH = std::max(4.0f, top - bottom);
                    const float localX = left - parentCanvasRect.x;
                    const float localY = bottom - parentCanvasRect.y;

                    pRectTransform->m_Size = { newW, newH };
                    if (pRectTransform->m_AnchorMin.x == pRectTransform->m_AnchorMax.x &&
                        pRectTransform->m_AnchorMin.y == pRectTransform->m_AnchorMax.y)
                    {
                        const float anchorX = parentCanvasRect.width * pRectTransform->m_AnchorMin.x;
                        const float anchorY = parentCanvasRect.height * pRectTransform->m_AnchorMin.y;
                        const float pivotX = localX + newW * pRectTransform->m_Pivot.x;
                        const float pivotY = localY + newH * pRectTransform->m_Pivot.y;
                        pRectTransform->SetAnchoredPosition({ pivotX - anchorX, pivotY - anchorY });
                    }
                    else
                    {
                        const float anchorMinX = parentCanvasRect.width * pRectTransform->m_AnchorMin.x;
                        const float anchorMinY = parentCanvasRect.height * pRectTransform->m_AnchorMin.y;
                        const float anchorMaxX = parentCanvasRect.width * pRectTransform->m_AnchorMax.x;
                        const float anchorMaxY = parentCanvasRect.height * pRectTransform->m_AnchorMax.y;
                        pRectTransform->m_OffsetMin = { localX - anchorMinX, localY - anchorMinY };
                        pRectTransform->m_OffsetMax = { (localX + newW) - anchorMaxX, (localY + newH) - anchorMaxY };
                    }
                }
                editor.m_SceneDirty = true;
                return true;
            }
            else
            {
                s_ActiveUiHandle = -1;
                s_ActiveUiHandleEntity = -1;
            }
        }

        return hoveredHandle >= 0;
    }

    bool DrawInteractiveColliderGizmo(
        CEditor& editor, CEntity& entity, int entityIndex,
        const Mat4& view, const Mat4& projection, const CViewportState& viewport)
    {
        if (editor.m_Scene2DMode || editor.m_IsPlaying)
        {
            return false;
        }

        CCollisionComponent* pCollision = entity.GetCollisionComponent();
        if (!pCollision || !pCollision->m_Enabled || !pCollision->m_Visualize)
        {
            return false;
        }

        const Mat4 entityWorld = quark::ComposeWorld(editor.m_Scene, entity);
        const Vec3 localCenter = pCollision->m_Center;
        const Vec3 worldCenter = entityWorld * localCenter;

        Vec3 aLocalHalf = {
            std::max(0.05f, pCollision->m_Size.x * 0.5f),
            std::max(0.05f, pCollision->m_Size.y * 0.5f),
            std::max(0.05f, pCollision->m_Size.z * 0.5f)
        };
        if (pCollision->m_ColliderType == COLLIDER_SPHERE)
        {
            const float r = std::max(0.05f, pCollision->m_Radius);
            aLocalHalf = { r, r, r };
        }
        else if (pCollision->m_ColliderType == COLLIDER_CAPSULE)
        {
            const float r = std::max(0.05f, pCollision->m_Radius);
            const float hh = std::max(r, pCollision->m_Height * 0.5f);
            aLocalHalf = { r, hh, r };
        }

        const Vec3 aLocalDirs[6] = {
            { 1.0f, 0.0f, 0.0f },
            { -1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, -1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, -1.0f }
        };
        const float aExtents[3] = { aLocalHalf.x, aLocalHalf.y, aLocalHalf.z };

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();
        const ImU32 wireColor = IM_COL32(60, 235, 110, 210);

        const Vec3 aBoxCorners[8] = {
            entityWorld * (localCenter + Vec3{ -aLocalHalf.x, -aLocalHalf.y, -aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{  aLocalHalf.x, -aLocalHalf.y, -aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{  aLocalHalf.x,  aLocalHalf.y, -aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{ -aLocalHalf.x,  aLocalHalf.y, -aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{ -aLocalHalf.x, -aLocalHalf.y,  aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{  aLocalHalf.x, -aLocalHalf.y,  aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{  aLocalHalf.x,  aLocalHalf.y,  aLocalHalf.z }),
            entityWorld * (localCenter + Vec3{ -aLocalHalf.x,  aLocalHalf.y,  aLocalHalf.z })
        };
        ImVec2 aScreenCorners[8] = {};
        bool aCornerVisible[8] = {};
        for (int i = 0; i < 8; ++i)
        {
            aCornerVisible[i] = ProjectWorldPointToViewport(
                aBoxCorners[i], view, projection, viewport, aScreenCorners[i]);
        }
        constexpr int aEdges[12][2] = {
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
        };
        for (const auto& edge : aEdges)
        {
            if (aCornerVisible[edge[0]] && aCornerVisible[edge[1]])
            {
                pDrawList->AddLine(aScreenCorners[edge[0]], aScreenCorners[edge[1]], wireColor, 1.5f);
            }
        }

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        int hoveredHandle = -1;
        ImVec2 aHandleScreen[6] = {};
        bool aHandleVis[6] = {};

        for (int i = 0; i < 6; ++i)
        {
            const int axis = i / 2;
            const Vec3 handleLocal = localCenter + aLocalDirs[i] * aExtents[axis];
            const Vec3 handleWorld = entityWorld * handleLocal;
            aHandleVis[i] = ProjectWorldPointToViewport(
                handleWorld, view, projection, viewport, aHandleScreen[i]);
            if (aHandleVis[i])
            {
                const float dx = mousePos.x - aHandleScreen[i].x;
                const float dy = mousePos.y - aHandleScreen[i].y;
                if (dx * dx + dy * dy <= 64.0f)
                {
                    hoveredHandle = i;
                }
            }
        }

        for (int i = 0; i < 6; ++i)
        {
            if (!aHandleVis[i])
            {
                continue;
            }
            const bool activeOrHovered = (s_ActiveColliderHandle == i || hoveredHandle == i);
            const float r = activeOrHovered ? 6.0f : 4.5f;
            const ImU32 fillCol = activeOrHovered ? IM_COL32(180, 255, 120, 255) : IM_COL32(50, 230, 100, 240);
            pDrawList->AddRectFilled(
                ImVec2(aHandleScreen[i].x - r, aHandleScreen[i].y - r),
                ImVec2(aHandleScreen[i].x + r, aHandleScreen[i].y + r),
                fillCol);
            pDrawList->AddRect(
                ImVec2(aHandleScreen[i].x - r, aHandleScreen[i].y - r),
                ImVec2(aHandleScreen[i].x + r, aHandleScreen[i].y + r),
                IM_COL32(15, 35, 20, 255), 0.0f, 0, 1.3f);
        }

        s_HoveredColliderHandle = (hoveredHandle >= 0);

        if (viewport.m_Hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoveredHandle >= 0)
        {
            editor.SaveState();
            s_ActiveColliderHandle = hoveredHandle;
            s_ActiveColliderEntity = entityIndex;
            s_ColliderDragStartExtent = aExtents[hoveredHandle / 2];
            s_ColliderDragStartCenter = pCollision->m_Center;
            s_ColliderDragStartMouse = mousePos;
        }

        if (s_ActiveColliderHandle >= 0 && s_ActiveColliderEntity == entityIndex)
        {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                const int axis = s_ActiveColliderHandle / 2;
                const Vec3 unitOffsetWorld = entityWorld * (localCenter + aLocalDirs[s_ActiveColliderHandle]);
                ImVec2 centerScreen = {};
                ImVec2 unitScreen = {};
                if (ProjectWorldPointToViewport(worldCenter, view, projection, viewport, centerScreen) &&
                    ProjectWorldPointToViewport(unitOffsetWorld, view, projection, viewport, unitScreen))
                {
                    const ImVec2 screenAxis(unitScreen.x - centerScreen.x, unitScreen.y - centerScreen.y);
                    const float axisLenSq = screenAxis.x * screenAxis.x + screenAxis.y * screenAxis.y;
                    if (axisLenSq > 1e-3f)
                    {
                        const ImVec2 mouseDelta(
                            mousePos.x - s_ColliderDragStartMouse.x,
                            mousePos.y - s_ColliderDragStartMouse.y);
                        const float deltaUnits =
                            (mouseDelta.x * screenAxis.x + mouseDelta.y * screenAxis.y) / axisLenSq;
                        const float newExtent = std::max(0.05f, s_ColliderDragStartExtent + deltaUnits);

                        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                        {
                            const float shiftAmount = (newExtent - s_ColliderDragStartExtent) * 0.5f;
                            pCollision->m_Center = s_ColliderDragStartCenter + aLocalDirs[s_ActiveColliderHandle] * shiftAmount;
                        }

                        if (pCollision->m_ColliderType == COLLIDER_SPHERE)
                        {
                            pCollision->m_Radius = newExtent;
                        }
                        else if (pCollision->m_ColliderType == COLLIDER_CAPSULE)
                        {
                            if (axis == 1)
                            {
                                pCollision->m_Height = std::max(pCollision->m_Radius * 2.0f, newExtent * 2.0f);
                            }
                            else
                            {
                                pCollision->m_Radius = newExtent;
                            }
                        }
                        else
                        {
                            if (axis == 0)
                            {
                                pCollision->m_Size.x = newExtent * 2.0f;
                            }
                            else if (axis == 1)
                            {
                                pCollision->m_Size.y = newExtent * 2.0f;
                            }
                            else
                            {
                                pCollision->m_Size.z = newExtent * 2.0f;
                            }
                        }
                        editor.m_SceneDirty = true;
                    }
                }
                return true;
            }
            else
            {
                s_ActiveColliderHandle = -1;
                s_ActiveColliderEntity = -1;
            }
        }

        return hoveredHandle >= 0;
    }
} // anonymous

bool CGizmoController::IsCustomGizmoOver()
{
    return s_HoveredColliderHandle || s_HoveredUiHandle ||
        s_ActiveColliderHandle >= 0 || s_ActiveUiHandle >= 0;
}

bool CGizmoController::IsCustomGizmoUsing()
{
    return s_ActiveColliderHandle >= 0 || s_ActiveUiHandle >= 0;
}

void CGizmoController::Draw(CEditor& editor, CFlyCamera& camera)
{
    s_HoveredColliderHandle = false;
    s_HoveredUiHandle = false;
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

            pRectTransform->SetAnchoredPosition({
                s_UiDragStartAnchoredPos.x + deltaLocalX,
                s_UiDragStartAnchoredPos.y + deltaLocalY
            });
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

        if (DrawInteractiveUiRectAndAnchorsGizmo(
                editor, selectedEntityIndex, canvasIndex, pCanvas, pRectTransform.get(),
                viewport, zoom2D, targetX, targetY) &&
            s_ActiveUiHandle >= 0)
        {
            return;
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

            pRectTransform->SetAnchoredPosition({
                s_UiDragStartAnchoredPos.x + deltaX,
                s_UiDragStartAnchoredPos.y + deltaY
            });
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

    if (DrawInteractiveColliderGizmo(editor, *pEntity, selectedEntityIndex, view, projection, viewport) &&
        s_ActiveColliderHandle >= 0)
    {
        return;
    }

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

    Mat4 gizmoTransform = quark::ComposeWorld(editor.m_Scene, *pEntity);
    {
        constexpr float MIN_GIZMO_AXIS_LEN = 1e-3f;
        const Mat4 fallbackRot =
            Mat4::rotationY(pTransform->m_Rotation.y * DEG2RAD) *
            Mat4::rotationX(pTransform->m_Rotation.x * DEG2RAD) *
            Mat4::rotationZ(pTransform->m_Rotation.z * DEG2RAD);
        for (int col = 0; col < 3; ++col)
        {
            Vec3 axis = {
                gizmoTransform.m[col * 4 + 0],
                gizmoTransform.m[col * 4 + 1],
                gizmoTransform.m[col * 4 + 2]
            };
            const float lenSq = axis.lengthSquared();
            if (!std::isfinite(lenSq) || lenSq < MIN_GIZMO_AXIS_LEN * MIN_GIZMO_AXIS_LEN)
            {
                Vec3 fallbackAxis = {
                    fallbackRot.m[col * 4 + 0],
                    fallbackRot.m[col * 4 + 1],
                    fallbackRot.m[col * 4 + 2]
                };
                if (fallbackAxis.lengthSquared() < 1e-6f)
                {
                    fallbackAxis = {
                        col == 0 ? 1.0f : 0.0f,
                        col == 1 ? 1.0f : 0.0f,
                        col == 2 ? 1.0f : 0.0f
                    };
                }
                fallbackAxis = fallbackAxis.normalized() * MIN_GIZMO_AXIS_LEN;
                gizmoTransform.m[col * 4 + 0] = fallbackAxis.x;
                gizmoTransform.m[col * 4 + 1] = fallbackAxis.y;
                gizmoTransform.m[col * 4 + 2] = fallbackAxis.z;
            }
        }
    }
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
        bool matrixFinite = true;
        for (int k = 0; k < 16; ++k)
        {
            if (!std::isfinite(aTransformMat4[k]))
            {
                matrixFinite = false;
                break;
            }
        }

        if (matrixFinite)
        {
            Mat4 worldTransform = Mat4::identity();
            memcpy(&worldTransform, aTransformMat4, sizeof(aTransformMat4));
            const Mat4 parentWorld = quark::ParentWorld(editor.m_Scene, *pEntity);
            Mat4 inverseParent = Mat4::identity();
            const bool hasInverseParent = quark::TryInvertAffine(parentWorld, inverseParent);

            if (editor.m_Ui.m_MeshEdit.m_GizmoMode == ImGuizmo::TRANSLATE)
            {
                const Vec3 worldPos = { worldTransform.m[12], worldTransform.m[13], worldTransform.m[14] };
                const Vec3 localPos = hasInverseParent ? Vec3(inverseParent * worldPos) : worldPos;
                pTransform->m_Position = localPos;
                if (pTransform->m_HasLocalMatrixOverride)
                {
                    pTransform->m_LocalMatrixOverride.m[12] = localPos.x;
                    pTransform->m_LocalMatrixOverride.m[13] = localPos.y;
                    pTransform->m_LocalMatrixOverride.m[14] = localPos.z;
                }
            }
            else
            {
                quark::DecomposeLocal(parentWorld, worldTransform, *pTransform);
                constexpr float MIN_SCALE_COMPONENT = 0.001f;
                auto clampNonZeroScale = [MIN_SCALE_COMPONENT](float s) -> float
                {
                    if (!std::isfinite(s) || std::fabs(s) < MIN_SCALE_COMPONENT)
                    {
                        return (s < 0.0f) ? -MIN_SCALE_COMPONENT : MIN_SCALE_COMPONENT;
                    }
                    return s;
                };
                pTransform->m_Scale.x = clampNonZeroScale(pTransform->m_Scale.x);
                pTransform->m_Scale.y = clampNonZeroScale(pTransform->m_Scale.y);
                pTransform->m_Scale.z = clampNonZeroScale(pTransform->m_Scale.z);
                pTransform->ClearLocalMatrixOverride();
                pTransform->SetLocalMatrixOverride(quark::ComposeLocal(*pTransform));
            }
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
