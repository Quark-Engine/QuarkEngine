#include "engine/canvas_system.h"

#include "editor/editor_state.h"
#include "editor/editor_utils.h"
#include "engine/transform.h"
#include "gui/gui_renderer_bridge.h"
#include "models.h"
#include "scripting/script_component.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

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

    const char* GetUiEventName(EUiEventType event)
    {
        switch (event)
        {
            case EUiEventType::POINTER_ENTER: return "pointer_enter";
            case EUiEventType::POINTER_EXIT: return "pointer_exit";
            case EUiEventType::POINTER_DOWN: return "pointer_down";
            case EUiEventType::POINTER_UP: return "pointer_up";
            case EUiEventType::CLICK: return "click";
            case EUiEventType::BEGIN_DRAG: return "begin_drag";
            case EUiEventType::DRAG: return "drag";
            case EUiEventType::END_DRAG: return "end_drag";
            case EUiEventType::DROP: return "drop";
            case EUiEventType::SELECT: return "select";
            case EUiEventType::DESELECT: return "deselect";
            case EUiEventType::SUBMIT: return "submit";
        }
        return "unknown";
    }

    bool TransformCanvasPointToLocal(const Mat4& transform, const Vec2& point, Vec2& outLocalPoint)
    {
        const float matrixA = transform.m[0];
        const float matrixB = transform.m[4];
        const float matrixC = transform.m[1];
        const float matrixD = transform.m[5];
        const float determinant = matrixA * matrixD - matrixB * matrixC;
        if (std::fabs(determinant) <= 1e-8f)
        {
            return false;
        }

        const float deltaX = point.x - transform.m[12];
        const float deltaY = point.y - transform.m[13];
        const float inverseDeterminant = 1.0f / determinant;
        outLocalPoint = {
            (matrixD * deltaX - matrixB * deltaY) * inverseDeterminant,
            (matrixA * deltaY - matrixC * deltaX) * inverseDeterminant
        };
        return true;
    }

    Vec2 TransformCanvasPointToScreen(const Mat4& transform, const Vec2& point,
                                      float canvasHeight, float originX, float originY)
    {
        const Vec3 transformed = transform * Vec3{ point.x, point.y, 0.0f };
        return {
            originX + transformed.x,
            originY + canvasHeight - transformed.y
        };
    }

    Rectangle TransformCanvasRectToScreenBounds(const Mat4& transform, const Rectangle& rect,
                                                 float canvasHeight, float originX, float originY)
    {
        const Vec2 aCorners[4] = {
            TransformCanvasPointToScreen(transform, { 0.0f, 0.0f }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { rect.width, 0.0f }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { rect.width, rect.height }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { 0.0f, rect.height }, canvasHeight, originX, originY)
        };
        float minX = aCorners[0].x;
        float minY = aCorners[0].y;
        float maxX = aCorners[0].x;
        float maxY = aCorners[0].y;
        for (int cornerIndex = 1; cornerIndex < 4; ++cornerIndex)
        {
            minX = std::min(minX, aCorners[cornerIndex].x);
            minY = std::min(minY, aCorners[cornerIndex].y);
            maxX = std::max(maxX, aCorners[cornerIndex].x);
            maxY = std::max(maxY, aCorners[cornerIndex].y);
        }

        return { minX, minY, maxX - minX, maxY - minY };
    }

    Rectangle TransformCanvasRectToCanvasBounds(const Mat4& transform, const Rectangle& rect,
                                                 float canvasHeight)
    {
        Rectangle bounds = TransformCanvasRectToScreenBounds(
            transform, rect, canvasHeight, 0.0f, 0.0f);
        bounds.y = canvasHeight - bounds.y - bounds.height;
        return bounds;
    }

    void DrawCanvasRect(const Mat4& transform, const Rectangle& rect, float canvasHeight,
                        float originX, float originY, Color color,
                        bool rounded = false, float roundness = 0.0f,
                        bool pixelPerfect = false)
    {
        Vec2 aCorners[4] = {
            TransformCanvasPointToScreen(transform, { 0.0f, 0.0f }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { rect.width, 0.0f }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { rect.width, rect.height }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { 0.0f, rect.height }, canvasHeight, originX, originY)
        };
        if (pixelPerfect)
        {
            for (Vec2& corner : aCorners)
            {
                corner.x = std::round(corner.x);
                corner.y = std::round(corner.y);
            }
        }
        const bool axisAligned = std::fabs(transform.m[1]) <= 1e-5f &&
            std::fabs(transform.m[4]) <= 1e-5f;

        if (axisAligned)
        {
            float minX = aCorners[0].x;
            float minY = aCorners[0].y;
            float maxX = aCorners[0].x;
            float maxY = aCorners[0].y;
            for (int cornerIndex = 1; cornerIndex < 4; ++cornerIndex)
            {
                minX = std::min(minX, aCorners[cornerIndex].x);
                minY = std::min(minY, aCorners[cornerIndex].y);
                maxX = std::max(maxX, aCorners[cornerIndex].x);
                maxY = std::max(maxY, aCorners[cornerIndex].y);
            }

            const Rectangle bounds = { minX, minY, maxX - minX, maxY - minY };
            if (rounded)
            {
                DrawRectangleRounded(bounds, roundness, 4, color);
            }
            else
            {
                DrawRectangleRec(bounds, color);
            }
            return;
        }

        DrawTriangle(aCorners[0], aCorners[1], aCorners[2], color);
        DrawTriangle(aCorners[0], aCorners[2], aCorners[3], color);
    }

    void DrawCanvasRectOutline(const Mat4& transform, const Rectangle& rect,
                               float canvasHeight, float originX, float originY,
                               float thickness, Color color, bool rounded = false,
                               bool pixelPerfect = false)
    {
        Vec2 aCorners[4] = {
            TransformCanvasPointToScreen(transform, { 0.0f, 0.0f }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { rect.width, 0.0f }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { rect.width, rect.height }, canvasHeight, originX, originY),
            TransformCanvasPointToScreen(transform, { 0.0f, rect.height }, canvasHeight, originX, originY)
        };
        if (pixelPerfect)
        {
            for (Vec2& corner : aCorners)
            {
                corner.x = std::round(corner.x);
                corner.y = std::round(corner.y);
            }
        }
        const bool axisAligned = std::fabs(transform.m[1]) <= 1e-5f &&
            std::fabs(transform.m[4]) <= 1e-5f;

        if (axisAligned && rounded)
        {
            float minX = aCorners[0].x;
            float minY = aCorners[0].y;
            float maxX = aCorners[0].x;
            float maxY = aCorners[0].y;
            for (int cornerIndex = 1; cornerIndex < 4; ++cornerIndex)
            {
                minX = std::min(minX, aCorners[cornerIndex].x);
                minY = std::min(minY, aCorners[cornerIndex].y);
                maxX = std::max(maxX, aCorners[cornerIndex].x);
                maxY = std::max(maxY, aCorners[cornerIndex].y);
            }
            DrawUiButtonRoundedOutline(
                Rectangle{ minX, minY, maxX - minX, maxY - minY },
                0.2f, thickness, color);
            return;
        }

        for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
        {
            DrawLineEx(aCorners[cornerIndex], aCorners[(cornerIndex + 1) % 4],
                       thickness, color);
        }
    }

    bool EnsureUiTexture(Texture2D& runtimeTexture, bool& textureLoaded,
                         std::string& loadedTexturePath, const std::string& texturePath)
    {
        if (loadedTexturePath == texturePath)
        {
            return textureLoaded && runtimeTexture.id != 0;
        }

        if (runtimeTexture.id != 0)
        {
            UnloadTexture(runtimeTexture);
            runtimeTexture = {};
        }
        textureLoaded = false;
        loadedTexturePath = texturePath;
        if (texturePath.empty())
        {
            return false;
        }

        runtimeTexture = LoadTexture(texturePath.c_str());
        if (runtimeTexture.id == 0)
        {
            TraceLog(LogLevel::Error, "UI",
                     TextFormat("[Canvas UI] Failed to load image texture: %s",
                                texturePath.c_str()));
            return false;
        }

        textureLoaded = true;
        return true;
    }

    void DrawCanvasTextureRegion(const Mat4& transform, float localX, float localY,
                                 float localWidth, float localHeight,
                                 float canvasHeight, float originX, float originY,
                                 const Texture2D& texture, const Rectangle& source,
                                 Color tint, bool pixelPerfect)
    {
        if (texture.id == 0 || localWidth <= 0.0f || localHeight <= 0.0f ||
            source.width <= 0.0f || source.height <= 0.0f)
        {
            return;
        }

        const float scaleX = std::sqrt(
            transform.m[0] * transform.m[0] + transform.m[1] * transform.m[1]);
        const float scaleY = std::sqrt(
            transform.m[4] * transform.m[4] + transform.m[5] * transform.m[5]);
        if (scaleX <= 0.000001f || scaleY <= 0.000001f)
        {
            return;
        }

        const Vec2 localCenter = {
            localX + localWidth * 0.5f,
            localY + localHeight * 0.5f
        };
        Vec2 screenCenter = TransformCanvasPointToScreen(
            transform, localCenter, canvasHeight, originX, originY);
        if (pixelPerfect)
        {
            screenCenter.x = std::round(screenCenter.x);
            screenCenter.y = std::round(screenCenter.y);
        }

        const float screenWidth = localWidth * scaleX;
        const float screenHeight = localHeight * scaleY;
        const Rectangle destination = {
            screenCenter.x - screenWidth * 0.5f,
            screenCenter.y - screenHeight * 0.5f,
            screenWidth,
            screenHeight
        };
        const Vec2 rotationPivot = { screenWidth * 0.5f, screenHeight * 0.5f };
        const float rotation = std::atan2(-transform.m[1], transform.m[0]) / DEG2RAD;
        DrawTexturePro(texture, source, destination, rotationPivot, rotation, tint);
    }

    void DrawCanvasTexture(const Mat4& transform, const Rectangle& rect,
                           float canvasHeight, float originX, float originY,
                           const Texture2D& texture, Color tint,
                           EUiImageType imageType, bool preserveAspect,
                           const std::array<float, 4>& border, bool fillCenter,
                           float fillAmount, bool pixelPerfect)
    {
        if (texture.id == 0 || texture.width <= 0 || texture.height <= 0 ||
            rect.width <= 0.0f || rect.height <= 0.0f)
        {
            return;
        }

        const float textureWidth = static_cast<float>(texture.width);
        const float textureHeight = static_cast<float>(texture.height);
        if (imageType == EUiImageType::SLICED)
        {
            const float leftSource = std::clamp(border[0], 0.0f, textureWidth * 0.5f);
            const float topSource = std::clamp(border[1], 0.0f, textureHeight * 0.5f);
            const float rightSource = std::clamp(border[2], 0.0f, textureWidth * 0.5f);
            const float bottomSource = std::clamp(border[3], 0.0f, textureHeight * 0.5f);
            const float horizontalBorderScale =
                leftSource + rightSource > rect.width
                ? rect.width / (leftSource + rightSource)
                : 1.0f;
            const float verticalBorderScale =
                topSource + bottomSource > rect.height
                ? rect.height / (topSource + bottomSource)
                : 1.0f;
            const float destinationX[4] = {
                0.0f,
                leftSource * horizontalBorderScale,
                rect.width - rightSource * horizontalBorderScale,
                rect.width
            };
            const float destinationY[4] = {
                0.0f,
                topSource * verticalBorderScale,
                rect.height - bottomSource * verticalBorderScale,
                rect.height
            };
            const float sourceX[4] = {
                0.0f, leftSource, textureWidth - rightSource, textureWidth
            };
            const float sourceY[4] = {
                0.0f, topSource, textureHeight - bottomSource, textureHeight
            };

            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 3; ++column)
                {
                    if (!fillCenter && row == 1 && column == 1)
                    {
                        continue;
                    }

                    const float localWidth = destinationX[column + 1] - destinationX[column];
                    const float localHeight = destinationY[row + 1] - destinationY[row];
                    const Rectangle source = {
                        sourceX[column],
                        sourceY[row],
                        sourceX[column + 1] - sourceX[column],
                        sourceY[row + 1] - sourceY[row]
                    };
                    DrawCanvasTextureRegion(
                        transform, destinationX[column], destinationY[row],
                        localWidth, localHeight, canvasHeight, originX, originY,
                        texture, source, tint, pixelPerfect);
                }
            }
            return;
        }

        if (imageType == EUiImageType::TILED)
        {
            for (float localY = 0.0f; localY < rect.height; localY += textureHeight)
            {
                const float tileHeight = std::min(textureHeight, rect.height - localY);
                for (float localX = 0.0f; localX < rect.width; localX += textureWidth)
                {
                    const float tileWidth = std::min(textureWidth, rect.width - localX);
                    DrawCanvasTextureRegion(
                        transform, localX, localY, tileWidth, tileHeight,
                        canvasHeight, originX, originY, texture,
                        { 0.0f, 0.0f, tileWidth, tileHeight }, tint, pixelPerfect);
                }
            }
            return;
        }

        float contentWidth = rect.width;
        float contentHeight = rect.height;
        float contentX = 0.0f;
        float contentY = 0.0f;
        if (preserveAspect)
        {
            const float aspectScale = std::min(rect.width / textureWidth, rect.height / textureHeight);
            contentWidth = textureWidth * aspectScale;
            contentHeight = textureHeight * aspectScale;
            contentX = (rect.width - contentWidth) * 0.5f;
            contentY = (rect.height - contentHeight) * 0.5f;
        }

        const float amount = imageType == EUiImageType::FILLED
            ? std::clamp(fillAmount, 0.0f, 1.0f)
            : 1.0f;
        DrawCanvasTextureRegion(
            transform, contentX, contentY, contentWidth * amount, contentHeight,
            canvasHeight, originX, originY, texture,
            { 0.0f, 0.0f, textureWidth * amount, textureHeight },
            tint, pixelPerfect);
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
        m_SelectedEntityIndex = -1;
        m_ActiveCanvasIndex = -1;
        m_PointerDownPosition = { 0.0f, 0.0f };
        m_DragStarted = false;
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

    Rectangle CCanvasSystem::ComputeElementCanvasRect(const CScene& scene, int entityIndex,
                                                      const Vec2& canvasSize) const
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

    Mat4 CCanvasSystem::ComputeElementCanvasTransform(const CScene& scene, int entityIndex,
                                                      const Vec2& canvasSize) const
    {
        return ComputeElementCanvasTransformRecursive(scene, entityIndex, canvasSize, 0);
    }

    Mat4 CCanvasSystem::ComputeElementCanvasTransformRecursive(const CScene& scene, int entityIndex,
                                                               const Vec2& canvasSize, int depth) const
    {
        if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()) ||
            depth >= 64)
        {
            return Mat4::identity();
        }

        const CEntity& entity = scene.m_vEntities[entityIndex];
        const CUiRectTransformComponent* pRectTransform = entity.GetRectTransformComponent();
        Vec2 parentSize = canvasSize;
        Mat4 parentTransform = Mat4::identity();

        if (entity.m_ParentId >= 0)
        {
            int parentIndex = entity.m_ParentId < static_cast<int>(scene.m_vEntities.size())
                ? entity.m_ParentId
                : FindEntityIndexById(scene, entity.m_ParentId);
            if (parentIndex >= 0 && parentIndex < static_cast<int>(scene.m_vEntities.size()) &&
                !scene.m_vEntities[parentIndex].GetCanvasComponent())
            {
                const Rectangle parentRect = ComputeElementCanvasRect(scene, parentIndex, canvasSize);
                parentSize = { parentRect.width, parentRect.height };
                parentTransform = ComputeElementCanvasTransformRecursive(
                    scene, parentIndex, canvasSize, depth + 1);
            }
        }

        const Rectangle localRect = pRectTransform
            ? pRectTransform->CalculateLocalRect(parentSize)
            : Rectangle{ 0.0f, 0.0f, parentSize.x, parentSize.y };
        const Vec2 pivot = pRectTransform
            ? Vec2{ localRect.width * pRectTransform->m_Pivot.x,
                    localRect.height * pRectTransform->m_Pivot.y }
            : Vec2{ 0.0f, 0.0f };
        const float rotation = pRectTransform ? pRectTransform->m_Rotation2D * DEG2RAD : 0.0f;
        const Vec2 scale = pRectTransform ? pRectTransform->m_Scale2D : Vec2{ 1.0f, 1.0f };

        return parentTransform *
            Mat4::translation(localRect.x, localRect.y, 0.0f) *
            Mat4::translation(pivot.x, pivot.y, 0.0f) *
            Mat4::rotationZ(rotation) *
            Mat4::scale(scale.x, scale.y, 1.0f) *
            Mat4::translation(-pivot.x, -pivot.y, 0.0f);
    }

    float CCanvasSystem::ComputeScreenSpaceCanvasScale(const CCanvasComponent& canvas,
                                                       const Vec2& viewportSize) const
    {
        const float referenceWidth = std::max(1.0f, canvas.m_ReferenceResolution.x);
        const float referenceHeight = std::max(1.0f, canvas.m_ReferenceResolution.y);
        const float scaleWidth = std::max(0.001f, viewportSize.x / referenceWidth);
        const float scaleHeight = std::max(0.001f, viewportSize.y / referenceHeight);
        const float match = std::clamp(canvas.m_MatchWidthOrHeight, 0.0f, 1.0f);
        const float scale = std::exp(
            std::log(scaleWidth) * (1.0f - match) +
            std::log(scaleHeight) * match);
        return std::max(0.001f, scale * std::max(0.001f, canvas.m_ScaleFactor));
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
            if (!HasCanvasChildren(scene, entityIndex))
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
                const Mat4 elementTransform = ComputeElementCanvasTransform(
                    scene, childIndex, { pCanvas->m_Width, pCanvas->m_Height });
                Vec2 localPoint = { 0.0f, 0.0f };
                if (IsPointInsideClipHierarchy(
                        scene, childIndex, entityIndex, { pCanvas->m_Width, pCanvas->m_Height },
                        hitCanvasPoint) &&
                    TransformCanvasPointToLocal(elementTransform, hitCanvasPoint, localPoint) &&
                    localPoint.x >= 0.0f && localPoint.x <= rect.width &&
                    localPoint.y >= 0.0f && localPoint.y <= rect.height)
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

    SCanvasHitResult CCanvasSystem::PickScreenSpaceUiElement(CScene& scene,
                                                             const SUiViewport& viewport,
                                                             const Vec2& screenPoint)
    {
        SCanvasHitResult bestHit;
        int bestSortingOrder = std::numeric_limits<int>::min();

        for (int canvasIndex = 0; canvasIndex < static_cast<int>(scene.m_vEntities.size()); ++canvasIndex)
        {
            CEntity& canvasEntity = scene.m_vEntities[canvasIndex];
            CCanvasComponent* pCanvas = canvasEntity.GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled ||
                pCanvas->m_RenderMode == ECanvasRenderMode::WORLD_SPACE)
            {
                continue;
            }
            if (!HasCanvasChildren(scene, canvasIndex))
            {
                continue;
            }

            const float scale = ComputeScreenSpaceCanvasScale(*pCanvas, viewport.m_WindowSize);
            const Vec2 canvasSize = {
                viewport.m_WindowSize.x / scale,
                viewport.m_WindowSize.y / scale
            };
            const Vec2 canvasPoint = {
                (screenPoint.x - viewport.m_WindowPos.x) / scale,
                (viewport.m_WindowSize.y - (screenPoint.y - viewport.m_WindowPos.y)) / scale
            };
            std::vector<int> vElementIndices;
            CollectCanvasElementsRecursive(scene, canvasIndex, vElementIndices);

            int hitElementIndex = -1;
            for (auto it = vElementIndices.rbegin(); it != vElementIndices.rend(); ++it)
            {
                const int elementIndex = *it;
                const CEntity& element = scene.m_vEntities[elementIndex];
                const Rectangle rect = ComputeElementCanvasRect(scene, elementIndex, canvasSize);
                const Mat4 elementTransform = ComputeElementCanvasTransform(
                    scene, elementIndex, canvasSize);
                Vec2 localPoint = { 0.0f, 0.0f };
                if (!IsPointInsideClipHierarchy(
                        scene, elementIndex, canvasIndex, canvasSize, canvasPoint) ||
                    !TransformCanvasPointToLocal(elementTransform, canvasPoint, localPoint) ||
                    localPoint.x < 0.0f || localPoint.x > rect.width ||
                    localPoint.y < 0.0f || localPoint.y > rect.height)
                {
                    continue;
                }

                const CButtonComponent* pButton = element.GetButtonComponent();
                const CImageComponent* pImage = element.GetImageComponent();
                const CPanelComponent* pPanel = element.GetPanelComponent();
                const CTextComponent* pText = element.GetTextComponent();
                const bool raycastTarget =
                    (pButton && pButton->m_Enabled && pButton->m_RaycastTarget) ||
                    (pImage && pImage->m_Enabled && pImage->m_RaycastTarget) ||
                    (pPanel && pPanel->m_Enabled && pPanel->m_RaycastTarget) ||
                    (pText && pText->m_Enabled && pText->m_RaycastTarget);
                if (raycastTarget)
                {
                    hitElementIndex = elementIndex;
                    break;
                }
            }

            if (hitElementIndex < 0 || pCanvas->m_SortingOrder < bestSortingOrder)
            {
                continue;
            }

            bestHit.m_CanvasEntityIndex = canvasIndex;
            bestHit.m_HitEntityIndex = hitElementIndex;
            bestHit.m_CanvasLocalPoint = canvasPoint;
            bestHit.m_RayDistance = 0.0f;
            bestHit.m_Hit = true;
            bestSortingOrder = pCanvas->m_SortingOrder;
        }

        return bestHit;
    }

    void CCanvasSystem::DispatchButtonEvent(CScene& scene, int entityIndex, EUiEventType event)
    {
        if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return;
        }

        CEntity& entity = scene.m_vEntities[entityIndex];
        CButtonComponent* pButton = entity.GetButtonComponent();
        if (!pButton || !pButton->m_Enabled || !pButton->m_Interactable)
        {
            return;
        }

        CScriptComponent* pScript = entity.GetScriptComponent();
        bool actionDispatched = false;
        for (const SUiEventCallback& callback : pButton->m_vEventCallbacks)
        {
            if (callback.m_Event != event || callback.m_Action.empty())
            {
                continue;
            }

            if (pScript && pScript->m_Enabled && pScript->m_pInstance)
            {
                pScript->m_pInstance->OnUIEvent(GetUiEventName(event), callback.m_Action);
                actionDispatched = true;
            }
            else
            {
                TraceLog(
                    LogLevel::Warn, "UI",
                    TextFormat("UI action '%s' on '%s' has no active script to receive '%s'",
                               callback.m_Action.c_str(), entity.m_Name.c_str(),
                               GetUiEventName(event)));
            }
        }

        if (event == EUiEventType::CLICK && pButton->m_OnClickAction.size() > 0)
        {
            const bool hasClickBinding = std::any_of(
                pButton->m_vEventCallbacks.begin(), pButton->m_vEventCallbacks.end(),
                [](const SUiEventCallback& callback)
                {
                    return callback.m_Event == EUiEventType::CLICK && !callback.m_Action.empty();
                });
            if (!hasClickBinding && pScript && pScript->m_Enabled && pScript->m_pInstance)
            {
                pScript->m_pInstance->OnUIEvent("click", pButton->m_OnClickAction);
                actionDispatched = true;
            }
            else if (!hasClickBinding && !actionDispatched)
            {
                TraceLog(
                    LogLevel::Warn, "UI",
                    TextFormat("On Click Action '%s' on '%s' has no active script",
                               pButton->m_OnClickAction.c_str(), entity.m_Name.c_str()));
            }
        }

        if (event == EUiEventType::CLICK && pButton->m_fnOnClick)
        {
            pButton->m_fnOnClick();
        }
    }

    void CCanvasSystem::SetSelectedButton(CScene& scene, int entityIndex)
    {
        if (entityIndex >= 0 && entityIndex < static_cast<int>(scene.m_vEntities.size()))
        {
            const CButtonComponent* pButton = scene.m_vEntities[entityIndex].GetButtonComponent();
            if (!pButton || !pButton->m_Enabled || !pButton->m_Interactable)
            {
                entityIndex = -1;
            }
        }
        else
        {
            entityIndex = -1;
        }

        if (m_SelectedEntityIndex == entityIndex)
        {
            return;
        }

        if (m_SelectedEntityIndex >= 0 &&
            m_SelectedEntityIndex < static_cast<int>(scene.m_vEntities.size()))
        {
            if (CButtonComponent* pButton =
                    scene.m_vEntities[m_SelectedEntityIndex].GetButtonComponent())
            {
                pButton->m_IsSelected = false;
                DispatchButtonEvent(scene, m_SelectedEntityIndex, EUiEventType::DESELECT);
            }
        }

        m_SelectedEntityIndex = entityIndex;
        if (m_SelectedEntityIndex >= 0)
        {
            if (CButtonComponent* pButton =
                    scene.m_vEntities[m_SelectedEntityIndex].GetButtonComponent())
            {
                pButton->m_IsSelected = true;
                DispatchButtonEvent(scene, m_SelectedEntityIndex, EUiEventType::SELECT);
            }
        }
    }

    int CCanvasSystem::FindNavigableButton(const CScene& scene, int entityIndex, int direction,
                                          const Vec2& canvasSize) const
    {
        if (direction < 0 || direction >= 4)
        {
            return -1;
        }

        const int canvasEntityIndex = entityIndex >= 0
            ? FindCanvasForEntity(scene, entityIndex)
            : -1;
        const CButtonComponent* pCurrentButton = entityIndex >= 0 &&
            entityIndex < static_cast<int>(scene.m_vEntities.size())
            ? scene.m_vEntities[entityIndex].GetButtonComponent()
            : nullptr;

        if (pCurrentButton && pCurrentButton->m_Navigation == EUiNavigation::NONE)
        {
            return -1;
        }
        if (pCurrentButton && pCurrentButton->m_Navigation == EUiNavigation::HORIZONTAL &&
            (direction == 0 || direction == 2))
        {
            return -1;
        }
        if (pCurrentButton && pCurrentButton->m_Navigation == EUiNavigation::VERTICAL &&
            (direction == 1 || direction == 3))
        {
            return -1;
        }
        if (pCurrentButton && pCurrentButton->m_Navigation == EUiNavigation::EXPLICIT)
        {
            const int targetIndex = FindEntityIndexById(
                scene, pCurrentButton->m_aNavigationTargetIds[direction]);
            if (targetIndex < 0 || FindCanvasForEntity(scene, targetIndex) != canvasEntityIndex)
            {
                return -1;
            }
            const CButtonComponent* pTargetButton =
                scene.m_vEntities[targetIndex].GetButtonComponent();
            return pTargetButton && pTargetButton->m_Enabled && pTargetButton->m_Interactable
                ? targetIndex
                : -1;
        }

        if (entityIndex < 0)
        {
            for (int candidateIndex = 0;
                 candidateIndex < static_cast<int>(scene.m_vEntities.size()); ++candidateIndex)
            {
                const CButtonComponent* pButton =
                    scene.m_vEntities[candidateIndex].GetButtonComponent();
                if (pButton && pButton->m_Enabled && pButton->m_Interactable &&
                    pButton->m_Navigation != EUiNavigation::NONE)
                {
                    return candidateIndex;
                }
            }
            return -1;
        }

        static constexpr float aDirectionX[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
        static constexpr float aDirectionY[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
        const Rectangle currentRect = ComputeElementCanvasRect(scene, entityIndex, canvasSize);
        const Mat4 currentTransform =
            ComputeElementCanvasTransform(scene, entityIndex, canvasSize);
        const Vec3 currentCenter = currentTransform *
            Vec3{ currentRect.width * 0.5f, currentRect.height * 0.5f, 0.0f };
        int bestIndex = -1;
        float bestScore = std::numeric_limits<float>::max();

        for (int candidateIndex = 0;
             candidateIndex < static_cast<int>(scene.m_vEntities.size()); ++candidateIndex)
        {
            if (candidateIndex == entityIndex ||
                FindCanvasForEntity(scene, candidateIndex) != canvasEntityIndex)
            {
                continue;
            }

            const CButtonComponent* pButton =
                scene.m_vEntities[candidateIndex].GetButtonComponent();
            if (!pButton || !pButton->m_Enabled || !pButton->m_Interactable ||
                pButton->m_Navigation == EUiNavigation::NONE)
            {
                continue;
            }

            const Rectangle rect =
                ComputeElementCanvasRect(scene, candidateIndex, canvasSize);
            const Mat4 transform =
                ComputeElementCanvasTransform(scene, candidateIndex, canvasSize);
            const Vec3 center = transform * Vec3{ rect.width * 0.5f, rect.height * 0.5f, 0.0f };
            const float deltaX = center.x - currentCenter.x;
            const float deltaY = center.y - currentCenter.y;
            const float forwardDistance =
                deltaX * aDirectionX[direction] + deltaY * aDirectionY[direction];
            if (forwardDistance <= 0.0f)
            {
                continue;
            }

            const float lateralDistance = std::fabs(
                deltaX * aDirectionY[direction] - deltaY * aDirectionX[direction]);
            const float score = forwardDistance + lateralDistance * 2.0f;
            if (score < bestScore)
            {
                bestScore = score;
                bestIndex = candidateIndex;
            }
        }

        return bestIndex;
    }

    bool CCanvasSystem::IsPointInsideClipHierarchy(
        const CScene& scene, int entityIndex, int canvasEntityIndex,
        const Vec2& canvasSize, const Vec2& canvasPoint) const
    {
        if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return false;
        }

        const int parentId = scene.m_vEntities[entityIndex].m_ParentId;
        int ancestorIndex = parentId >= 0 &&
            parentId < static_cast<int>(scene.m_vEntities.size())
            ? parentId
            : FindEntityIndexById(scene, parentId);
        int depth = 0;
        while (ancestorIndex >= 0 &&
               ancestorIndex < static_cast<int>(scene.m_vEntities.size()) &&
               depth++ < 64)
        {
            const CEntity& ancestor = scene.m_vEntities[ancestorIndex];
            const CUiRectTransformComponent* pRectTransform =
                ancestor.GetRectTransformComponent();
            if (pRectTransform && pRectTransform->m_ClipChildren)
            {
                const Rectangle rect =
                    ComputeElementCanvasRect(scene, ancestorIndex, canvasSize);
                const Mat4 transform =
                    ComputeElementCanvasTransform(scene, ancestorIndex, canvasSize);
                const Rectangle clipBounds =
                    TransformCanvasRectToCanvasBounds(transform, rect, canvasSize.y);
                if (canvasPoint.x < clipBounds.x ||
                    canvasPoint.x > clipBounds.x + clipBounds.width ||
                    canvasPoint.y < clipBounds.y ||
                    canvasPoint.y > clipBounds.y + clipBounds.height)
                {
                    return false;
                }
            }

            if (ancestorIndex == canvasEntityIndex)
            {
                break;
            }

            const int parentId = ancestor.m_ParentId;
            ancestorIndex = parentId >= 0 &&
                parentId < static_cast<int>(scene.m_vEntities.size())
                ? parentId
                : FindEntityIndexById(scene, parentId);
        }

        return true;
    }

    void CCanvasSystem::UpdateInteraction(CScene& scene, const SUiViewport& viewport,
                                          const Camera3D& camera, bool interactive)
    {
        if (!interactive || viewport.m_WindowSize.x <= 0.0f || viewport.m_WindowSize.y <= 0.0f)
        {
            return;
        }

        const CButtonComponent* pSelectedButton =
            m_SelectedEntityIndex >= 0 &&
            m_SelectedEntityIndex < static_cast<int>(scene.m_vEntities.size())
            ? scene.m_vEntities[m_SelectedEntityIndex].GetButtonComponent()
            : nullptr;
        if (m_SelectedEntityIndex >= 0 &&
            (!pSelectedButton || !pSelectedButton->m_Enabled || !pSelectedButton->m_Interactable))
        {
            SetSelectedButton(scene, -1);
        }

        const Vec2 rawMouse = GetMousePosition();
        const bool mouseInside =
            rawMouse.x >= viewport.m_WindowPos.x &&
            rawMouse.x <= viewport.m_WindowPos.x + viewport.m_WindowSize.x &&
            rawMouse.y >= viewport.m_WindowPos.y &&
            rawMouse.y <= viewport.m_WindowPos.y + viewport.m_WindowSize.y;
        SCanvasHitResult hit;
        if (mouseInside)
        {
            hit = PickScreenSpaceUiElement(scene, viewport, rawMouse);
            if (!hit.m_Hit)
            {
                const Ray ray = ScreenToWorldRay(viewport, rawMouse, camera);
                hit = PickUiElement(scene, ray, camera);
            }
        }

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
                    DispatchButtonEvent(scene, m_HoveredEntityIndex, EUiEventType::POINTER_EXIT);
                }
            }
            m_HoveredEntityIndex = currentHovered;
            if (m_HoveredEntityIndex >= 0)
            {
                if (auto* pBtn = scene.m_vEntities[m_HoveredEntityIndex].GetButtonComponent())
                {
                    pBtn->m_IsHovered = true;
                    DispatchButtonEvent(scene, m_HoveredEntityIndex, EUiEventType::POINTER_ENTER);
                }
            }
        }

        const bool gamepadAvailable = IsGamepadAvailable(0);
        const bool moveUp = IsKeyPressed(KEY_UP) ||
            (gamepadAvailable && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP));
        const bool moveRight = IsKeyPressed(KEY_RIGHT) ||
            (gamepadAvailable && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT));
        const bool moveDown = IsKeyPressed(KEY_DOWN) ||
            (gamepadAvailable && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN));
        const bool moveLeft = IsKeyPressed(KEY_LEFT) ||
            (gamepadAvailable && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT));
        int navigationDirection = moveUp ? 0 : (moveRight ? 1 : (moveDown ? 2 : (moveLeft ? 3 : -1)));
        if (navigationDirection >= 0)
        {
            Vec2 canvasSize = { 800.0f, 600.0f };
            const int canvasEntityIndex =
                FindCanvasForEntity(scene, m_SelectedEntityIndex);
            if (canvasEntityIndex >= 0)
            {
                if (const CCanvasComponent* pCanvas =
                        scene.m_vEntities[canvasEntityIndex].GetCanvasComponent())
                {
                    canvasSize = { pCanvas->m_Width, pCanvas->m_Height };
                    if (pCanvas->m_RenderMode != ECanvasRenderMode::WORLD_SPACE)
                    {
                        const float scale =
                            ComputeScreenSpaceCanvasScale(*pCanvas, viewport.m_WindowSize);
                        canvasSize = {
                            viewport.m_WindowSize.x / scale,
                            viewport.m_WindowSize.y / scale
                        };
                    }
                }
            }

            const int nextIndex = FindNavigableButton(
                scene, m_SelectedEntityIndex, navigationDirection, canvasSize);
            if (nextIndex >= 0)
            {
                SetSelectedButton(scene, nextIndex);
            }
        }

        const bool submitPressed =
            IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            (gamepadAvailable &&
             IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
        if (submitPressed)
        {
            int submitIndex = m_SelectedEntityIndex;
            if (submitIndex < 0)
            {
                submitIndex = FindNavigableButton(scene, -1, 0, { 800.0f, 600.0f });
                SetSelectedButton(scene, submitIndex);
            }
            if (submitIndex >= 0)
            {
                DispatchButtonEvent(scene, submitIndex, EUiEventType::SUBMIT);
                DispatchButtonEvent(scene, submitIndex, EUiEventType::CLICK);
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (m_HoveredEntityIndex >= 0)
            {
                m_PressedEntityIndex = m_HoveredEntityIndex;
                if (auto* pBtn = scene.m_vEntities[m_PressedEntityIndex].GetButtonComponent())
                {
                    SetSelectedButton(scene, m_PressedEntityIndex);
                    pBtn->m_IsPressed = true;
                    m_PointerDownPosition = rawMouse;
                    m_DragStarted = false;
                    DispatchButtonEvent(scene, m_PressedEntityIndex, EUiEventType::POINTER_DOWN);
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
                    const float deltaX = rawMouse.x - m_PointerDownPosition.x;
                    const float deltaY = rawMouse.y - m_PointerDownPosition.y;
                    if (!m_DragStarted && deltaX * deltaX + deltaY * deltaY >= 25.0f)
                    {
                        m_DragStarted = true;
                        DispatchButtonEvent(scene, m_PressedEntityIndex, EUiEventType::BEGIN_DRAG);
                    }
                    if (m_DragStarted)
                    {
                        DispatchButtonEvent(scene, m_PressedEntityIndex, EUiEventType::DRAG);
                    }
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
                    DispatchButtonEvent(scene, m_PressedEntityIndex, EUiEventType::POINTER_UP);
                    if (m_DragStarted)
                    {
                        DispatchButtonEvent(scene, m_PressedEntityIndex, EUiEventType::END_DRAG);
                        if (m_HoveredEntityIndex >= 0 &&
                            m_HoveredEntityIndex != m_PressedEntityIndex)
                        {
                            DispatchButtonEvent(scene, m_HoveredEntityIndex, EUiEventType::DROP);
                        }
                    }
                    if (m_PressedEntityIndex == m_HoveredEntityIndex && pBtn->m_Interactable)
                    {
                        DispatchButtonEvent(scene, m_PressedEntityIndex, EUiEventType::CLICK);
                    }
                }
            }
            m_PressedEntityIndex = -1;
            m_DragStarted = false;
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
                                            float originX, float originY,
                                            float scaleX, float scaleY)
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
            const Mat4 elementTransform =
                Mat4::scale(scaleX, scaleY, 1.0f) *
                ComputeElementCanvasTransform(scene, elementIndex, { canvasWidth, canvasHeight });
            const float renderedCanvasHeight = canvasHeight * scaleY;
            Rectangle clipRect = {};
            bool hasClipRect = false;
            int ancestorIndex = entity.m_ParentId >= 0 &&
                entity.m_ParentId < static_cast<int>(scene.m_vEntities.size())
                ? entity.m_ParentId
                : FindEntityIndexById(scene, entity.m_ParentId);
            int ancestorDepth = 0;
            while (ancestorIndex >= 0 &&
                   ancestorIndex < static_cast<int>(scene.m_vEntities.size()) &&
                   ancestorDepth++ < 64)
            {
                const CEntity& ancestor = scene.m_vEntities[ancestorIndex];
                const CUiRectTransformComponent* pAncestorRect =
                    ancestor.GetRectTransformComponent();
                if (pAncestorRect && pAncestorRect->m_ClipChildren)
                {
                    const Rectangle ancestorRect = ComputeElementCanvasRect(
                        scene, ancestorIndex, { canvasWidth, canvasHeight });
                    const Mat4 ancestorTransform =
                        Mat4::scale(scaleX, scaleY, 1.0f) *
                        ComputeElementCanvasTransform(
                            scene, ancestorIndex, { canvasWidth, canvasHeight });
                    const Rectangle ancestorClip = TransformCanvasRectToScreenBounds(
                        ancestorTransform, ancestorRect, renderedCanvasHeight, originX, originY);
                    if (!hasClipRect)
                    {
                        clipRect = ancestorClip;
                        hasClipRect = true;
                    }
                    else
                    {
                        const float minX = std::max(clipRect.x, ancestorClip.x);
                        const float minY = std::max(clipRect.y, ancestorClip.y);
                        const float maxX = std::min(
                            clipRect.x + clipRect.width,
                            ancestorClip.x + ancestorClip.width);
                        const float maxY = std::min(
                            clipRect.y + clipRect.height,
                            ancestorClip.y + ancestorClip.height);
                        clipRect = {
                            minX, minY, std::max(0.0f, maxX - minX),
                            std::max(0.0f, maxY - minY)
                        };
                    }
                }

                if (ancestorIndex == canvasEntityIndex)
                {
                    break;
                }

                const int parentId = ancestor.m_ParentId;
                ancestorIndex = parentId >= 0 &&
                    parentId < static_cast<int>(scene.m_vEntities.size())
                    ? parentId
                    : FindEntityIndexById(scene, parentId);
            }

            if (hasClipRect && (clipRect.width <= 0.0f || clipRect.height <= 0.0f))
            {
                continue;
            }
            if (hasClipRect)
            {
                BeginScissorMode(
                    static_cast<int>(std::floor(clipRect.x)),
                    static_cast<int>(std::floor(clipRect.y)),
                    static_cast<int>(std::ceil(clipRect.width)),
                    static_cast<int>(std::ceil(clipRect.height)));
            }

            if (auto* pPanel = entity.GetPanelComponent())
            {
                if (pPanel->m_Enabled)
                {
                    if (EnsureUiTexture(
                            pPanel->m_RuntimeTexture, pPanel->m_TextureLoaded,
                            pPanel->m_LoadedTexturePath, pPanel->m_BackgroundImage))
                    {
                        DrawCanvasTexture(
                            elementTransform, rect, renderedCanvasHeight, originX, originY,
                            pPanel->m_RuntimeTexture, pPanel->m_BackgroundColor,
                            EUiImageType::SIMPLE, false, {}, true,
                            1.0f, canvas.m_PixelPerfect);
                    }
                    else
                    {
                        DrawCanvasRect(
                            elementTransform, rect, renderedCanvasHeight, originX, originY,
                            pPanel->m_BackgroundColor, false, 0.0f,
                            canvas.m_PixelPerfect);
                    }
                }
            }

            if (auto* pImage = entity.GetImageComponent())
            {
                if (pImage->m_Enabled)
                {
                    if (EnsureUiTexture(
                            pImage->m_RuntimeTexture, pImage->m_TextureLoaded,
                            pImage->m_LoadedTexturePath, pImage->m_SourceTexture))
                    {
                        DrawCanvasTexture(
                            elementTransform, rect, renderedCanvasHeight, originX, originY,
                            pImage->m_RuntimeTexture, pImage->m_Color,
                            pImage->m_ImageType, pImage->m_PreserveAspect,
                            pImage->m_aBorder, pImage->m_FillCenter, pImage->m_FillAmount,
                            canvas.m_PixelPerfect);
                    }
                    else
                    {
                        DrawCanvasRect(
                            elementTransform, rect, renderedCanvasHeight, originX, originY,
                            pImage->m_Color, false, 0.0f, canvas.m_PixelPerfect);
                    }
                }
            }

            if (auto* pBtn = entity.GetButtonComponent())
            {
                if (pBtn->m_Enabled)
                {
                    const Color btnColor = pBtn->GetCurrentColor();
                    DrawCanvasRect(elementTransform, rect, renderedCanvasHeight, originX, originY,
                                   btnColor, true, 0.2f, canvas.m_PixelPerfect);
                    DrawCanvasRectOutline(
                        elementTransform, rect, renderedCanvasHeight, originX, originY,
                        1.5f, pBtn->m_IsHovered ? WHITE : Color{ 120, 120, 130, 255 },
                        true, canvas.m_PixelPerfect);
                }
            }

            if (hasClipRect)
            {
                EndScissorMode();
            }

            if (auto* pText = entity.GetTextComponent())
            {
                if (pText->m_Enabled && !pText->m_Text.empty())
                {
                    SGuiWidgetDesc textWidget;
                    textWidget.m_WidgetId = static_cast<uint64_t>(entity.m_Id);
                    textWidget.m_WidgetType = EGuiWidgetType::TEXT;
                    textWidget.m_Text = pText->m_Text;
                    textWidget.m_TextFont = pText->m_Font;
                    textWidget.m_TextFontStyle = static_cast<int>(pText->m_FontStyle);
                    textWidget.m_TextFontSize = std::max(1.0f, pText->m_FontSize);
                    textWidget.m_TextLineSpacing = pText->m_LineSpacing;
                    textWidget.m_TextCharacterSpacing = pText->m_CharacterSpacing;
                    textWidget.m_TextRichText = pText->m_RichText;
                    textWidget.m_TextColor = pText->m_Color;
                    textWidget.m_TextHorizontalAlignment =
                        static_cast<int>(pText->m_HorizontalAlignment);
                    textWidget.m_TextVerticalAlignment =
                        static_cast<int>(pText->m_VerticalAlignment);
                    textWidget.m_TextHorizontalOverflow =
                        static_cast<int>(pText->m_HorizontalOverflow);
                    textWidget.m_TextVerticalOverflow =
                        static_cast<int>(pText->m_VerticalOverflow);
                    textWidget.m_TextBestFit = pText->m_BestFit;
                    textWidget.m_TextMaterial = pText->m_Material;
                    textWidget.m_UseTextClip = hasClipRect;
                    textWidget.m_TextClipRect = clipRect;

                    Vec2 screenTextPosition = TransformCanvasPointToScreen(
                        elementTransform, { 0.0f, rect.height },
                        renderedCanvasHeight, originX, originY);
                    if (canvas.m_PixelPerfect)
                    {
                        screenTextPosition.x = std::round(screenTextPosition.x);
                        screenTextPosition.y = std::round(screenTextPosition.y);
                    }
                    const float textScaleX = std::sqrt(
                        elementTransform.m[0] * elementTransform.m[0] +
                        elementTransform.m[1] * elementTransform.m[1]);
                    const float textScaleY = std::sqrt(
                        elementTransform.m[4] * elementTransform.m[4] +
                        elementTransform.m[5] * elementTransform.m[5]);
                    const float rotation = std::atan2(
                        -elementTransform.m[1], elementTransform.m[0]) / DEG2RAD;
                    textWidget.m_Position = screenTextPosition;
                    textWidget.m_Size = {
                        rect.width * textScaleX,
                        rect.height * textScaleY
                    };
                    textWidget.m_TextFontSize *= textScaleY;
                    textWidget.m_TextCharacterSpacing *= textScaleX;
                    textWidget.m_TextRotation = rotation;

                    std::vector<SGuiWidgetDesc*> vTextWidgets = { &textWidget };
                    quark::gui::CGuiSystem::Get().RenderOnly(vTextWidgets);
                }
            }
        }
    }

    bool CCanvasSystem::HasCanvasChildren(const CScene& scene, int canvasEntityIndex) const
    {
        if (canvasEntityIndex < 0 ||
            canvasEntityIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return false;
        }

        const int canvasId = scene.m_vEntities[canvasEntityIndex].m_Id;
        for (const CEntity& entity : scene.m_vEntities)
        {
            if (entity.m_ParentId == canvasEntityIndex ||
                (canvasId != canvasEntityIndex && entity.m_ParentId == canvasId))
            {
                return true;
            }
        }

        return false;
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
            if (pCanvas->m_RenderMode == ECanvasRenderMode::SCREEN_SPACE_CAMERA)
            {
                continue;
            }

            if (!HasCanvasChildren(scene, entityIndex))
            {
                pCanvas->ReleaseOwnedResources();
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
                                                 Vec2 mousePosInViewport, bool interactive,
                                                 const Camera3D& activeCamera,
                                                 EUiCanvasViewport viewportKind)
    {
        std::vector<int> vCanvasIndices;
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CCanvasComponent* pCanvas = scene.m_vEntities[entityIndex].GetCanvasComponent();
            if (!pCanvas || !pCanvas->m_Enabled || pCanvas->m_RenderMode == ECanvasRenderMode::WORLD_SPACE)
            {
                continue;
            }
            if (!HasCanvasChildren(scene, entityIndex))
            {
                pCanvas->ReleaseOwnedResources();
                continue;
            }

            vCanvasIndices.push_back(entityIndex);
        }

        std::stable_sort(vCanvasIndices.begin(), vCanvasIndices.end(),
                         [&scene](int firstIndex, int secondIndex)
        {
            return scene.m_vEntities[firstIndex].GetCanvasComponent()->m_SortingOrder <
                scene.m_vEntities[secondIndex].GetCanvasComponent()->m_SortingOrder;
        });

        for (int entityIndex : vCanvasIndices)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CCanvasComponent* pCanvas = entity.GetCanvasComponent();
            const float screenW = viewport.m_WindowSize.x > 0.0f ? viewport.m_WindowSize.x : 1920.0f;
            const float screenH = viewport.m_WindowSize.y > 0.0f ? viewport.m_WindowSize.y : 1080.0f;
            const float scale = ComputeScreenSpaceCanvasScale(*pCanvas, { screenW, screenH });
            const float canvasWidth = screenW / scale;
            const float canvasHeight = screenH / scale;

            if (pCanvas->m_RenderMode == ECanvasRenderMode::SCREEN_SPACE_OVERLAY)
            {
                RenderCanvasElements(
                    scene, entityIndex, *pCanvas, canvasWidth, canvasHeight,
                    0.0f, 0.0f, scale, scale);
                continue;
            }

            const int viewportIndex = static_cast<int>(viewportKind);
            if (viewportIndex < 0 ||
                viewportIndex >= static_cast<int>(pCanvas->m_aScreenSpaceTextures.size()))
            {
                continue;
            }

            RenderTexture2D& screenSpaceTexture =
                pCanvas->m_aScreenSpaceTextures[viewportIndex];
            const int textureWidth = std::max(1, static_cast<int>(std::round(screenW)));
            const int textureHeight = std::max(1, static_cast<int>(std::round(screenH)));
            if (screenSpaceTexture.id == 0 ||
                screenSpaceTexture.texture.width != textureWidth ||
                screenSpaceTexture.texture.height != textureHeight)
            {
                if (screenSpaceTexture.id != 0)
                {
                    UnloadRenderTexture(screenSpaceTexture);
                }
                screenSpaceTexture = LoadRenderTexture(textureWidth, textureHeight);
                SetTextureFilter(
                    screenSpaceTexture.texture,
                    pCanvas->m_PixelPerfect ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
            }

            if (!IsRenderTextureValid(screenSpaceTexture))
            {
                TraceLog(LogLevel::Error, "UI", "[Canvas UI] Failed to create the camera-space canvas render texture.");
                continue;
            }

            BeginTextureMode(screenSpaceTexture);
            ClearBackground(Color{ 0, 0, 0, 0 });
            RenderCanvasElements(
                scene, entityIndex, *pCanvas, canvasWidth, canvasHeight,
                0.0f, 0.0f, scale, scale);
            EndTextureMode();

            EnsureQuadModel(*pCanvas);
            if (pCanvas->m_RuntimeQuadModel.materialCount <= 0 ||
                !pCanvas->m_RuntimeQuadModel.materials)
            {
                continue;
            }
            pCanvas->m_RuntimeQuadModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
                screenSpaceTexture.texture;
            pCanvas->m_RuntimeQuadModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture =
                screenSpaceTexture.texture;

            Camera3D canvasCamera = activeCamera;
            const int renderCameraIndex = FindEntityIndexById(
                scene, pCanvas->m_RenderCameraEntityId);
            if (renderCameraIndex >= 0)
            {
                const CEntity& cameraEntity = scene.m_vEntities[renderCameraIndex];
                const CCameraComponent* pRenderCamera = cameraEntity.GetCameraComponent();
                if (pRenderCamera && pRenderCamera->m_Enabled &&
                    cameraEntity.GetTransformComponent())
                {
                    const Mat4 renderCameraWorld = quark::ComposeWorld(scene, renderCameraIndex);
                    canvasCamera.position =
                        renderCameraWorld * Vec3{ 0.0f, 0.0f, 0.0f };
                    canvasCamera.target =
                        renderCameraWorld * Vec3{ 0.0f, 0.0f, -1.0f };
                    canvasCamera.up =
                        renderCameraWorld * Vec3{ 0.0f, 1.0f, 0.0f } -
                        canvasCamera.position;
                    canvasCamera.fovy = pRenderCamera->m_FieldOfView;
                    canvasCamera.projection = CAMERA_PERSPECTIVE;
                }
            }

            Vec3 cameraForward = canvasCamera.target - canvasCamera.position;
            Vec3 cameraUp = canvasCamera.up;
            if (cameraForward.length() <= 0.000001f || cameraUp.length() <= 0.000001f)
            {
                continue;
            }
            cameraForward = cameraForward.normalized();
            cameraUp = cameraUp.normalized();
            Vec3 cameraRight = cameraForward.cross(cameraUp);
            if (cameraRight.length() <= 0.000001f)
            {
                continue;
            }
            cameraRight = cameraRight.normalized();
            cameraUp = cameraRight.cross(cameraForward).normalized();

            Mat4 cameraWorld = Mat4::identity();
            cameraWorld.m[0] = cameraRight.x;
            cameraWorld.m[1] = cameraRight.y;
            cameraWorld.m[2] = cameraRight.z;
            cameraWorld.m[4] = cameraUp.x;
            cameraWorld.m[5] = cameraUp.y;
            cameraWorld.m[6] = cameraUp.z;
            cameraWorld.m[8] = -cameraForward.x;
            cameraWorld.m[9] = -cameraForward.y;
            cameraWorld.m[10] = -cameraForward.z;
            cameraWorld.m[12] = canvasCamera.position.x;
            cameraWorld.m[13] = canvasCamera.position.y;
            cameraWorld.m[14] = canvasCamera.position.z;

            const float planeDistance = std::max(0.1f, pCanvas->m_PlaneDistance);
            const float planeHeight =
                2.0f * planeDistance * std::tan(canvasCamera.fovy * DEG2RAD * 0.5f);
            const float planeWidth = planeHeight * screenW / screenH;
            const Mat4 planeTransform =
                cameraWorld *
                Mat4::translation(0.0f, 0.0f, -planeDistance) *
                Mat4::translation(-planeWidth * 0.5f, -planeHeight * 0.5f, 0.0f) *
                Mat4::scale(planeWidth, planeHeight, 1.0f);

            BeginMode3D(canvasCamera);
            DisableBackfaceCulling();
            DrawModelEx(pCanvas->m_RuntimeQuadModel, planeTransform);
            EnableBackfaceCulling();
            EndMode3D();
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
