#include "engine/physics_world.h"
#include "engine/transform.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace quark::physics
{
    namespace
    {
        constexpr float PHYSICS_EPSILON = 1e-6f;

        std::uint64_t MakePairKey(int idA, int idB)
        {
            const std::uint32_t lo = static_cast<std::uint32_t>(std::min(idA, idB));
            const std::uint32_t hi = static_cast<std::uint32_t>(std::max(idA, idB));
            return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
        }

        void UnpackPairKey(std::uint64_t key, int& outIdA, int& outIdB)
        {
            outIdA = static_cast<int>(key >> 32);
            outIdB = static_cast<int>(key & 0xFFFFFFFFu);
        }

        Vec3 ClampPointToBox(const Vec3& point, const Vec3& boxMin, const Vec3& boxMax)
        {
            return {
                std::clamp(point.x, boxMin.x, boxMax.x),
                std::clamp(point.y, boxMin.y, boxMax.y),
                std::clamp(point.z, boxMin.z, boxMax.z)
            };
        }

        Vec3 ClosestPointOnSegment(const Vec3& p, const Vec3& a, const Vec3& b)
        {
            const Vec3 ab = b - a;
            const float lenSq = ab.lengthSquared();
            if (lenSq <= PHYSICS_EPSILON)
            {
                return a;
            }
            const float t = std::clamp((p - a).dot(ab) / lenSq, 0.0f, 1.0f);
            return a + ab * t;
        }

        void ClosestPointsBetweenSegments(const Vec3& p1, const Vec3& q1,
                                          const Vec3& p2, const Vec3& q2,
                                          Vec3& outC1, Vec3& outC2)
        {
            const Vec3 d1 = q1 - p1;
            const Vec3 d2 = q2 - p2;
            const Vec3 r = p1 - p2;
            const float a = d1.dot(d1);
            const float e = d2.dot(d2);
            const float f = d2.dot(r);

            float s = 0.0f;
            float t = 0.0f;

            if (a <= PHYSICS_EPSILON && e <= PHYSICS_EPSILON)
            {
                outC1 = p1;
                outC2 = p2;
                return;
            }

            if (a <= PHYSICS_EPSILON)
            {
                t = std::clamp(f / e, 0.0f, 1.0f);
            }
            else
            {
                const float c = d1.dot(r);
                if (e <= PHYSICS_EPSILON)
                {
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                }
                else
                {
                    const float b = d1.dot(d2);
                    const float denom = a * e - b * b;
                    if (denom != 0.0f)
                    {
                        s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
                    }
                    t = (b * s + f) / e;
                    if (t < 0.0f)
                    {
                        t = 0.0f;
                        s = std::clamp(-c / a, 0.0f, 1.0f);
                    }
                    else if (t > 1.0f)
                    {
                        t = 1.0f;
                        s = std::clamp((b - c) / a, 0.0f, 1.0f);
                    }
                }
            }

            outC1 = p1 + d1 * s;
            outC2 = p2 + d2 * t;
        }

        SContactManifold TestSphereSphere(const Vec3& centerA, float radiusA,
                                          const Vec3& centerB, float radiusB)
        {
            SContactManifold manifold;
            const Vec3 delta = centerB - centerA;
            const float distSq = delta.lengthSquared();
            const float radiusSum = radiusA + radiusB;
            if (distSq >= radiusSum * radiusSum)
            {
                return manifold;
            }

            const float dist = std::sqrt(distSq);
            if (dist > PHYSICS_EPSILON)
            {
                manifold.m_Normal = delta * (1.0f / dist);
                manifold.m_Penetration = radiusSum - dist;
                manifold.m_Point = centerA + manifold.m_Normal * (radiusA - manifold.m_Penetration * 0.5f);
            }
            else
            {
                manifold.m_Normal = { 0.0f, 1.0f, 0.0f };
                manifold.m_Penetration = radiusSum;
                manifold.m_Point = centerA;
            }
            manifold.m_HasContact = true;
            return manifold;
        }

        SContactManifold TestBoxBox(const Vec3& minA, const Vec3& maxA,
                                    const Vec3& minB, const Vec3& maxB)
        {
            SContactManifold manifold;
            if (maxA.x <= minB.x || maxB.x <= minA.x ||
                maxA.y <= minB.y || maxB.y <= minA.y ||
                maxA.z <= minB.z || maxB.z <= minA.z)
            {
                return manifold;
            }

            const float penPosX = maxB.x - minA.x;
            const float penNegX = maxA.x - minB.x;
            const float sepX = std::min(penPosX, penNegX);
            const float signX = (penNegX <= penPosX) ? 1.0f : -1.0f;

            const float penPosY = maxB.y - minA.y;
            const float penNegY = maxA.y - minB.y;
            const float sepY = std::min(penPosY, penNegY);
            const float signY = (penNegY <= penPosY) ? 1.0f : -1.0f;

            const float penPosZ = maxB.z - minA.z;
            const float penNegZ = maxA.z - minB.z;
            const float sepZ = std::min(penPosZ, penNegZ);
            const float signZ = (penNegZ <= penPosZ) ? 1.0f : -1.0f;

            if (sepX <= sepY && sepX <= sepZ)
            {
                manifold.m_Normal = { signX, 0.0f, 0.0f };
                manifold.m_Penetration = sepX;
            }
            else if (sepY <= sepX && sepY <= sepZ)
            {
                manifold.m_Normal = { 0.0f, signY, 0.0f };
                manifold.m_Penetration = sepY;
            }
            else
            {
                manifold.m_Normal = { 0.0f, 0.0f, signZ };
                manifold.m_Penetration = sepZ;
            }

            manifold.m_Point = {
                (std::max(minA.x, minB.x) + std::min(maxA.x, maxB.x)) * 0.5f,
                (std::max(minA.y, minB.y) + std::min(maxA.y, maxB.y)) * 0.5f,
                (std::max(minA.z, minB.z) + std::min(maxA.z, maxB.z)) * 0.5f
            };
            manifold.m_HasContact = true;
            return manifold;
        }

        SContactManifold TestSphereBox(const Vec3& sphereCenter, float radius,
                                       const Vec3& boxMin, const Vec3& boxMax)
        {
            SContactManifold manifold;
            const Vec3 closest = ClampPointToBox(sphereCenter, boxMin, boxMax);
            const Vec3 delta = closest - sphereCenter;
            const float distSq = delta.lengthSquared();

            if (distSq > radius * radius)
            {
                return manifold;
            }

            if (distSq > PHYSICS_EPSILON)
            {
                const float dist = std::sqrt(distSq);
                manifold.m_Normal = delta * (1.0f / dist);
                manifold.m_Penetration = radius - dist;
                manifold.m_Point = closest;
                manifold.m_HasContact = true;
                return manifold;
            }

            const float dxMin = sphereCenter.x - boxMin.x;
            const float dxMax = boxMax.x - sphereCenter.x;
            const float dyMin = sphereCenter.y - boxMin.y;
            const float dyMax = boxMax.y - sphereCenter.y;
            const float dzMin = sphereCenter.z - boxMin.z;
            const float dzMax = boxMax.z - sphereCenter.z;

            float minDist = dxMin;
            Vec3 normalAtoB = { 1.0f, 0.0f, 0.0f };
            if (dxMax < minDist)
            {
                minDist = dxMax;
                normalAtoB = { -1.0f, 0.0f, 0.0f };
            }
            if (dyMin < minDist)
            {
                minDist = dyMin;
                normalAtoB = { 0.0f, 1.0f, 0.0f };
            }
            if (dyMax < minDist)
            {
                minDist = dyMax;
                normalAtoB = { 0.0f, -1.0f, 0.0f };
            }
            if (dzMin < minDist)
            {
                minDist = dzMin;
                normalAtoB = { 0.0f, 0.0f, 1.0f };
            }
            if (dzMax < minDist)
            {
                minDist = dzMax;
                normalAtoB = { 0.0f, 0.0f, -1.0f };
            }

            manifold.m_Normal = normalAtoB;
            manifold.m_Penetration = radius + minDist;
            manifold.m_Point = sphereCenter;
            manifold.m_HasContact = true;
            return manifold;
        }

        SContactManifold TestCapsuleBox(const Vec3& segA, const Vec3& segB, float radius,
                                        const Vec3& boxMin, const Vec3& boxMax)
        {
            const float minY = std::min(segA.y, segB.y);
            const float maxY = std::max(segA.y, segB.y);
            const float clampedY = std::clamp(
                (boxMin.y + boxMax.y) * 0.5f,
                minY,
                maxY);

            Vec3 candidate = { segA.x, clampedY, segA.z };
            for (int iter = 0; iter < 2; ++iter)
            {
                const Vec3 boxClosest = ClampPointToBox(candidate, boxMin, boxMax);
                candidate = ClosestPointOnSegment(boxClosest, segA, segB);
            }

            return TestSphereBox(candidate, radius, boxMin, boxMax);
        }

        bool IntersectRayBox(const Vec3& origin, const Vec3& dir, float maxDistance,
                             const Vec3& boxMin, const Vec3& boxMax,
                             float& outT, Vec3& outNormal)
        {
            float tMin = 0.0f;
            float tMax = maxDistance;
            Vec3 hitNormal = { 0.0f, 1.0f, 0.0f };

            const float aOrigin[3] = { origin.x, origin.y, origin.z };
            const float aDir[3] = { dir.x, dir.y, dir.z };
            const float aMin[3] = { boxMin.x, boxMin.y, boxMin.z };
            const float aMax[3] = { boxMax.x, boxMax.y, boxMax.z };

            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::fabs(aDir[axis]) < PHYSICS_EPSILON)
                {
                    if (aOrigin[axis] < aMin[axis] || aOrigin[axis] > aMax[axis])
                    {
                        return false;
                    }
                    continue;
                }

                const float invD = 1.0f / aDir[axis];
                float t1 = (aMin[axis] - aOrigin[axis]) * invD;
                float t2 = (aMax[axis] - aOrigin[axis]) * invD;
                float nSign = -1.0f;
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                    nSign = 1.0f;
                }

                if (t1 > tMin)
                {
                    tMin = t1;
                    hitNormal = { 0.0f, 0.0f, 0.0f };
                    if (axis == 0)
                    {
                        hitNormal.x = nSign;
                    }
                    else if (axis == 1)
                    {
                        hitNormal.y = nSign;
                    }
                    else
                    {
                        hitNormal.z = nSign;
                    }
                }
                tMax = std::min(tMax, t2);
                if (tMin > tMax)
                {
                    return false;
                }
            }

            outT = tMin;
            outNormal = hitNormal;
            return true;
        }

        bool IntersectRaySphere(const Vec3& origin, const Vec3& dir, float maxDistance,
                                const Vec3& center, float radius,
                                float& outT, Vec3& outNormal)
        {
            const Vec3 oc = origin - center;
            const float b = oc.dot(dir);
            const float c = oc.dot(oc) - radius * radius;
            const float disc = b * b - c;
            if (disc < 0.0f)
            {
                return false;
            }

            const float sqrtDisc = std::sqrt(disc);
            float t = -b - sqrtDisc;
            if (t < 0.0f)
            {
                t = -b + sqrtDisc;
            }
            if (t < 0.0f || t > maxDistance)
            {
                return false;
            }

            outT = t;
            const Vec3 hitPoint = origin + dir * t;
            const Vec3 n = hitPoint - center;
            outNormal = n.lengthSquared() > PHYSICS_EPSILON ? n.normalized() : Vec3{ 0.0f, 1.0f, 0.0f };
            return true;
        }

        bool IntersectRayCapsule(const Vec3& origin, const Vec3& dir, float maxDistance,
                                 const Vec3& segA, const Vec3& segB, float radius,
                                 float& outT, Vec3& outNormal)
        {
            bool hitAny = false;
            float bestT = maxDistance;
            Vec3 bestNormal = { 0.0f, 1.0f, 0.0f };

            float capT = 0.0f;
            Vec3 capNormal{};
            if (IntersectRaySphere(origin, dir, bestT, segA, radius, capT, capNormal))
            {
                hitAny = true;
                bestT = capT;
                bestNormal = capNormal;
            }
            if (IntersectRaySphere(origin, dir, bestT, segB, radius, capT, capNormal))
            {
                hitAny = true;
                bestT = capT;
                bestNormal = capNormal;
            }

            const Vec3 ba = segB - segA;
            const float baLenSq = ba.lengthSquared();
            if (baLenSq > PHYSICS_EPSILON)
            {
                const Vec3 oa = origin - segA;
                const float baDotD = ba.dot(dir);
                const float baDotOa = ba.dot(oa);
                const float a = baLenSq - baDotD * baDotD;
                const float b = baLenSq * oa.dot(dir) - baDotOa * baDotD;
                const float c = baLenSq * oa.dot(oa) - baDotOa * baDotOa - radius * radius * baLenSq;
                const float disc = b * b - a * c;
                if (a > PHYSICS_EPSILON && disc >= 0.0f)
                {
                    const float t = (-b - std::sqrt(disc)) / a;
                    if (t >= 0.0f && t < bestT)
                    {
                        const float y = baDotOa + t * baDotD;
                        if (y >= 0.0f && y <= baLenSq)
                        {
                            const Vec3 hitPoint = origin + dir * t;
                            const Vec3 axisPoint = segA + ba * (y / baLenSq);
                            const Vec3 n = hitPoint - axisPoint;
                            bestT = t;
                            bestNormal = n.lengthSquared() > PHYSICS_EPSILON ? n.normalized() : Vec3{ 0.0f, 1.0f, 0.0f };
                            hitAny = true;
                        }
                    }
                }
            }

            if (hitAny)
            {
                outT = bestT;
                outNormal = bestNormal;
            }
            return hitAny;
        }

        void ApplyWorldDeltaToEntity(CScene& scene, int entityIndex, const Vec3& worldDelta)
        {
            CEntity& entity = scene.m_vEntities[static_cast<size_t>(entityIndex)];
            CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pTransform)
            {
                return;
            }
            pTransform->ClearLocalMatrixOverride();

            if (entity.m_ParentId < 0)
            {
                pTransform->m_Position = pTransform->m_Position + worldDelta;
                return;
            }

            const int parentIndex = scene.FindEntityIndexById(entity.m_ParentId);
            if (parentIndex < 0)
            {
                pTransform->m_Position = pTransform->m_Position + worldDelta;
                return;
            }

            const Mat4 parentWorld = quark::ComposeWorld(scene, parentIndex);
            const Mat4 invParent = parentWorld.inverted();
            const Vec3 localOrigin = Vec3(invParent * Vec3{ 0.0f, 0.0f, 0.0f });
            const Vec3 localEnd = Vec3(invParent * worldDelta);
            pTransform->m_Position = pTransform->m_Position + (localEnd - localOrigin);
        }

        Vec3 FilterFrozenAxes(const Vec3& v, const CRigidbodyComponent* pBody)
        {
            if (!pBody)
            {
                return v;
            }
            return {
                pBody->m_FreezePositionX ? 0.0f : v.x,
                pBody->m_FreezePositionY ? 0.0f : v.y,
                pBody->m_FreezePositionZ ? 0.0f : v.z
            };
        }

        Vec3 FilterFrozenRotationAxes(const Vec3& v, const CRigidbodyComponent* pBody)
        {
            if (!pBody)
            {
                return v;
            }
            return {
                pBody->m_FreezeRotationX ? 0.0f : v.x,
                pBody->m_FreezeRotationY ? 0.0f : v.y,
                pBody->m_FreezeRotationZ ? 0.0f : v.z
            };
        }

        Vec3 ComputeInverseInertia(const CEntity& entity, const CRigidbodyComponent* pBody)
        {
            if (!pBody || !pBody->m_Enabled || pBody->m_IsKinematic)
            {
                return { 0.0f, 0.0f, 0.0f };
            }

            const float mass = std::max(pBody->m_Mass, 0.001f);
            Vec3 inertia = { (1.0f / 6.0f) * mass, (1.0f / 6.0f) * mass, (1.0f / 6.0f) * mass };
            const CCollisionComponent* pCollider = entity.GetCollisionComponent();
            const CTransformComponent* pTransform = entity.GetTransformComponent();
            if (pCollider)
            {
                const Vec3 scale = pTransform ? pTransform->m_Scale : Vec3{ 1.0f, 1.0f, 1.0f };
                if (pCollider->m_ColliderType == COLLIDER_SPHERE)
                {
                    const float r = std::max(pCollider->m_Radius * std::max({ std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z) }), 0.001f);
                    const float iSphere = 0.4f * mass * r * r;
                    inertia = { iSphere, iSphere, iSphere };
                }
                else if (pCollider->m_ColliderType == COLLIDER_CAPSULE)
                {
                    const float r = std::max(pCollider->m_Radius * std::max(std::fabs(scale.x), std::fabs(scale.z)), 0.001f);
                    const float h = std::max(pCollider->m_Height * std::fabs(scale.y), r * 2.0f);
                    const float iy = 0.5f * mass * r * r;
                    const float ixz = (1.0f / 12.0f) * mass * (3.0f * r * r + h * h);
                    inertia = { ixz, iy, ixz };
                }
                else
                {
                    const float sx = std::max(std::fabs(pCollider->m_Size.x * scale.x), 0.001f);
                    const float sy = std::max(std::fabs(pCollider->m_Size.y * scale.y), 0.001f);
                    const float sz = std::max(std::fabs(pCollider->m_Size.z * scale.z), 0.001f);
                    inertia = {
                        (1.0f / 12.0f) * mass * (sy * sy + sz * sz),
                        (1.0f / 12.0f) * mass * (sx * sx + sz * sz),
                        (1.0f / 12.0f) * mass * (sx * sx + sy * sy)
                    };
                }
            }

            return {
                (pBody->m_FreezeRotationX || inertia.x <= PHYSICS_EPSILON) ? 0.0f : (1.0f / inertia.x),
                (pBody->m_FreezeRotationY || inertia.y <= PHYSICS_EPSILON) ? 0.0f : (1.0f / inertia.y),
                (pBody->m_FreezeRotationZ || inertia.z <= PHYSICS_EPSILON) ? 0.0f : (1.0f / inertia.z)
            };
        }

        Vec3 ComponentWiseMul(const Vec3& a, const Vec3& b)
        {
            return { a.x * b.x, a.y * b.y, a.z * b.z };
        }

        float ComputeMinColliderExtent(const CEntity& entity)
        {
            const CCollisionComponent* pCollider = entity.GetCollisionComponent();
            const CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pCollider)
            {
                return 0.5f;
            }
            const Vec3 scale = pTransform ? pTransform->m_Scale : Vec3{ 1.0f, 1.0f, 1.0f };
            if (pCollider->m_ColliderType == COLLIDER_SPHERE)
            {
                return std::max(pCollider->m_Radius * std::max({ std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z) }), 0.02f);
            }
            if (pCollider->m_ColliderType == COLLIDER_CAPSULE)
            {
                return std::max(pCollider->m_Radius * std::max(std::fabs(scale.x), std::fabs(scale.z)), 0.02f);
            }
            const float hx = std::fabs(pCollider->m_Size.x * scale.x) * 0.5f;
            const float hy = std::fabs(pCollider->m_Size.y * scale.y) * 0.5f;
            const float hz = std::fabs(pCollider->m_Size.z * scale.z) * 0.5f;
            return std::max(std::min({ hx, hy, hz }), 0.02f);
        }
    } // anonymous

    void CPhysicsWorld::Reset()
    {
        m_PreviousCollisions.clear();
        m_PreviousTriggers.clear();
        m_vWorldColliders.clear();
    }

    CPhysicsWorld::SWorldCollider CPhysicsWorld::BuildWorldCollider(const CScene& scene,
                                                                    int entityIndex,
                                                                    const CCollisionComponent& collider)
    {
        SWorldCollider wc;
        wc.m_EntityIndex = entityIndex;
        wc.m_EntityId = scene.m_vEntities[static_cast<size_t>(entityIndex)].m_Id;
        wc.m_Type = collider.m_ColliderType;
        wc.m_IsTrigger = collider.m_IsTrigger;

        const Mat4 world = quark::ComposeWorld(scene, entityIndex);
        const Vec3 axisX = { world.m[0], world.m[1], world.m[2] };
        const Vec3 axisY = { world.m[4], world.m[5], world.m[6] };
        const Vec3 axisZ = { world.m[8], world.m[9], world.m[10] };
        const float scaleX = std::max(axisX.length(), 0.001f);
        const float scaleY = std::max(axisY.length(), 0.001f);
        const float scaleZ = std::max(axisZ.length(), 0.001f);

        wc.m_Center = Vec3(world * collider.m_Center);

        if (collider.m_ColliderType == COLLIDER_SPHERE)
        {
            wc.m_Type = COLLIDER_SPHERE;
            const float maxScale = std::max({ scaleX, scaleY, scaleZ });
            wc.m_Radius = std::max(collider.m_Radius * maxScale, 0.001f);
            const Vec3 rVec = { wc.m_Radius, wc.m_Radius, wc.m_Radius };
            wc.m_BroadphaseBounds = { wc.m_Center - rVec, wc.m_Center + rVec };
        }
        else if (collider.m_ColliderType == COLLIDER_CAPSULE)
        {
            float radialScale = std::max(scaleX, scaleZ);
            float heightScale = scaleY;
            Vec3 axisDir = { 0.0f, 1.0f, 0.0f };
            if (collider.m_Direction == ECapsuleDirection::X_AXIS)
            {
                radialScale = std::max(scaleY, scaleZ);
                heightScale = scaleX;
                axisDir = { 1.0f, 0.0f, 0.0f };
            }
            else if (collider.m_Direction == ECapsuleDirection::Z_AXIS)
            {
                radialScale = std::max(scaleX, scaleY);
                heightScale = scaleZ;
                axisDir = { 0.0f, 0.0f, 1.0f };
            }

            wc.m_Radius = std::max(collider.m_Radius * radialScale, 0.001f);
            const float scaledHeight = std::max(collider.m_Height * heightScale, wc.m_Radius * 2.0f);
            const float halfSeg = std::max(0.0f, scaledHeight * 0.5f - wc.m_Radius);
            wc.m_SegmentA = wc.m_Center - axisDir * halfSeg;
            wc.m_SegmentB = wc.m_Center + axisDir * halfSeg;
            const Vec3 minSeg = {
                std::min(wc.m_SegmentA.x, wc.m_SegmentB.x) - wc.m_Radius,
                std::min(wc.m_SegmentA.y, wc.m_SegmentB.y) - wc.m_Radius,
                std::min(wc.m_SegmentA.z, wc.m_SegmentB.z) - wc.m_Radius
            };
            const Vec3 maxSeg = {
                std::max(wc.m_SegmentA.x, wc.m_SegmentB.x) + wc.m_Radius,
                std::max(wc.m_SegmentA.y, wc.m_SegmentB.y) + wc.m_Radius,
                std::max(wc.m_SegmentA.z, wc.m_SegmentB.z) + wc.m_Radius
            };
            wc.m_BroadphaseBounds = { minSeg, maxSeg };
        }
        else
        {
            wc.m_Type = COLLIDER_BOX;
            const Vec3 halfLocal = {
                std::max(std::fabs(collider.m_Size.x) * 0.5f, 0.001f),
                std::max(std::fabs(collider.m_Size.y) * 0.5f, 0.001f),
                std::max(std::fabs(collider.m_Size.z) * 0.5f, 0.001f)
            };
            const Vec3 halfWorld = {
                std::fabs(axisX.x) * halfLocal.x + std::fabs(axisY.x) * halfLocal.y + std::fabs(axisZ.x) * halfLocal.z,
                std::fabs(axisX.y) * halfLocal.x + std::fabs(axisY.y) * halfLocal.y + std::fabs(axisZ.y) * halfLocal.z,
                std::fabs(axisX.z) * halfLocal.x + std::fabs(axisY.z) * halfLocal.y + std::fabs(axisZ.z) * halfLocal.z
            };
            wc.m_BoxMin = wc.m_Center - halfWorld;
            wc.m_BoxMax = wc.m_Center + halfWorld;
            wc.m_BroadphaseBounds = { wc.m_BoxMin, wc.m_BoxMax };
        }

        return wc;
    }

    bool CPhysicsWorld::OverlapsAabb(const BoundingBox& a, const BoundingBox& b)
    {
        return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
               (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
               (a.min.z <= b.max.z && a.max.z >= b.min.z);
    }

    SContactManifold CPhysicsWorld::TestColliders(const SWorldCollider& a, const SWorldCollider& b)
    {
        if (a.m_Type == COLLIDER_SPHERE && b.m_Type == COLLIDER_SPHERE)
        {
            return TestSphereSphere(a.m_Center, a.m_Radius, b.m_Center, b.m_Radius);
        }
        if (a.m_Type == COLLIDER_BOX && b.m_Type == COLLIDER_BOX)
        {
            return TestBoxBox(a.m_BoxMin, a.m_BoxMax, b.m_BoxMin, b.m_BoxMax);
        }
        if (a.m_Type == COLLIDER_SPHERE && b.m_Type == COLLIDER_BOX)
        {
            return TestSphereBox(a.m_Center, a.m_Radius, b.m_BoxMin, b.m_BoxMax);
        }
        if (a.m_Type == COLLIDER_BOX && b.m_Type == COLLIDER_SPHERE)
        {
            SContactManifold m = TestSphereBox(b.m_Center, b.m_Radius, a.m_BoxMin, a.m_BoxMax);
            m.m_Normal = m.m_Normal * -1.0f;
            return m;
        }
        if (a.m_Type == COLLIDER_CAPSULE && b.m_Type == COLLIDER_SPHERE)
        {
            const Vec3 closest = ClosestPointOnSegment(b.m_Center, a.m_SegmentA, a.m_SegmentB);
            return TestSphereSphere(closest, a.m_Radius, b.m_Center, b.m_Radius);
        }
        if (a.m_Type == COLLIDER_SPHERE && b.m_Type == COLLIDER_CAPSULE)
        {
            const Vec3 closest = ClosestPointOnSegment(a.m_Center, b.m_SegmentA, b.m_SegmentB);
            return TestSphereSphere(a.m_Center, a.m_Radius, closest, b.m_Radius);
        }
        if (a.m_Type == COLLIDER_CAPSULE && b.m_Type == COLLIDER_CAPSULE)
        {
            Vec3 c1{};
            Vec3 c2{};
            ClosestPointsBetweenSegments(a.m_SegmentA, a.m_SegmentB, b.m_SegmentA, b.m_SegmentB, c1, c2);
            return TestSphereSphere(c1, a.m_Radius, c2, b.m_Radius);
        }
        if (a.m_Type == COLLIDER_CAPSULE && b.m_Type == COLLIDER_BOX)
        {
            return TestCapsuleBox(a.m_SegmentA, a.m_SegmentB, a.m_Radius, b.m_BoxMin, b.m_BoxMax);
        }
        if (a.m_Type == COLLIDER_BOX && b.m_Type == COLLIDER_CAPSULE)
        {
            SContactManifold m = TestCapsuleBox(b.m_SegmentA, b.m_SegmentB, b.m_Radius, a.m_BoxMin, a.m_BoxMax);
            m.m_Normal = m.m_Normal * -1.0f;
            return m;
        }
        return {};
    }

    void CPhysicsWorld::SyncBeforeFixedSteps(CScene& scene)
    {
        for (CEntity& entity : scene.m_vEntities)
        {
            CRigidbodyComponent* pBody = entity.GetRigidbodyComponent();
            CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pBody || !pBody->m_Enabled || !pTransform)
            {
                continue;
            }

            if (!pBody->m_HasInterpolationState)
            {
                pBody->m_PrevPosition = pTransform->m_Position;
                pBody->m_PrevRotation = pTransform->m_Rotation;
                pBody->m_PhysicsPosition = pTransform->m_Position;
                pBody->m_PhysicsRotation = pTransform->m_Rotation;
                pBody->m_LastRenderedPosition = pTransform->m_Position;
                pBody->m_LastRenderedRotation = pTransform->m_Rotation;
                pBody->m_HasInterpolationState = true;
                continue;
            }

            const bool posTeleported = (pTransform->m_Position - pBody->m_LastRenderedPosition).lengthSquared() > 1e-8f;
            const bool rotTeleported = (pTransform->m_Rotation - pBody->m_LastRenderedRotation).lengthSquared() > 1e-8f;
            if (posTeleported)
            {
                pBody->m_PhysicsPosition = pTransform->m_Position;
                pBody->m_PrevPosition = pTransform->m_Position;
            }
            else
            {
                pTransform->m_Position = pBody->m_PhysicsPosition;
            }

            if (rotTeleported)
            {
                pBody->m_PhysicsRotation = pTransform->m_Rotation;
                pBody->m_PrevRotation = pTransform->m_Rotation;
            }
            else
            {
                pTransform->m_Rotation = pBody->m_PhysicsRotation;
            }
        }
    }

    void CPhysicsWorld::ApplyInterpolation(CScene& scene, float alpha, float remainderTime)
    {
        const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
        bool anyVisualChanged = false;

        for (CEntity& entity : scene.m_vEntities)
        {
            CRigidbodyComponent* pBody = entity.GetRigidbodyComponent();
            CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pBody || !pBody->m_Enabled || !pTransform || !pBody->m_HasInterpolationState)
            {
                continue;
            }

            if (pBody->m_Interpolate == ERigidbodyInterpolate::INTERPOLATE)
            {
                pTransform->m_Position = pBody->m_PrevPosition + (pBody->m_PhysicsPosition - pBody->m_PrevPosition) * clampedAlpha;
                pTransform->m_Rotation = pBody->m_PrevRotation + (pBody->m_PhysicsRotation - pBody->m_PrevRotation) * clampedAlpha;
                pTransform->ClearLocalMatrixOverride();
                anyVisualChanged = true;
            }
            else if (pBody->m_Interpolate == ERigidbodyInterpolate::EXTRAPOLATE && !pBody->m_IsKinematic && !pBody->m_IsSleeping)
            {
                const float dt = std::max(0.0f, remainderTime);
                pTransform->m_Position = pBody->m_PhysicsPosition + pBody->m_Velocity * dt;
                pTransform->m_Rotation = pBody->m_PhysicsRotation + pBody->m_AngularVelocity * (RAD2DEG * dt);
                pTransform->ClearLocalMatrixOverride();
                anyVisualChanged = true;
            }
            else
            {
                pTransform->m_Position = pBody->m_PhysicsPosition;
                pTransform->m_Rotation = pBody->m_PhysicsRotation;
            }

            pBody->m_LastRenderedPosition = pTransform->m_Position;
            pBody->m_LastRenderedRotation = pTransform->m_Rotation;
        }

        if (anyVisualChanged)
        {
            scene.BumpTransformRevision();
        }
    }

    void CPhysicsWorld::Step(CScene& scene, float fixedDeltaTime, std::vector<SPhysicsEvent>& vOutEvents)
    {
        vOutEvents.clear();
        if (fixedDeltaTime <= 0.0f)
        {
            return;
        }

        const int entityCount = static_cast<int>(scene.m_vEntities.size());
        bool anyTransformMoved = false;

        int substeps = 1;
        for (int i = 0; i < entityCount; ++i)
        {
            CEntity& entity = scene.m_vEntities[static_cast<size_t>(i)];
            CRigidbodyComponent* pBody = entity.GetRigidbodyComponent();
            CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pBody || !pBody->m_Enabled || !pTransform)
            {
                continue;
            }

            pBody->m_PrevPosition = pTransform->m_Position;
            pBody->m_PrevRotation = pTransform->m_Rotation;
            pBody->m_HasInterpolationState = true;
            pBody->m_IsGrounded = false;

            if (!pBody->m_IsKinematic && pBody->m_CollisionDetection != ECollisionDetectionMode::DISCRETE)
            {
                const float invMass = pBody->GetInverseMass();
                Vec3 estAccel = pBody->m_AccumulatedForce * invMass;
                if (pBody->m_UseGravity)
                {
                    estAccel = estAccel + m_Gravity * pBody->m_GravityScale;
                }
                const Vec3 estVel = pBody->m_Velocity + estAccel * fixedDeltaTime;
                const float travelDist = estVel.length() * fixedDeltaTime;
                const float minExtent = ComputeMinColliderExtent(entity);
                const float maxStepDist = minExtent * 0.4f;
                if (travelDist > maxStepDist)
                {
                    const int needed = static_cast<int>(std::ceil(travelDist / maxStepDist));
                    substeps = std::max(substeps, std::clamp(needed, 2, 64));
                }
            }
        }

        const float subDt = fixedDeltaTime / static_cast<float>(substeps);
        std::unordered_set<std::uint64_t> stepCollisions;
        std::unordered_set<std::uint64_t> stepTriggers;
        std::unordered_map<std::uint64_t, SPhysicsEvent> stepCollisionEventData;
        std::unordered_map<std::uint64_t, SPhysicsEvent> stepTriggerEventData;

        for (int sub = 0; sub < substeps; ++sub)
        {
            for (int i = 0; i < entityCount; ++i)
            {
                CEntity& entity = scene.m_vEntities[static_cast<size_t>(i)];
                CRigidbodyComponent* pBody = entity.GetRigidbodyComponent();
                CTransformComponent* pTransform = entity.GetTransformComponent();
                if (!pBody || !pBody->m_Enabled || !pTransform)
                {
                    continue;
                }

                if (pBody->m_IsKinematic)
                {
                    pBody->m_AccumulatedForce = { 0.0f, 0.0f, 0.0f };
                    pBody->m_AccumulatedTorque = { 0.0f, 0.0f, 0.0f };
                    continue;
                }

                const float invMass = pBody->GetInverseMass();
                const Vec3 invInertia = ComputeInverseInertia(entity, pBody);

                Vec3 accel = pBody->m_AccumulatedForce * invMass;
                if (pBody->m_UseGravity)
                {
                    accel = accel + m_Gravity * pBody->m_GravityScale;
                }
                pBody->m_Velocity = pBody->m_Velocity + accel * subDt;

                if (pBody->m_LinearDrag > 0.0f)
                {
                    const float damping = std::exp(-pBody->m_LinearDrag * subDt);
                    pBody->m_Velocity = pBody->m_Velocity * damping;
                }
                pBody->m_Velocity = FilterFrozenAxes(pBody->m_Velocity, pBody);

                const Vec3 angularAccel = ComponentWiseMul(pBody->m_AccumulatedTorque, invInertia);
                pBody->m_AngularVelocity = pBody->m_AngularVelocity + angularAccel * subDt;
                if (pBody->m_AngularDrag > 0.0f)
                {
                    const float angularDamping = std::exp(-pBody->m_AngularDrag * subDt);
                    pBody->m_AngularVelocity = pBody->m_AngularVelocity * angularDamping;
                }
                pBody->m_AngularVelocity = FilterFrozenRotationAxes(pBody->m_AngularVelocity, pBody);

                if (sub == substeps - 1)
                {
                    pBody->m_AccumulatedForce = { 0.0f, 0.0f, 0.0f };
                    pBody->m_AccumulatedTorque = { 0.0f, 0.0f, 0.0f };
                }

                Vec3 displacement = pBody->m_Velocity * subDt;
                const float dispLenSq = displacement.lengthSquared();
                if (dispLenSq > 1e-12f)
                {
                    if (pBody->m_CollisionDetection != ECollisionDetectionMode::DISCRETE)
                    {
                        const float dispLen = std::sqrt(dispLenSq);
                        const float minExtent = ComputeMinColliderExtent(entity);
                        if (dispLen > minExtent * 0.4f)
                        {
                            const Vec3 dir = displacement * (1.0f / dispLen);
                            const Vec3 rayOrigin = Vec3(quark::ComposeWorld(scene, i) * Vec3{ 0.0f, 0.0f, 0.0f });
                            SPhysicsRaycastHit sweepHit;
                            if (Raycast(scene, rayOrigin, dir, dispLen + minExtent, sweepHit, false, entity.m_Id))
                            {
                                const float safeDist = std::max(0.0f, sweepHit.m_Distance - minExtent * 0.85f);
                                if (safeDist < dispLen)
                                {
                                    displacement = dir * safeDist;
                                }
                            }
                        }
                    }

                    ApplyWorldDeltaToEntity(scene, i, displacement);
                    anyTransformMoved = true;
                }

                const Vec3 rotDeltaDeg = pBody->m_AngularVelocity * (RAD2DEG * subDt);
                if (rotDeltaDeg.lengthSquared() > 1e-10f)
                {
                    pTransform->ClearLocalMatrixOverride();
                    pTransform->m_Rotation = pTransform->m_Rotation + rotDeltaDeg;
                    anyTransformMoved = true;
                }
            }

            m_vWorldColliders.clear();
            m_vWorldColliders.reserve(static_cast<size_t>(entityCount));
            for (int i = 0; i < entityCount; ++i)
            {
                const CEntity& entity = scene.m_vEntities[static_cast<size_t>(i)];
                const CCollisionComponent* pCollider = entity.GetCollisionComponent();
                if (!pCollider || !pCollider->m_Enabled || !entity.GetTransformComponent())
                {
                    continue;
                }
                m_vWorldColliders.push_back(BuildWorldCollider(scene, i, *pCollider));
            }

            const size_t colliderCount = m_vWorldColliders.size();
            for (size_t i = 0; i < colliderCount; ++i)
            {
                for (size_t j = i + 1; j < colliderCount; ++j)
                {
                    SWorldCollider& colA = m_vWorldColliders[i];
                    SWorldCollider& colB = m_vWorldColliders[j];

                    if (!OverlapsAabb(colA.m_BroadphaseBounds, colB.m_BroadphaseBounds))
                    {
                        continue;
                    }

                    CEntity& entityA = scene.m_vEntities[static_cast<size_t>(colA.m_EntityIndex)];
                    CEntity& entityB = scene.m_vEntities[static_cast<size_t>(colB.m_EntityIndex)];
                    CRigidbodyComponent* pBodyA = entityA.GetRigidbodyComponent();
                    CRigidbodyComponent* pBodyB = entityB.GetRigidbodyComponent();

                    const bool isTriggerPair = colA.m_IsTrigger || colB.m_IsTrigger;
                    const bool hasAnyBody = (pBodyA && pBodyA->m_Enabled) || (pBodyB && pBodyB->m_Enabled);
                    if (!hasAnyBody && !isTriggerPair)
                    {
                        continue;
                    }

                    const SContactManifold manifold = TestColliders(colA, colB);
                    if (!manifold.m_HasContact)
                    {
                        continue;
                    }

                    const std::uint64_t pairKey = MakePairKey(colA.m_EntityId, colB.m_EntityId);
                    if (isTriggerPair)
                    {
                        stepTriggers.insert(pairKey);
                        stepTriggerEventData[pairKey] = {
                            EPhysicsEventType::TRIGGER_ENTER,
                            colA.m_EntityId,
                            colB.m_EntityId,
                            manifold.m_Point,
                            manifold.m_Normal,
                            manifold.m_Penetration
                        };
                        continue;
                    }

                    const CCollisionComponent* pColCompA = entityA.GetCollisionComponent();
                    const CCollisionComponent* pColCompB = entityB.GetCollisionComponent();
                    const bool providesContacts =
                        (!pColCompA || pColCompA->m_ProvidesContacts) &&
                        (!pColCompB || pColCompB->m_ProvidesContacts);
                    if (providesContacts)
                    {
                        stepCollisions.insert(pairKey);
                        stepCollisionEventData[pairKey] = {
                            EPhysicsEventType::COLLISION_ENTER,
                            colA.m_EntityId,
                            colB.m_EntityId,
                            manifold.m_Point,
                            manifold.m_Normal,
                            manifold.m_Penetration
                        };
                    }

                    const float invMassA = pBodyA ? pBodyA->GetInverseMass() : 0.0f;
                    const float invMassB = pBodyB ? pBodyB->GetInverseMass() : 0.0f;
                    const float invMassSum = invMassA + invMassB;
                    if (invMassSum <= PHYSICS_EPSILON)
                    {
                        continue;
                    }

                    if (pBodyA && manifold.m_Normal.y < -0.5f)
                    {
                        pBodyA->m_IsGrounded = true;
                    }
                    if (pBodyB && manifold.m_Normal.y > 0.5f)
                    {
                        pBodyB->m_IsGrounded = true;
                    }

                    constexpr float SLOP = 0.0001f;
                    const float correctionPercent = (invMassA == 0.0f || invMassB == 0.0f) ? 1.0f : 0.9f;
                    const float corrMag = std::max(manifold.m_Penetration - SLOP, 0.0f) * correctionPercent / invMassSum;
                    if (corrMag > 0.0f)
                    {
                        if (invMassA > 0.0f)
                        {
                            const Vec3 deltaA = FilterFrozenAxes(manifold.m_Normal * (-corrMag * invMassA), pBodyA);
                            ApplyWorldDeltaToEntity(scene, colA.m_EntityIndex, deltaA);
                            colA = BuildWorldCollider(scene, colA.m_EntityIndex, *entityA.GetCollisionComponent());
                            anyTransformMoved = true;
                        }
                        if (invMassB > 0.0f)
                        {
                            const Vec3 deltaB = FilterFrozenAxes(manifold.m_Normal * (corrMag * invMassB), pBodyB);
                            ApplyWorldDeltaToEntity(scene, colB.m_EntityIndex, deltaB);
                            colB = BuildWorldCollider(scene, colB.m_EntityIndex, *entityB.GetCollisionComponent());
                            anyTransformMoved = true;
                        }
                    }

                    const Vec3 rA = manifold.m_Point - colA.m_Center;
                    const Vec3 rB = manifold.m_Point - colB.m_Center;
                    const Vec3 invInertiaA = ComputeInverseInertia(entityA, pBodyA);
                    const Vec3 invInertiaB = ComputeInverseInertia(entityB, pBodyB);

                    const Vec3 velA = pBodyA ? (pBodyA->m_Velocity + pBodyA->m_AngularVelocity.cross(rA)) : Vec3{ 0.0f, 0.0f, 0.0f };
                    const Vec3 velB = pBodyB ? (pBodyB->m_Velocity + pBodyB->m_AngularVelocity.cross(rB)) : Vec3{ 0.0f, 0.0f, 0.0f };
                    const Vec3 relVel = velB - velA;
                    const float velAlongNormal = relVel.dot(manifold.m_Normal);
                    if (velAlongNormal > 0.0f)
                    {
                        continue;
                    }

                    const Vec3 rAxN = rA.cross(manifold.m_Normal);
                    const Vec3 rBxN = rB.cross(manifold.m_Normal);
                    const float angularDenomA = ComponentWiseMul(invInertiaA, rAxN).cross(rA).dot(manifold.m_Normal);
                    const float angularDenomB = ComponentWiseMul(invInertiaB, rBxN).cross(rB).dot(manifold.m_Normal);
                    const float effectiveInvMass = invMassSum + angularDenomA + angularDenomB;
                    if (effectiveInvMass <= PHYSICS_EPSILON)
                    {
                        continue;
                    }

                    const float baseRestA = pBodyA ? pBodyA->m_Restitution : 0.0f;
                    const float baseRestB = pBodyB ? pBodyB->m_Restitution : 0.0f;
                    const float restA = pColCompA ? pColCompA->GetEffectiveRestitution(baseRestA) : baseRestA;
                    const float restB = pColCompB ? pColCompB->GetEffectiveRestitution(baseRestB) : baseRestB;
                    float restitution = std::max(restA, restB);
                    if (std::fabs(velAlongNormal) < 0.5f)
                    {
                        restitution = 0.0f;
                    }

                    const float impulseMag = -(1.0f + restitution) * velAlongNormal / effectiveInvMass;
                    const Vec3 impulse = manifold.m_Normal * impulseMag;

                    if (invMassA > 0.0f)
                    {
                        pBodyA->m_Velocity = FilterFrozenAxes(pBodyA->m_Velocity - impulse * invMassA, pBodyA);
                        const Vec3 angDeltaA = ComponentWiseMul(invInertiaA, rA.cross(impulse * -1.0f));
                        pBodyA->m_AngularVelocity = FilterFrozenRotationAxes(pBodyA->m_AngularVelocity + angDeltaA, pBodyA);
                    }
                    if (invMassB > 0.0f)
                    {
                        pBodyB->m_Velocity = FilterFrozenAxes(pBodyB->m_Velocity + impulse * invMassB, pBodyB);
                        const Vec3 angDeltaB = ComponentWiseMul(invInertiaB, rB.cross(impulse));
                        pBodyB->m_AngularVelocity = FilterFrozenRotationAxes(pBodyB->m_AngularVelocity + angDeltaB, pBodyB);
                    }

                    const Vec3 postVelA = pBodyA ? (pBodyA->m_Velocity + pBodyA->m_AngularVelocity.cross(rA)) : Vec3{ 0.0f, 0.0f, 0.0f };
                    const Vec3 postVelB = pBodyB ? (pBodyB->m_Velocity + pBodyB->m_AngularVelocity.cross(rB)) : Vec3{ 0.0f, 0.0f, 0.0f };
                    const Vec3 postRelVel = postVelB - postVelA;
                    Vec3 tangent = postRelVel - manifold.m_Normal * postRelVel.dot(manifold.m_Normal);
                    const float tangentLenSq = tangent.lengthSquared();
                    if (tangentLenSq > PHYSICS_EPSILON)
                    {
                        tangent = tangent * (1.0f / std::sqrt(tangentLenSq));
                        const Vec3 rAxT = rA.cross(tangent);
                        const Vec3 rBxT = rB.cross(tangent);
                        const float tangDenom = invMassSum +
                            ComponentWiseMul(invInertiaA, rAxT).cross(rA).dot(tangent) +
                            ComponentWiseMul(invInertiaB, rBxT).cross(rB).dot(tangent);
                        if (tangDenom > PHYSICS_EPSILON)
                        {
                            const float jt = -postRelVel.dot(tangent) / tangDenom;
                            const float baseFricA = pBodyA ? pBodyA->m_Friction : 0.5f;
                            const float baseFricB = pBodyB ? pBodyB->m_Friction : 0.5f;
                            const float fricA = pColCompA ? pColCompA->GetEffectiveFriction(baseFricA) : baseFricA;
                            const float fricB = pColCompB ? pColCompB->GetEffectiveFriction(baseFricB) : baseFricB;
                            const float mu = std::sqrt(std::max(0.0f, fricA * fricB));
                            const float clampedJt = std::clamp(jt, -impulseMag * mu, impulseMag * mu);
                            const Vec3 frictionImpulse = tangent * clampedJt;

                            if (invMassA > 0.0f)
                            {
                                pBodyA->m_Velocity = FilterFrozenAxes(pBodyA->m_Velocity - frictionImpulse * invMassA, pBodyA);
                                const Vec3 angFricA = ComponentWiseMul(invInertiaA, rA.cross(frictionImpulse * -1.0f));
                                pBodyA->m_AngularVelocity = FilterFrozenRotationAxes(pBodyA->m_AngularVelocity + angFricA, pBodyA);
                            }
                            if (invMassB > 0.0f)
                            {
                                pBodyB->m_Velocity = FilterFrozenAxes(pBodyB->m_Velocity + frictionImpulse * invMassB, pBodyB);
                                const Vec3 angFricB = ComponentWiseMul(invInertiaB, rB.cross(frictionImpulse));
                                pBodyB->m_AngularVelocity = FilterFrozenRotationAxes(pBodyB->m_AngularVelocity + angFricB, pBodyB);
                            }
                        }
                    }
                }
            }
        }

        for (const auto& [pairKey, evData] : stepCollisionEventData)
        {
            SPhysicsEvent ev = evData;
            ev.m_Type = (m_PreviousCollisions.count(pairKey) > 0)
                ? EPhysicsEventType::COLLISION_STAY
                : EPhysicsEventType::COLLISION_ENTER;
            vOutEvents.push_back(ev);
        }
        for (const auto& [pairKey, evData] : stepTriggerEventData)
        {
            SPhysicsEvent ev = evData;
            ev.m_Type = (m_PreviousTriggers.count(pairKey) > 0)
                ? EPhysicsEventType::TRIGGER_STAY
                : EPhysicsEventType::TRIGGER_ENTER;
            vOutEvents.push_back(ev);
        }
        for (std::uint64_t prevKey : m_PreviousCollisions)
        {
            if (stepCollisions.count(prevKey) == 0)
            {
                int idA = -1;
                int idB = -1;
                UnpackPairKey(prevKey, idA, idB);
                vOutEvents.push_back({ EPhysicsEventType::COLLISION_EXIT, idA, idB, {}, { 0.0f, 1.0f, 0.0f }, 0.0f });
            }
        }
        for (std::uint64_t prevKey : m_PreviousTriggers)
        {
            if (stepTriggers.count(prevKey) == 0)
            {
                int idA = -1;
                int idB = -1;
                UnpackPairKey(prevKey, idA, idB);
                vOutEvents.push_back({ EPhysicsEventType::TRIGGER_EXIT, idA, idB, {}, { 0.0f, 1.0f, 0.0f }, 0.0f });
            }
        }

        m_PreviousCollisions = std::move(stepCollisions);
        m_PreviousTriggers = std::move(stepTriggers);

        if (anyTransformMoved)
        {
            scene.BumpTransformRevision();
        }

        for (int i = 0; i < entityCount; ++i)
        {
            CEntity& entity = scene.m_vEntities[static_cast<size_t>(i)];
            CRigidbodyComponent* pBody = entity.GetRigidbodyComponent();
            const CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pBody || !pBody->m_Enabled || !pTransform)
            {
                continue;
            }
            pBody->m_PhysicsPosition = pTransform->m_Position;
            pBody->m_PhysicsRotation = pTransform->m_Rotation;
            pBody->m_LastRenderedPosition = pTransform->m_Position;
            pBody->m_LastRenderedRotation = pTransform->m_Rotation;

            if (pBody->m_IsKinematic)
            {
                continue;
            }
            const float speedSq = pBody->m_Velocity.lengthSquared();
            const float angSpeedSq = pBody->m_AngularVelocity.lengthSquared();
            if (pBody->m_IsGrounded && speedSq < 0.0025f && angSpeedSq < 0.0025f)
            {
                pBody->m_SleepTimer += fixedDeltaTime;
                if (pBody->m_SleepTimer >= 0.5f)
                {
                    pBody->m_IsSleeping = true;
                    pBody->m_Velocity = { 0.0f, 0.0f, 0.0f };
                    pBody->m_AngularVelocity = { 0.0f, 0.0f, 0.0f };
                }
            }
            else
            {
                pBody->m_SleepTimer = 0.0f;
                pBody->m_IsSleeping = false;
            }
        }
    }

    bool CPhysicsWorld::Raycast(const CScene& scene,
                                const Vec3& origin,
                                const Vec3& direction,
                                float maxDistance,
                                SPhysicsRaycastHit& outHit,
                                bool includeTriggers,
                                int ignoreEntityId) const
    {
        outHit = {};
        const float dirLenSq = direction.lengthSquared();
        if (dirLenSq <= PHYSICS_EPSILON || maxDistance <= 0.0f)
        {
            return false;
        }
        const Vec3 normDir = direction * (1.0f / std::sqrt(dirLenSq));

        float bestDist = maxDistance;
        bool foundHit = false;

        const int entityCount = static_cast<int>(scene.m_vEntities.size());
        for (int i = 0; i < entityCount; ++i)
        {
            const CEntity& entity = scene.m_vEntities[static_cast<size_t>(i)];
            if (entity.m_Id == ignoreEntityId)
            {
                continue;
            }
            const CCollisionComponent* pCollider = entity.GetCollisionComponent();
            if (!pCollider || !pCollider->m_Enabled || !entity.GetTransformComponent())
            {
                continue;
            }
            if (pCollider->m_IsTrigger && !includeTriggers)
            {
                continue;
            }

            const SWorldCollider wc = BuildWorldCollider(scene, i, *pCollider);
            float t = 0.0f;
            Vec3 normal = { 0.0f, 1.0f, 0.0f };
            bool hit = false;

            if (wc.m_Type == COLLIDER_SPHERE)
            {
                hit = IntersectRaySphere(origin, normDir, bestDist, wc.m_Center, wc.m_Radius, t, normal);
            }
            else if (wc.m_Type == COLLIDER_CAPSULE)
            {
                hit = IntersectRayCapsule(origin, normDir, bestDist, wc.m_SegmentA, wc.m_SegmentB, wc.m_Radius, t, normal);
            }
            else
            {
                hit = IntersectRayBox(origin, normDir, bestDist, wc.m_BoxMin, wc.m_BoxMax, t, normal);
            }

            if (hit && t < bestDist)
            {
                bestDist = t;
                foundHit = true;
                outHit.m_Hit = true;
                outHit.m_EntityId = entity.m_Id;
                outHit.m_Distance = t;
                outHit.m_Point = origin + normDir * t;
                outHit.m_Normal = normal;
            }
        }

        return foundHit;
    }
} // quark::physics
