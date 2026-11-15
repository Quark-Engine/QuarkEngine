#ifndef __CANVAS_SYSTEM_H__
#define __CANVAS_SYSTEM_H__

#include "QuarkCore/QuarkCore.hpp"
#include "component.h"
#include "scene.h"

#include <cfloat>
#include <string>
#include <vector>

class CViewportState;

namespace quark::ui
{

    struct SUiViewport
    {
        Vec2 m_WindowPos = { 0.0f, 0.0f };
        Vec2 m_WindowSize = { 0.0f, 0.0f };

        SUiViewport() = default;
        SUiViewport(const Vec2& pos, const Vec2& size) : m_WindowPos(pos), m_WindowSize(size) {}
        SUiViewport(float x, float y, float w, float h) : m_WindowPos{ x, y }, m_WindowSize{ w, h } {}
        SUiViewport(const CViewportState& viewport);
    };

    struct SCanvasHitResult
    {
        int m_CanvasEntityIndex = -1;
        int m_HitEntityIndex = -1;
        Vec2 m_CanvasLocalPoint = { 0.0f, 0.0f };
        float m_RayDistance = FLT_MAX;
        bool m_Hit = false;
    };

    class CCanvasSystem
    {
    public:
        static CCanvasSystem& Get();

        void Initialize();
        void Shutdown();

        Rectangle ComputeElementCanvasRect(const CScene& scene, int entityIndex,
                                           const Vec2& canvasSize) const;
        Mat4 ComputeElementCanvasTransform(const CScene& scene, int entityIndex, const Vec2& canvasSize) const;
        float ComputeScreenSpaceCanvasScale(const CCanvasComponent& canvas,
                                            const Vec2& viewportSize) const;
        Mat4 GetCanvasWorldMatrix(const CScene& scene, int canvasEntityIndex);

        bool RaycastWorldCanvas(const CScene& scene, const CCanvasComponent& canvas,
                                const Mat4& canvasWorldTransform, const Ray& ray,
                                Vec2& outCanvasPoint, float& outDistance);

        SCanvasHitResult PickUiElement(CScene& scene, const Ray& ray,
                                       const Camera3D& camera, float maxDistance = 1000.0f);
        SCanvasHitResult PickScreenSpaceUiElement(CScene& scene, const SUiViewport& viewport,
                                                  const Vec2& screenPoint);

        void UpdateInteraction(CScene& scene, const SUiViewport& viewport,
                               const Camera3D& camera, bool interactive);

        void UpdateCanvasTextures(CScene& scene);

        void RenderWorldSpaceCanvases(CScene& scene, const Camera3D& camera);
        void Render2DCanvases(CScene& scene, const SUiViewport& viewport, int selectedEntityIndex);
        void RenderScreenSpaceCanvases(CScene& scene, const SUiViewport& viewport,
                                      Vec2 mousePosInViewport, bool interactive,
                                      const Camera3D& activeCamera,
                                      EUiCanvasViewport viewportKind);

        void DrawCanvasGizmos(CScene& scene, int selectedEntityIndex, const Camera3D& camera);
        int FindCanvasForEntity(const CScene& scene, int entityIndex) const;

        Vec2 ScreenToCanvas(const CCanvasComponent& canvas, const Mat4& canvasWorldMatrix,
                            const Camera3D& camera, const SUiViewport& viewport,
                            const Vec2& screenPoint);
        Vec3 CanvasToWorld(const CCanvasComponent& canvas, const Mat4& canvasWorldMatrix,
                           const Vec2& canvasPoint);
        Vec2 WorldToCanvas(const CCanvasComponent& canvas, const Mat4& canvasWorldMatrix,
                           const Vec3& worldPoint);

    private:
        CCanvasSystem() = default;
        ~CCanvasSystem() = default;

        void RenderCanvasElements(CScene& scene, int canvasEntityIndex,
                                  CCanvasComponent& canvas, float canvasWidth, float canvasHeight,
                                  float originX = 0.0f, float originY = 0.0f,
                                  float scaleX = 1.0f, float scaleY = 1.0f);
        bool HasCanvasChildren(const CScene& scene, int canvasEntityIndex) const;
        void DispatchButtonEvent(CScene& scene, int entityIndex, EUiEventType event);
        void SetSelectedButton(CScene& scene, int entityIndex);
        int FindNavigableButton(const CScene& scene, int entityIndex, int direction,
                                const Vec2& canvasSize) const;
        bool IsPointInsideClipHierarchy(const CScene& scene, int entityIndex,
                                        int canvasEntityIndex, const Vec2& canvasSize,
                                        const Vec2& canvasPoint) const;
        void EnsureQuadModel(CCanvasComponent& canvas);
        void CollectCanvasElementsRecursive(const CScene& scene, int parentIndex,
                                            std::vector<int>& vOutIndices);
        Mat4 ComputeElementCanvasTransformRecursive(const CScene& scene, int entityIndex,
                                                    const Vec2& canvasSize, int depth) const;

        int m_HoveredEntityIndex = -1;
        int m_PressedEntityIndex = -1;
        int m_SelectedEntityIndex = -1;
        int m_ActiveCanvasIndex = -1;
        Vec2 m_PointerDownPosition = { 0.0f, 0.0f };
        bool m_DragStarted = false;
        bool m_Initialized = false;
    };

} // quark::ui

#endif // __CANVAS_SYSTEM_H__
