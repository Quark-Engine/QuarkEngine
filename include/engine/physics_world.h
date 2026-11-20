#ifndef __ENGINE_PHYSICS_WORLD_H__
#define __ENGINE_PHYSICS_WORLD_H__

#include "QuarkCore/QuarkCore.hpp"
#include "scene.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace quark::physics
{
    enum class EPhysicsEventType
    {
        COLLISION_ENTER = 0,
        COLLISION_STAY = 1,
        COLLISION_EXIT = 2,
        TRIGGER_ENTER = 3,
        TRIGGER_STAY = 4,
        TRIGGER_EXIT = 5
    };

    struct SPhysicsEvent
    {
        EPhysicsEventType m_Type = EPhysicsEventType::COLLISION_ENTER;
        int m_EntityA = -1;
        int m_EntityB = -1;
        Vec3 m_Point = { 0.0f, 0.0f, 0.0f };
        Vec3 m_Normal = { 0.0f, 1.0f, 0.0f };
        float m_Penetration = 0.0f;
    };

    struct SPhysicsRaycastHit
    {
        bool m_Hit = false;
        int m_EntityId = -1;
        Vec3 m_Point = { 0.0f, 0.0f, 0.0f };
        Vec3 m_Normal = { 0.0f, 1.0f, 0.0f };
        float m_Distance = 0.0f;
    };

    struct SContactManifold
    {
        Vec3 m_Point = { 0.0f, 0.0f, 0.0f };
        Vec3 m_Normal = { 0.0f, 1.0f, 0.0f };
        float m_Penetration = 0.0f;
        bool m_HasContact = false;
    };

    class CPhysicsWorld
    {
    public:
        void Reset();
        void Step(CScene& scene, float fixedDeltaTime, std::vector<SPhysicsEvent>& vOutEvents);

        bool Raycast(const CScene& scene,
                     const Vec3& origin,
                     const Vec3& direction,
                     float maxDistance,
                     SPhysicsRaycastHit& outHit,
                     bool includeTriggers = false,
                     int ignoreEntityId = -1) const;

        void SetGravity(const Vec3& gravity)
        {
            m_Gravity = gravity;
        }

        Vec3 GetGravity() const
        {
            return m_Gravity;
        }

    private:
        struct SWorldCollider
        {
            int m_EntityIndex = -1;
            int m_EntityId = -1;
            EColliderType m_Type = COLLIDER_BOX;
            bool m_IsTrigger = false;
            Vec3 m_Center = { 0.0f, 0.0f, 0.0f };
            Vec3 m_BoxMin = { 0.0f, 0.0f, 0.0f };
            Vec3 m_BoxMax = { 0.0f, 0.0f, 0.0f };
            Vec3 m_SegmentA = { 0.0f, 0.0f, 0.0f };
            Vec3 m_SegmentB = { 0.0f, 0.0f, 0.0f };
            float m_Radius = 0.5f;
            BoundingBox m_BroadphaseBounds{};
        };

        static SWorldCollider BuildWorldCollider(const CScene& scene, int entityIndex,
                                                 const CCollisionComponent& collider);
        static bool OverlapsAabb(const BoundingBox& a, const BoundingBox& b);
        static SContactManifold TestColliders(const SWorldCollider& a, const SWorldCollider& b);

        Vec3 m_Gravity = { 0.0f, -9.81f, 0.0f };
        std::unordered_set<std::uint64_t> m_PreviousCollisions;
        std::unordered_set<std::uint64_t> m_PreviousTriggers;
        std::vector<SWorldCollider> m_vWorldColliders;
    };
} // quark::physics

#endif // __ENGINE_PHYSICS_WORLD_H__
