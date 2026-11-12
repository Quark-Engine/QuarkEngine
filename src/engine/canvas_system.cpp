#include "engine/canvas_system.h"

#include "editor/editor_state.h"
#include "editor/editor_utils.h"
#include "engine/transform.h"
#include "gui/gui_renderer_bridge.h"
#include "models.h"

#include <algorithm>
#include <cmath>
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

    void DrawUiButtonRoundedOutline(Rectangle rec, float roundness, float thick, Color color)
    {
        const float radius = std::clamp(roundness * std::min(rec.width, rec.height) * 0.5f, 0.0f, std::min(rec.width, rec.height) * 0.5f);
        if (radius <= 0.0f)
        {
            DrawRectangleLinesEx(rec, thick, color);
            return;
        }

        DrawLineEx(Vec2{ rec.x + radius, rec.y }, Vec2{ rec.x + rec.width - radius, rec.y }, thick, color);
        DrawLineEx(Vec2{ rec.x + rec.width, rec.y + radius }, Vec2{ rec.x + rec.width, rec.y + rec.height - radius }, thick, color);
        DrawLineEx(Vec2{ rec.x + rec.width - radius, rec.y + rec.height }, Vec2{ rec.x + radius, rec.y + rec.height }, thick, color);
        DrawLineEx(Vec2{ rec.x, rec.y + rec.height - radius }, Vec2{ rec.x, rec.y + radius }, thick, color);

        auto drawCornerArc = [&](Vec2 center, float startAngle, float endAngle)
        {
            constexpr int kSegments = 8;
            Vec2 last = Vec2{
                center.x + std::cos(startAngle * DEG2RAD) * radius,
                center.y + std::sin(startAngle * DEG2RAD) * radius
            };
            for (int i = 1; i <= kSegments; ++i)
            {
                const float a = (startAngle + (endAngle - startAngle) * static_cast<float>(i) / kSegments) * DEG2RAD;
                const Vec2 cur = Vec2{
                    center.x + std::cos(a) * radius,
                    center.y + std::sin(a) * radius
                };
                DrawLineEx(last, cur, thick, color);
                last = cur;
            }
        };

        drawCornerArc(Vec2{ rec.x + radius, rec.y + radius }, 180.0f, 270.0f);
        drawCornerArc(Vec2{ rec.x + rec.width - radius, rec.y + radius }, 270.0f, 360.0f);
        drawCornerArc(Vec2{ rec.x + rec.width - radius, rec.y + rec.height - radius }, 0.0f, 90.0f);
        drawCornerArc(Vec2{ rec.x + radius, rec.y + rec.height - radius }, 90.0f, 180.0f);
    }

    int FindEntityIndexById(const CScene& scene, int id)
    {
        if (id < 0)
        {
            return -1;
        }
        for (int i = 0; i < static_cast<int>(scene.m_vEntities.size()); ++i)
        {
            if (scene.m_vEntities[i].m_Id == id)
            {
                return i;
            }
        }
        return -1;
    }

    Ray ScreenToWorldRay(const quark::ui::SUiViewport& viewport, const Vec2& mouse, const Camera3D& camera)
    {
        if (viewport.m_WindowSize.x <= 0.0f || viewport.m_WindowSize.y <= 0.0f)
        {
            return Ray{ camera.position, { 0.0f, 0.0f, 1.0f } };
        }

        const Vec2 localPos = { mouse.x - viewport.m_WindowPos.x, mouse.y - viewport.m_WindowPos.y };
        return GetScreenToWorldRayEx(
            localPos,
            camera,
            static_cast<int>(viewport.m_WindowSize.x),
            static_cast<int>(viewport.m_WindowSize.y));
    }

    int FindButtonEntityIndex(const CScene& scene, int hitIndex)
    {
        int current = hitIndex;
        int depth = 0;
        while (current >= 0 && current < static_cast<int>(scene.m_vEntities.size()) && depth < 32)
        {
            if (scene.m_vEntities[current].GetButtonComponent())
            {
                return current;
            }
            if (scene.m_vEntities[current].GetCanvasComponent())
            {
                break;
            }
            int parentIdx = -1;
            if (scene.m_vEntities[current].m_ParentId < static_cast<int>(scene.m_vEntities.size()))
            {
                parentIdx = scene.m_vEntities[current].m_ParentId;
            }
            else
            {
                parentIdx = FindEntityIndexById(scene, scene.m_vEntities[current].m_ParentId);
            }
            current = parentIdx;
            depth++;
        }
        return -1;
    }
} // anonymous

namespace quark::ui
{

    SUiViewport::SUiViewport(const CViewportState& viewport)
        : m_WindowPos{ viewport.m_WindowPos.x, viewport.m_WindowPos.y }
        , m_WindowSize{ viewport.m_WindowSize.x, viewport.m_WindowSize.y }
    {
    }

    CCanvasSystem& CCanvasSystem::Get()
    {
        static CCanvasSystem s_Instance;
        return s_Instance;
    }

    void CCanvasSystem::Initialize()
    {
        m_Initialized = true;
    }

    void CCanvasSystem::Shutdown()
    {
        m_HoveredEntityIndex = -1;
        m_PressedEntityIndex = -1;
        m_ActiveCanvasIndex = -1;
        m_Initialized = false;
    }

    Mat4 CCanvasSystem::GetCanvasWorldMatrix(const CScene& scene, int canvasEntityIndex)
    {
        if (canvasEntityIndex < 0 || canvasEntityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return Mat4::identity();
        }
        return quark::ComposeWorld(scene, canvasEntityIndex);
    }

    int CCanvasSystem::FindCanvasForEntity(const CScene& scene, int entityIndex) const
    {
        if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return -1;
        }

        if (scene.m_vEntities[entityIndex].GetCanvasComponent())
        {
            return entityIndex;
        }

        int current = scene.m_vEntities[entityIndex].m_ParentId;
        int depth = 0;
        while (current >= 0 && depth < 64)
        {
            int parentIdx = -1;
            if (current < static_cast<int>(scene.m_vEntities.size()))
            {
                parentIdx = current;
            }
            else
            {
                parentIdx = FindEntityIndexById(scene, current);
            }

            if (parentIdx < 0 || parentIdx >= static_cast<int>(scene.m_vEntities.size()))
            {
                break;
            }

            if (scene.m_vEntities[parentIdx].GetCanvasComponent())
            {
                return parentIdx;
            }

            current = scene.m_vEntities[parentIdx].m_ParentId;
            depth++;
        }

        return -1;
    }

    Rectangle CCanvasSystem::ComputeElementCanvasRect(const CScene& scene, int entityIndex, const Vec2& canvasSize)
    {
        if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return { 0.0f, 0.0f, canvasSize.x, canvasSize.y };
        }

        const CEntity& entity = scene.m_vEntities[entityIndex];
        const CUiRectTransformComponent* pRectTransform = entity.GetRectTransformComponent();

        Vec2 parentSize = canvasSize;
        Vec2 parentOffset = { 0.0f, 0.0f };

        if (entity.m_ParentId >= 0)
        {
            int parentIndex = -1;
            if (entity.m_ParentId < static_cast<int>(scene.m_vEntities.size()))
            {
                parentIndex = entity.m_ParentId;
            }
            else
            {
                parentIndex = FindEntityIndexById(scene, entity.m_ParentId);
            }

            if (parentIndex >= 0 && parentIndex < static_cast<int>(scene.m_vEntities.size()))
            {
                const CEntity& parentEntity = scene.m_vEntities[parentIndex];
                if (!parentEntity.GetCanvasComponent())
                {
                    const Rectangle parentRect = ComputeElementCanvasRect(scene, parentIndex, canvasSize);
                    parentSize = { parentRect.width, parentRect.height };
                    parentOffset = { parentRect.x, parentRect.y };
                }
            }
        }

        if (pRectTransform)
        {
            Rectangle rect = pRectTransform->CalculateLocalRect(parentSize);
            rect.x += parentOffset.x;
            rect.y += parentOffset.y;
            return rect;
        }

        return { parentOffset.x, parentOffset.y, parentSize.x, parentSize.y };
    }

    bool CCanvasSystem::RaycastWorldCanvas(const CScene& scene, const CCanvasComponent& canvas,
                                          const Mat4& canvasWorldTransform, const Ray& ray,
                                          Vec2& outCanvasPoint, float& outDistance)
    {
        const Vec2 pivot = { 0.5f, 0.5f };
        const Vec3 planePoint = canvasWorldTransform * Vec3{ 0.0f, 0.0f, 0.0f };
        const Vec3 normalPoint = canvasWorldTransform * Vec3{ 0.0f, 0.0f, 1.0f };
        const Vec3 normal = (normalPoint - planePoint).normalized();

        const float denom = ray.direction.dot(normal);
        if (std::fabs(denom) < 0.00001f)
        {
            return false;
        }

        const float t = (planePoint - ray.position).dot(normal) / denom;
        if (t <= 0.001f)
        {
            return false;
        }

        const Vec3 hitWorld = ray.position + ray.direction * t;
        const Vec3 hitLocal = WorldToCanvasLocal(canvasWorldTransform, hitWorld);

        const float canvasX = hitLocal.x + canvas.m_Width * pivot.x;
        const float canvasY = hitLocal.y + canvas.m_Height * pivot.y;

        if (canvasX >= 0.0f && canvasX <= canvas.m_Width &&
            canvasY >= 0.0f && canvasY <= canvas.m_Height)
        {
            outCanvasPoint = { canvasX, canvasY };
            outDistance = t;
            return true;
        }

        return false;
    }

    void CCanvasSystem::CollectCanvasElementsRecursive(const CScene& scene, int parentIndex,
                                                      std::vector<int>& vOutIndices)
    {
        for (int i = 0; i < static_cast<int>(scene.m_vEntities.size()); ++i)
        {
            const CEntity& entity = scene.m_vEntities[i];
            if (entity.m_ParentId == parentIndex ||
                (parentIndex >= 0 && parentIndex < static_cast<int>(scene.m_vEntities.size()) &&
                 entity.m_ParentId == scene.m_vEntities[parentIndex].m_Id))
            {
                vOutIndices.push_back(i);
                CollectCanvasElementsRecursive(scene, i, vOutIndices);
            }
        }
    }

    SCanvasHitResult CCanvasSystem::PickUiElement(CScene& scene, const Ray& ray,
                                                 const Camera3D& camera, float maxDistance)
    {
        SCanvasHitResult bestHit;
        float bestDistance = maxDistance;

        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled || pCanvas->m_RenderMode != ECanvasRenderMode::WORLD_SPACE)
            {
                continue;
            }

            const Mat4 canvasTransform = GetCanvasWorldMatrix(scene, entityIndex);
            Vec2 hitCanvasPoint = { 0.0f, 0.0f };
            float hitDistance = 0.0f;

            if (!RaycastWorldCanvas(scene, *pCanvas, canvasTransform, ray, hitCanvasPoint, hitDistance))
            {
                continue;
            }

            if (hitDistance >= bestDistance)
            {
                continue;
            }

            bool occluded = false;
            for (int meshIndex = 0; meshIndex < static_cast<int>(scene.m_vEntities.size()); ++meshIndex)
            {
                if (meshIndex == entityIndex)
                {
                    continue;
                }
                const CEntity& meshEntity = scene.m_vEntities[meshIndex];
                if (meshEntity.GetCameraComponent() || meshEntity.GetLightComponent() ||
                    meshEntity.GetCanvasComponent() || meshEntity.HasUiComponent())
                {
                    continue;
                }
                const CMeshComponent* pMesh = meshEntity.GetMeshComponent();
                if (!pMesh || !pMesh->m_Enabled || pMesh->m_Model.meshCount <= 0)
                {
                    continue;
                }

                const Mat4 meshTransform = quark::ComposeMeshWorld(scene, meshEntity);
                const BoundingBox localBounds = GetModelBoundingBox(pMesh->m_Model);
                const Vec3 corners[8] = {
                    meshTransform * Vec3{ localBounds.min.x, localBounds.min.y, localBounds.min.z },
                    meshTransform * Vec3{ localBounds.max.x, localBounds.min.y, localBounds.min.z },
                    meshTransform * Vec3{ localBounds.min.x, localBounds.max.y, localBounds.min.z },
                    meshTransform * Vec3{ localBounds.max.x, localBounds.max.y, localBounds.min.z },
                    meshTransform * Vec3{ localBounds.min.x, localBounds.min.y, localBounds.max.z },
                    meshTransform * Vec3{ localBounds.max.x, localBounds.min.y, localBounds.max.z },
                    meshTransform * Vec3{ localBounds.min.x, localBounds.max.y, localBounds.max.z },
                    meshTransform * Vec3{ localBounds.max.x, localBounds.max.y, localBounds.max.z }
                };
                BoundingBox worldBounds{ corners[0], corners[0] };
                for (int c = 1; c < 8; ++c)
                {
                    worldBounds.min.x = std::min(worldBounds.min.x, corners[c].x);
                    worldBounds.min.y = std::min(worldBounds.min.y, corners[c].y);
                    worldBounds.min.z = std::min(worldBounds.min.z, corners[c].z);
                    worldBounds.max.x = std::max(worldBounds.max.x, corners[c].x);
                    worldBounds.max.y = std::max(worldBounds.max.y, corners[c].y);
                    worldBounds.max.z = std::max(worldBounds.max.z, corners[c].z);
                }

                const RayCollision boxHit = GetRayCollisionBox(ray, worldBounds);
                if (boxHit.hit && boxHit.distance > 0.001f && boxHit.distance < hitDistance - 0.05f)
                {
                    occluded = true;
                    break;
                }
            }

            if (occluded)
            {
                continue;
            }

            std::vector<int> vElementIndices;
            CollectCanvasElementsRecursive(scene, entityIndex, vElementIndices);

            int hitElementIndex = entityIndex;
            for (auto it = vElementIndices.rbegin(); it != vElementIndices.rend(); ++it)
            {
                const int childIndex = *it;
                const CEntity& childEntity = scene.m_vEntities[childIndex];
                const Rectangle rect = ComputeElementCanvasRect(scene, childIndex, { pCanvas->m_Width, pCanvas->m_Height });

                if (hitCanvasPoint.x >= rect.x && hitCanvasPoint.x <= rect.x + rect.width &&
                    hitCanvasPoint.y >= rect.y && hitCanvasPoint.y <= rect.y + rect.height)
                {
                    bool isRaycastTarget = false;
                    if (const auto* pBtn = childEntity.GetButtonComponent())
                    {
                        isRaycastTarget = pBtn->m_RaycastTarget;
                    }
                    else if (const auto* pImg = childEntity.GetImageComponent())
                    {
                        isRaycastTarget = pImg->m_RaycastTarget;
                    }
                    else if (const auto* pPnl = childEntity.GetPanelComponent())
                    {
                        isRaycastTarget = pPnl->m_RaycastTarget;
                    }
                    else if (const auto* pTxt = childEntity.GetTextComponent())
                    {
                        isRaycastTarget = pTxt->m_RaycastTarget;
                    }

                    if (isRaycastTarget)
                    {
                        hitElementIndex = childIndex;
                        break;
                    }
                }
            }

            bestHit.m_CanvasEntityIndex = entityIndex;
            bestHit.m_HitEntityIndex = hitElementIndex;
            bestHit.m_CanvasLocalPoint = hitCanvasPoint;
            bestHit.m_RayDistance = hitDistance;
            bestHit.m_Hit = true;
            bestDistance = hitDistance;
        }

        return bestHit;
    }

    void CCanvasSystem::UpdateInteraction(CScene& scene, const SUiViewport& viewport,
                                          const Camera3D& camera, bool interactive)
    {
        if (!interactive || viewport.m_WindowSize.x <= 0.0f || viewport.m_WindowSize.y <= 0.0f)
        {
            return;
        }

        const Vec2 rawMouse = GetMousePosition();
        if (rawMouse.x < viewport.m_WindowPos.x || rawMouse.x > viewport.m_WindowPos.x + viewport.m_WindowSize.x ||
            rawMouse.y < viewport.m_WindowPos.y || rawMouse.y > viewport.m_WindowPos.y + viewport.m_WindowSize.y)
        {
            return;
        }

        const Ray ray = ScreenToWorldRay(viewport, rawMouse, camera);
        const SCanvasHitResult hit = PickUiElement(scene, ray, camera);

        int currentHovered = -1;
        if (hit.m_Hit && hit.m_HitEntityIndex >= 0 && hit.m_HitEntityIndex < static_cast<int>(scene.m_vEntities.size()))
        {
            const int btnIndex = FindButtonEntityIndex(scene, hit.m_HitEntityIndex);
            currentHovered = (btnIndex >= 0) ? btnIndex : hit.m_HitEntityIndex;
        }

        if (m_HoveredEntityIndex != currentHovered)
        {
            if (m_HoveredEntityIndex >= 0 && m_HoveredEntityIndex < static_cast<int>(scene.m_vEntities.size()))
            {
                if (auto* pBtn = scene.m_vEntities[m_HoveredEntityIndex].GetButtonComponent())
                {
                    pBtn->m_IsHovered = false;
                    pBtn->m_IsPressed = false;
                }
            }
            m_HoveredEntityIndex = currentHovered;
            if (m_HoveredEntityIndex >= 0)
            {
                if (auto* pBtn = scene.m_vEntities[m_HoveredEntityIndex].GetButtonComponent())
                {
                    pBtn->m_IsHovered = true;
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (m_HoveredEntityIndex >= 0)
            {
                m_PressedEntityIndex = m_HoveredEntityIndex;
                if (auto* pBtn = scene.m_vEntities[m_PressedEntityIndex].GetButtonComponent())
                {
                    pBtn->m_IsPressed = true;
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            if (m_PressedEntityIndex >= 0 && m_PressedEntityIndex < static_cast<int>(scene.m_vEntities.size()))
            {
                if (auto* pBtn = scene.m_vEntities[m_PressedEntityIndex].GetButtonComponent())
                {
                    pBtn->m_IsPressed = (m_PressedEntityIndex == m_HoveredEntityIndex);
                }
            }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            if (m_PressedEntityIndex >= 0 && m_PressedEntityIndex < static_cast<int>(scene.m_vEntities.size()))
            {
                auto* pBtn = scene.m_vEntities[m_PressedEntityIndex].GetButtonComponent();
                if (pBtn)
                {
                    pBtn->m_IsPressed = false;
                    if (m_PressedEntityIndex == m_HoveredEntityIndex && pBtn->m_Interactable)
                    {
                        TraceLog(LogLevel::Info, "UI", TextFormat("[Canvas UI] Button '%s' clicked!",
                                 scene.m_vEntities[m_PressedEntityIndex].m_Name.c_str()));

                        if (pBtn->m_fnOnClick)
                        {
                            pBtn->m_fnOnClick();
                        }
                        else
                        {
                            const std::string& name = scene.m_vEntities[m_PressedEntityIndex].m_Name;
                            if (name == "Exit Button" || pBtn->m_OnClickAction == "Exit")
                            {
                                TraceLog(LogLevel::Info, "UI", "[Canvas UI] Exit button clicked. Exiting Play mode.");
                            }
                            else if (name == "Start Button" || pBtn->m_OnClickAction == "Start")
                            {
                                TraceLog(LogLevel::Info, "UI", "[Canvas UI] Start Game button clicked.");
                            }
                            else if (name == "Settings Button" || pBtn->m_OnClickAction == "Settings")
                            {
                                TraceLog(LogLevel::Info, "UI", "[Canvas UI] Settings button clicked.");
                            }
                        }
                    }
                }
            }
            m_PressedEntityIndex = -1;
        }
    }

    void CCanvasSystem::EnsureQuadModel(CCanvasComponent& canvas)
    {
        const RendererType currentBackend = GetCurrentBackend();
        if (canvas.m_QuadModelInitialized && canvas.m_QuadBackend == currentBackend)
        {
            return;
        }

        if (canvas.m_QuadModelInitialized)
        {
            UnloadModel(canvas.m_RuntimeQuadModel);
            canvas.m_RuntimeQuadModel = {};
            canvas.m_QuadModelInitialized = false;
        }

        Mesh mesh = {};
        mesh.vertexCount = 4;
        mesh.triangleCount = 4;

        mesh.vertices = new float[4 * 3]{
            0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
        };

        mesh.normals = new float[4 * 3]{
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f
        };

        const bool isOpenGL = (currentBackend == RendererType::OpenGL);
        if (isOpenGL)
        {
            mesh.texcoords = new float[4 * 2]{
                0.0f, 0.0f,
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f
            };
        }
        else
        {
            mesh.texcoords = new float[4 * 2]{
                0.0f, 1.0f,
                1.0f, 1.0f,
                1.0f, 0.0f,
                0.0f, 0.0f
            };
        }

        mesh.indices = new unsigned short[4 * 3]{
            0, 1, 2,
            0, 2, 3,
            0, 2, 1,
            0, 3, 2
        };

        canvas.m_RuntimeQuadModel = LoadModelFromMesh(mesh);
        if (canvas.m_RuntimeQuadModel.materialCount == 0)
        {
            canvas.m_RuntimeQuadModel.materialCount = 1;
            canvas.m_RuntimeQuadModel.materials = new Material[1];
            canvas.m_RuntimeQuadModel.materials[0] = LoadMaterialDefault();
        }
        canvas.m_RuntimeQuadModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        canvas.m_RuntimeQuadModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
        canvas.m_QuadBackend = currentBackend;
        canvas.m_QuadModelInitialized = true;
    }

    void CCanvasSystem::RenderCanvasElements(CScene& scene, int canvasEntityIndex,
                                            CCanvasComponent& canvas, float canvasWidth, float canvasHeight,
                                            float originX, float originY)
    {
        std::vector<int> vElementIndices;
        CollectCanvasElementsRecursive(scene, canvasEntityIndex, vElementIndices);

        for (int elementIndex : vElementIndices)
        {
            CEntity& entity = scene.m_vEntities[elementIndex];
            if (!entity.GetComponents())
            {
                continue;
            }

            const Rectangle rect = ComputeElementCanvasRect(scene, elementIndex, { canvasWidth, canvasHeight });
            const float screenY = canvasHeight - (rect.y + rect.height);
            const float drawX = originX + rect.x;
            const float drawY = originY + screenY;

            if (auto* pPanel = entity.GetPanelComponent())
            {
                if (pPanel->m_Enabled)
                {
                    DrawRectangleRec(Rectangle{ drawX, drawY, rect.width, rect.height },
                                     pPanel->m_BackgroundColor);
                }
            }

            if (auto* pImage = entity.GetImageComponent())
            {
                if (pImage->m_Enabled)
                {
                    DrawRectangleRec(Rectangle{ drawX, drawY, rect.width, rect.height },
                                     pImage->m_Color);
                }
            }

            if (auto* pBtn = entity.GetButtonComponent())
            {
                if (pBtn->m_Enabled)
                {
                    const Color btnColor = pBtn->GetCurrentColor();
                    DrawRectangleRounded(
                        Rectangle{ drawX, drawY, rect.width, rect.height },
                        0.2f, 4, btnColor);
                    DrawUiButtonRoundedOutline(
                        Rectangle{ drawX, drawY, rect.width, rect.height },
                        0.2f, 1.5f,
                        pBtn->m_IsHovered ? WHITE : Color{ 120, 120, 130, 255 });
                }
            }

            if (auto* pText = entity.GetTextComponent())
            {
                if (pText->m_Enabled && !pText->m_Text.empty())
                {
                    const float fontSize = std::max(8.0f, pText->m_FontSize);
                    const int fontStyle = static_cast<int>(pText->m_FontStyle);
                    const Vec2 textSize = quark::gui::MeasureUiText(
                        pText->m_Text.c_str(), fontSize, pText->m_Font.c_str(), fontStyle);

                    float textX = drawX;
                    if (pText->m_HorizontalAlignment == EUiTextAlignment::CENTER)
                    {
                        textX = drawX + (rect.width - textSize.x) * 0.5f;
                    }
                    else if (pText->m_HorizontalAlignment == EUiTextAlignment::RIGHT)
                    {
                        textX = drawX + rect.width - textSize.x;
                    }

                    float textY = drawY + (rect.height - textSize.y) * 0.5f;
                    if (pText->m_VerticalAlignment == EUiTextVerticalAlignment::TOP)
                    {
                        textY = drawY;
                    }
                    else if (pText->m_VerticalAlignment == EUiTextVerticalAlignment::BOTTOM)
                    {
                        textY = drawY + rect.height - textSize.y;
                    }

                    quark::gui::DrawUiText(
                        pText->m_Text.c_str(),
                        textX,
                        textY,
                        fontSize,
                        pText->m_Font.c_str(),
                        fontStyle,
                        pText->m_Color);
                }
            }
        }
    }

    void CCanvasSystem::UpdateCanvasTextures(CScene& scene)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled)
            {
                continue;
            }

            const int texW = static_cast<int>(std::max(16.0f, pCanvas->m_Width));
            const int texH = static_cast<int>(std::max(16.0f, pCanvas->m_Height));

            if (pCanvas->m_RuntimeTexture.id == 0 ||
                pCanvas->m_RuntimeTexture.texture.width != texW ||
                pCanvas->m_RuntimeTexture.texture.height != texH)
            {
                if (pCanvas->m_RuntimeTexture.id != 0)
                {
                    UnloadRenderTexture(pCanvas->m_RuntimeTexture);
                }
                pCanvas->m_RuntimeTexture = LoadRenderTexture(texW, texH);
                SetTextureFilter(pCanvas->m_RuntimeTexture.texture, TEXTURE_FILTER_BILINEAR);
                pCanvas->m_QuadModelInitialized = false;
            }

            BeginTextureMode(pCanvas->m_RuntimeTexture);
            ClearBackground(Color{ 0, 0, 0, 0 });
            RenderCanvasElements(scene, entityIndex, *pCanvas, pCanvas->m_Width, pCanvas->m_Height);
            EndTextureMode();

            EnsureQuadModel(*pCanvas);
            if (pCanvas->m_RuntimeQuadModel.materialCount > 0 && pCanvas->m_RuntimeQuadModel.materials)
            {
                pCanvas->m_RuntimeQuadModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
                    pCanvas->m_RuntimeTexture.texture;
                pCanvas->m_RuntimeQuadModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture =
                    pCanvas->m_RuntimeTexture.texture;
            }
        }
    }

    void CCanvasSystem::RenderWorldSpaceCanvases(CScene& scene, const Camera3D& camera)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled || pCanvas->m_RenderMode != ECanvasRenderMode::WORLD_SPACE)
            {
                continue;
            }

            if (pCanvas->m_RuntimeTexture.id == 0 || !pCanvas->m_QuadModelInitialized)
            {
                continue;
            }

            const Mat4 worldTransform = GetCanvasWorldMatrix(scene, entityIndex);
            const Vec2 pivot = { 0.5f, 0.5f };
            const Mat4 localScaleMat =
                Mat4::translation(-pCanvas->m_Width * pivot.x, -pCanvas->m_Height * pivot.y, 0.0f) *
                Mat4::scale(pCanvas->m_Width, pCanvas->m_Height, 1.0f);

            DisableBackfaceCulling();
            DrawModelEx(pCanvas->m_RuntimeQuadModel, worldTransform * localScaleMat);
            EnableBackfaceCulling();
        }
    }

    void CCanvasSystem::Render2DCanvases(CScene& scene, const SUiViewport& viewport, int selectedEntityIndex)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled)
            {
                continue;
            }

            const float w = pCanvas->m_Width;
            const float h = pCanvas->m_Height;
            const float canvasX = (viewport.m_WindowSize.x - w) * 0.5f;
            const float canvasY = (viewport.m_WindowSize.y - h) * 0.5f;

            DrawRectangleRec(Rectangle{ canvasX, canvasY, w, h }, Color{ 32, 34, 44, 230 });
            DrawRectangleLinesEx(Rectangle{ canvasX, canvasY, w, h }, 2.0f, Color{ 80, 160, 240, 255 });
            DrawText(TextFormat("Canvas (%dx%d)", static_cast<int>(w), static_cast<int>(h)),
                     static_cast<int>(canvasX + 8.0f), static_cast<int>(canvasY + 6.0f), 14,
                     Color{ 160, 210, 255, 255 });

            RenderCanvasElements(scene, entityIndex, *pCanvas, w, h, canvasX, canvasY);

            if (selectedEntityIndex >= 0 && selectedEntityIndex < static_cast<int>(scene.m_vEntities.size()))
            {
                if (selectedEntityIndex == entityIndex)
                {
                    DrawRectangleLinesEx(Rectangle{ canvasX, canvasY, w, h }, 2.5f, SKYBLUE);
                }
                else if (scene.m_vEntities[selectedEntityIndex].GetRectTransformComponent())
                {
                    const Rectangle elemRect = ComputeElementCanvasRect(scene, selectedEntityIndex, { w, h });
                    const float elemScreenY = canvasY + (h - (elemRect.y + elemRect.height));
                    DrawRectangleLinesEx(Rectangle{ canvasX + elemRect.x, elemScreenY, elemRect.width, elemRect.height }, 2.0f, YELLOW);
                }
            }
        }
    }

    void CCanvasSystem::RenderScreenSpaceCanvases(CScene& scene, const SUiViewport& viewport,
                                                 Vec2 mousePosInViewport, bool interactive)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled || pCanvas->m_RenderMode == ECanvasRenderMode::WORLD_SPACE)
            {
                continue;
            }

            const float screenW = viewport.m_WindowSize.x > 0.0f ? viewport.m_WindowSize.x : 1920.0f;
            const float screenH = viewport.m_WindowSize.y > 0.0f ? viewport.m_WindowSize.y : 1080.0f;

            RenderCanvasElements(scene, entityIndex, *pCanvas, screenW, screenH);
        }
    }

    void CCanvasSystem::DrawCanvasGizmos(CScene& scene, int selectedEntityIndex, const Camera3D& camera)
    {
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled || pCanvas->m_RenderMode != ECanvasRenderMode::WORLD_SPACE)
            {
                continue;
            }

            const bool isCanvasSelected = (selectedEntityIndex == entityIndex);
            bool isDescendantSelected = false;
            if (!isCanvasSelected && selectedEntityIndex >= 0 && selectedEntityIndex < static_cast<int>(scene.m_vEntities.size()))
            {
                int p = scene.m_vEntities[selectedEntityIndex].m_ParentId;
                while (p >= 0)
                {
                    if (p == entityIndex || p == entity.m_Id)
                    {
                        isDescendantSelected = true;
                        break;
                    }
                    if (p < static_cast<int>(scene.m_vEntities.size()))
                    {
                        p = scene.m_vEntities[p].m_ParentId;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            const Mat4 worldTransform = GetCanvasWorldMatrix(scene, entityIndex);
            const Vec2 pivot = { 0.5f, 0.5f };
            const float w = pCanvas->m_Width;
            const float h = pCanvas->m_Height;

            const Vec3 aCorners[4] = {
                worldTransform * Vec3{ -w * pivot.x, -h * pivot.y, 0.0f },
                worldTransform * Vec3{ w * (1.0f - pivot.x), -h * pivot.y, 0.0f },
                worldTransform * Vec3{ w * (1.0f - pivot.x), h * (1.0f - pivot.y), 0.0f },
                worldTransform * Vec3{ -w * pivot.x, h * (1.0f - pivot.y), 0.0f }
            };

            const Color gizmoColor = (isCanvasSelected || isDescendantSelected)
                ? WHITE
                : Color{ 100, 150, 220, 160 };

            for (int i = 0; i < 4; ++i)
            {
                DrawLine3D(aCorners[i], aCorners[(i + 1) % 4], gizmoColor);
            }

            if (isDescendantSelected && scene.m_vEntities[selectedEntityIndex].GetRectTransformComponent())
            {
                const Rectangle elemRect = ComputeElementCanvasRect(scene, selectedEntityIndex, { w, h });
                const Vec3 aElemCorners[4] = {
                    worldTransform * Vec3{ -w * pivot.x + elemRect.x, -h * pivot.y + elemRect.y, 0.002f },
                    worldTransform * Vec3{ -w * pivot.x + elemRect.x + elemRect.width, -h * pivot.y + elemRect.y, 0.002f },
                    worldTransform * Vec3{ -w * pivot.x + elemRect.x + elemRect.width, -h * pivot.y + elemRect.y + elemRect.height, 0.002f },
                    worldTransform * Vec3{ -w * pivot.x + elemRect.x, -h * pivot.y + elemRect.y + elemRect.height, 0.002f }
                };

                for (int i = 0; i < 4; ++i)
                {
                    DrawLine3D(aElemCorners[i], aElemCorners[(i + 1) % 4], YELLOW);
                }
            }
        }
    }

    Vec2 CCanvasSystem::ScreenToCanvas(const CCanvasComponent& canvas, const Mat4& canvasWorldMatrix,
                                      const Camera3D& camera, const SUiViewport& viewport,
                                      const Vec2& screenPoint)
    {
        const Ray ray = ScreenToWorldRay(viewport, screenPoint, camera);
        Vec2 canvasPoint = { 0.0f, 0.0f };
        float dist = 0.0f;
        CScene dummyScene;
        RaycastWorldCanvas(dummyScene, canvas, canvasWorldMatrix, ray, canvasPoint, dist);
        return canvasPoint;
    }

    Vec3 CCanvasSystem::CanvasToWorld(const CCanvasComponent& canvas, const Mat4& canvasWorldMatrix,
                                     const Vec2& canvasPoint)
    {
        const Vec2 pivot = { 0.5f, 0.5f };
        const Vec3 localPoint = {
            canvasPoint.x - canvas.m_Width * pivot.x,
            canvasPoint.y - canvas.m_Height * pivot.y,
            0.0f
        };
        return canvasWorldMatrix * localPoint;
    }

    Vec2 CCanvasSystem::WorldToCanvas(const CCanvasComponent& canvas, const Mat4& canvasWorldMatrix,
                                     const Vec3& worldPoint)
    {
        const Vec3 localPoint = WorldToCanvasLocal(canvasWorldMatrix, worldPoint);
        const Vec2 pivot = { 0.5f, 0.5f };
        return {
            localPoint.x + canvas.m_Width * pivot.x,
            localPoint.y + canvas.m_Height * pivot.y
        };
    }

} // quark::ui
