#ifndef __EDITOR_EDITOR_SCENE_PICKER_H__
#define __EDITOR_EDITOR_SCENE_PICKER_H__

#include "QuarkCore/QuarkCore.hpp"
#include "../scene.h"
#include "editor_state.h"

class CScenePicker
{
public:
    static Vec3 RayPlaneHit(Ray ray);

    static Vec2 WorldToScreen(const CViewportState& viewport, const Vec3& world,
        const Camera3D& camera);
    static Ray ScreenToWorldRay(const CViewportState& viewport, const Vec2& mouse,
        const Camera3D& camera);

    static bool RaycastEntity(const CScene& scene, const CEntity& entity, Ray ray,
        float& outDistance);
    static bool PickMeshTriangle(const CScene& scene, const CEntity& entity, int meshIndex,
        Ray ray, int& outTriangleIndex, int& outVertexCorner);
};

#endif // __EDITOR_EDITOR_SCENE_PICKER_H__
