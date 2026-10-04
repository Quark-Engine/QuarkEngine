#ifndef __EDITOR_EDITOR_SCENE_PICKER_H__
#define __EDITOR_EDITOR_SCENE_PICKER_H__

#include "QuarkCore/QuarkCore.hpp"
#include "../scene.h"
#include "editor_state.h"

class CScenePicker
{
public:
    static qc::Vec3 RayPlaneHit(qc::Ray ray);

    static qc::Vec2 WorldToScreen(const CViewportState& viewport, const qc::Vec3& world,
        const qc::Camera3D& camera);
    static qc::Ray ScreenToWorldRay(const CViewportState& viewport, const qc::Vec2& mouse,
        const qc::Camera3D& camera);

    static bool RaycastEntity(const CScene& scene, const CEntity& entity, qc::Ray ray,
        float& outDistance);
    static bool PickMeshTriangle(const CScene& scene, const CEntity& entity, int meshIndex,
        qc::Ray ray, int& outTriangleIndex, int& outVertexCorner);
};

#endif // __EDITOR_EDITOR_SCENE_PICKER_H__
