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

        Vec3 ClosestPointOnTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
        {
            const Vec3 ab = b - a;
            const Vec3 ac = c - a;
            const Vec3 ap = p - a;
            const float d1 = ab.dot(ap);
            const float d2 = ac.dot(ap);
            if (d1 <= 0.0f && d2 <= 0.0f)
            {
                return a;
            }

            const Vec3 bp = p - b;
            const float d3 = ab.dot(bp);
            const float d4 = ac.dot(bp);
            if (d3 >= 0.0f && d4 <= d3)
            {
                return b;
            }

            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                const float v = d1 / (d1 - d3);
                return a + ab * v;
            }

            const Vec3 cp = p - c;
            const float d5 = ab.dot(cp);
            const float d6 = ac.dot(cp);
            if (d6 >= 0.0f && d5 <= d6)
            {
                return c;
            }

            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            {
                const float w = d2 / (d2 - d6);
                return a + ac * w;
            }

            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            {
                const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return b + (c - b) * w;
            }

            const float denom = 1.0f / (va + vb + vc);
            const float v = vb * denom;
            const float w = vc * denom;
            return a + ab * v + ac * w;
        }

        bool IntersectRayTriangle(const Vec3& origin, const Vec3& dir, float maxDist,
                                  const Vec3& v0, const Vec3& v1, const Vec3& v2,
                                  float& outT, Vec3& outNormal)
        {
            const Vec3 edge1 = v1 - v0;
            const Vec3 edge2 = v2 - v0;
            const Vec3 h = dir.cross(edge2);
            const float det = edge1.dot(h);
            if (std::fabs(det) <= PHYSICS_EPSILON)
            {
                return false;
            }

            const float invDet = 1.0f / det;
            const Vec3 s = origin - v0;
            const float u = s.dot(h) * invDet;
            if (u < 0.0f || u > 1.0f)
            {
                return false;
            }

            const Vec3 q = s.cross(edge1);
            const float v = dir.dot(q) * invDet;
            if (v < 0.0f || u + v > 1.0f)
            {
                return false;
            }

            const float t = edge2.dot(q) * invDet;
            if (t < 0.0f || t > maxDist)
            {
                return false;
            }

            Vec3 normal = edge1.cross(edge2);
            const float nLen = normal.length();
            if (nLen > PHYSICS_EPSILON)
            {
                normal = normal * (1.0f / nLen);
                if (normal.dot(dir) > 0.0f)
                {
                    normal = normal * -1.0f;
                }
            }
            else
            {
                normal = dir * -1.0f;
            }

            outT = t;
            outNormal = normal;
            return true;
        }

        void ProjectBoxOntoAxis(const Vec3& boxCenter, const Vec3& boxHalf, const Vec3& axis,
                                float& outMin, float& outMax)
        {
            const float c = boxCenter.dot(axis);
            const float r = std::fabs(axis.x) * boxHalf.x +
                            std::fabs(axis.y) * boxHalf.y +
                            std::fabs(axis.z) * boxHalf.z;
            outMin = c - r;
            outMax = c + r;
        }

        void ProjectTriangleOntoAxis(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& axis,
                                     float& outMin, float& outMax)
        {
            const float p0 = v0.dot(axis);
            const float p1 = v1.dot(axis);
            const float p2 = v2.dot(axis);
            outMin = std::min({ p0, p1, p2 });
            outMax = std::max({ p0, p1, p2 });
        }

        void ProjectPointsOntoAxis(const std::vector<Vec3>& vPoints, const Vec3& axis,
                                   float& outMin, float& outMax)
        {
            outMin = FLT_MAX;
            outMax = -FLT_MAX;
            for (const Vec3& pt : vPoints)
            {
                const float p = pt.dot(axis);
                outMin = std::min(outMin, p);
                outMax = std::max(outMax, p);
            }
        }

        SContactManifold TestBoxTriangle(const Vec3& boxMin, const Vec3& boxMax,
                                         const Vec3& v0, const Vec3& v1, const Vec3& v2,
                                         const Vec3& triNormal)
        {
            SContactManifold manifold;
            const Vec3 boxCenter = (boxMin + boxMax) * 0.5f;
            const Vec3 boxHalf = (boxMax - boxMin) * 0.5f;
            const Vec3 triCenter = (v0 + v1 + v2) * (1.0f / 3.0f);

            const Vec3 aBoxAxes[3] = {
                { 1.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f }
            };
            const Vec3 aEdges[3] = { v1 - v0, v2 - v1, v0 - v2 };

            float minOverlap = FLT_MAX;
            Vec3 bestAxis = { 0.0f, 1.0f, 0.0f };

            auto testAxis = [&](const Vec3& rawAxis) -> bool
            {
                const float lenSq = rawAxis.lengthSquared();
                if (lenSq <= 1e-10f)
                {
                    return true;
                }
                const Vec3 axis = rawAxis * (1.0f / std::sqrt(lenSq));
                float bMin = 0.0f;
                float bMax = 0.0f;
                float tMin = 0.0f;
                float tMax = 0.0f;
                ProjectBoxOntoAxis(boxCenter, boxHalf, axis, bMin, bMax);
                ProjectTriangleOntoAxis(v0, v1, v2, axis, tMin, tMax);
                const float overlap = std::min(bMax, tMax) - std::max(bMin, tMin);
                if (overlap <= 0.0f)
                {
                    return false;
                }
                if (overlap < minOverlap)
                {
                    minOverlap = overlap;
                    bestAxis = axis;
                }
                return true;
            };

            for (int i = 0; i < 3; ++i)
            {
                if (!testAxis(aBoxAxes[i]))
                {
                    return manifold;
                }
            }
            if (!testAxis(triNormal))
            {
                return manifold;
            }
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                {
                    if (!testAxis(aBoxAxes[i].cross(aEdges[j])))
                    {
                        return manifold;
                    }
                }
            }

            if ((triCenter - boxCenter).dot(bestAxis) < 0.0f)
            {
                bestAxis = bestAxis * -1.0f;
            }

            manifold.m_Normal = bestAxis;
            manifold.m_Penetration = minOverlap;
            manifold.m_Point = ClosestPointOnTriangle(boxCenter, v0, v1, v2);
            manifold.m_HasContact = true;
            return manifold;
        }

        void SolveJacobi3x3(float aMatrix[3][3], float outEigenValues[3], float outEigenVectors[3][3])
        {
            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    outEigenVectors[r][c] = (r == c) ? 1.0f : 0.0f;
                }
            }

            for (int iter = 0; iter < 24; ++iter)
            {
                int p = 0;
                int q = 1;
                float maxOffDiag = std::fabs(aMatrix[0][1]);
                if (std::fabs(aMatrix[0][2]) > maxOffDiag)
                {
                    maxOffDiag = std::fabs(aMatrix[0][2]);
                    p = 0;
                    q = 2;
                }
                if (std::fabs(aMatrix[1][2]) > maxOffDiag)
                {
                    maxOffDiag = std::fabs(aMatrix[1][2]);
                    p = 1;
                    q = 2;
                }
                if (maxOffDiag < 1e-7f)
                {
                    break;
                }

                const float app = aMatrix[p][p];
                const float aqq = aMatrix[q][q];
                const float apq = aMatrix[p][q];
                const float tau = (aqq - app) / (2.0f * apq);
                const float t = (tau >= 0.0f)
                    ? (1.0f / (tau + std::sqrt(1.0f + tau * tau)))
                    : (-1.0f / (-tau + std::sqrt(1.0f + tau * tau)));
                const float c = 1.0f / std::sqrt(1.0f + t * t);
                const float s = t * c;

                aMatrix[p][p] = app - t * apq;
                aMatrix[q][q] = aqq + t * apq;
                aMatrix[p][q] = 0.0f;
                aMatrix[q][p] = 0.0f;

                for (int r = 0; r < 3; ++r)
                {
                    if (r != p && r != q)
                    {
                        const float arp = aMatrix[r][p];
                        const float arq = aMatrix[r][q];
                        aMatrix[r][p] = c * arp - s * arq;
                        aMatrix[p][r] = aMatrix[r][p];
                        aMatrix[r][q] = s * arp + c * arq;
                        aMatrix[q][r] = aMatrix[r][q];
                    }
                    const float vrp = outEigenVectors[r][p];
                    const float vrq = outEigenVectors[r][q];
                    outEigenVectors[r][p] = c * vrp - s * vrq;
                    outEigenVectors[r][q] = s * vrp + c * vrq;
                }
            }

            const Vec3 col0 = { outEigenVectors[0][0], outEigenVectors[1][0], outEigenVectors[2][0] };
            const Vec3 col1 = { outEigenVectors[0][1], outEigenVectors[1][1], outEigenVectors[2][1] };
            const Vec3 col2 = { outEigenVectors[0][2], outEigenVectors[1][2], outEigenVectors[2][2] };
            if (col0.cross(col1).dot(col2) < 0.0f)
            {
                outEigenVectors[0][2] = -outEigenVectors[0][2];
                outEigenVectors[1][2] = -outEigenVectors[1][2];
                outEigenVectors[2][2] = -outEigenVectors[2][2];
            }

            outEigenValues[0] = std::max(aMatrix[0][0], 0.0001f);
            outEigenValues[1] = std::max(aMatrix[1][1], 0.0001f);
            outEigenValues[2] = std::max(aMatrix[2][2], 0.0001f);
        }

        Vec3 RotationMatrixToEulerDeg(const float m[3][3])
        {
            const float sy = std::sqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0]);
            float rx = 0.0f;
            float ry = 0.0f;
            float rz = 0.0f;
            if (sy > 1e-5f)
            {
                rx = std::atan2(m[2][1], m[2][2]);
                ry = std::atan2(-m[2][0], sy);
                rz = std::atan2(m[1][0], m[0][0]);
            }
            else
            {
                rx = std::atan2(-m[1][2], m[1][1]);
                ry = std::atan2(-m[2][0], sy);
                rz = 0.0f;
            }
            auto snapZero = [](float deg)
            {
                return (std::fabs(deg) < 0.001f) ? 0.0f : deg;
            };
            return { snapZero(rx * RAD2DEG), snapZero(ry * RAD2DEG), snapZero(rz * RAD2DEG) };
        }

        float ShortestAngleDeltaDeg(float fromDeg, float toDeg)
        {
            float diff = std::fmod(toDeg - fromDeg, 360.0f);
            if (diff > 180.0f)
            {
                diff -= 360.0f;
            }
            else if (diff < -180.0f)
            {
                diff += 360.0f;
            }
            return diff;
        }

        Vec3 LerpEulerAnglesDeg(const Vec3& fromDeg, const Vec3& toDeg, float t)
        {
            return {
                fromDeg.x + ShortestAngleDeltaDeg(fromDeg.x, toDeg.x) * t,
                fromDeg.y + ShortestAngleDeltaDeg(fromDeg.y, toDeg.y) * t,
                fromDeg.z + ShortestAngleDeltaDeg(fromDeg.z, toDeg.z) * t
            };
        }

        Mat4 ComposeRotationMatrix(const Vec3& eulerDeg)
        {
            return Mat4::rotationY(eulerDeg.y * DEG2RAD) *
                   Mat4::rotationX(eulerDeg.x * DEG2RAD) *
                   Mat4::rotationZ(eulerDeg.z * DEG2RAD);
        }

        Vec3 DecomposeRotationMatrix(const Mat4& rot)
        {
            const Vec3 right = { rot.m[0], rot.m[1], rot.m[2] };
            const Vec3 up = { rot.m[4], rot.m[5], rot.m[6] };
            const Vec3 dir = { rot.m[8], rot.m[9], rot.m[10] };
            const float sinPitch = std::clamp(-dir.y, -1.0f, 1.0f);
            const float pitch = std::asin(sinPitch);
            const float cosPitch = std::cos(pitch);
            float yaw = 0.0f;
            float roll = 0.0f;
            if (std::fabs(cosPitch) > 1e-4f)
            {
                yaw = std::atan2(dir.x, dir.z);
                roll = std::atan2(right.y, up.y);
            }
            else
            {
                yaw = std::atan2(-right.z, right.x);
                roll = 0.0f;
            }
            return { pitch * RAD2DEG, yaw * RAD2DEG, roll * RAD2DEG };
        }

        Mat4 AxisAngleMatrixRad(const Vec3& unitAxis, float angleRad)
        {
            const float c = std::cos(angleRad);
            const float s = std::sin(angleRad);
            const float t = 1.0f - c;
            const float x = unitAxis.x;
            const float y = unitAxis.y;
            const float z = unitAxis.z;

            Mat4 m = Mat4::identity();
            m.m[0] = t * x * x + c;
            m.m[1] = t * x * y + s * z;
            m.m[2] = t * x * z - s * y;

            m.m[4] = t * x * y - s * z;
            m.m[5] = t * y * y + c;
            m.m[6] = t * y * z + s * x;

            m.m[8] = t * x * z + s * y;
            m.m[9] = t * y * z - s * x;
            m.m[10] = t * z * z + c;
            return m;
        }

        void IntegrateWorldAngularRotation(CTransformComponent& transform, const Vec3& angularVelRad, float dt)
        {
            const float angleRad = angularVelRad.length() * dt;
            if (angleRad <= 1e-7f)
            {
                return;
            }
            const Vec3 unitAxis = angularVelRad * (dt / angleRad);
            const Mat4 deltaRot = AxisAngleMatrixRad(unitAxis, angleRad);
            const Mat4 currentRot = ComposeRotationMatrix(transform.m_Rotation);
            transform.ClearLocalMatrixOverride();
            transform.m_Rotation = DecomposeRotationMatrix(deltaRot * currentRot);
        }

        bool IsNonConvexMeshEntity(const CEntity& entity)
        {
            const CCollisionComponent* pCol = entity.GetCollisionComponent();
            return pCol && pCol->m_Enabled &&
                   pCol->m_ColliderType == COLLIDER_MESH &&
                   !pCol->m_Convex;
        }

        Vec3 ComputeInverseInertia(const CEntity& entity, const CRigidbodyComponent* pBody)
        {
            if (!pBody || !pBody->m_Enabled || pBody->m_IsKinematic || IsNonConvexMeshEntity(entity))
            {
                return { 0.0f, 0.0f, 0.0f };
            }

            const SInertiaTensorInfo info = CPhysicsWorld::ComputeInertiaTensor(entity, pBody->m_Mass);
            const Vec3 inertia = info.m_Diagonal;

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

    SInertiaTensorInfo CPhysicsWorld::ComputeInertiaTensor(const CEntity& entity, float rawMass)
    {
        SInertiaTensorInfo result;
        const float mass = std::max(rawMass, 0.001f);
        result.m_Diagonal = { (1.0f / 6.0f) * mass, (1.0f / 6.0f) * mass, (1.0f / 6.0f) * mass };
        result.m_RotationDeg = { 0.0f, 0.0f, 0.0f };

        const CCollisionComponent* pCollider = entity.GetCollisionComponent();
        const CTransformComponent* pTransform = entity.GetTransformComponent();
        if (!pCollider)
        {
            return result;
        }

        const Vec3 scale = pTransform ? pTransform->m_Scale : Vec3{ 1.0f, 1.0f, 1.0f };
        if (pCollider->m_ColliderType == COLLIDER_SPHERE)
        {
            const float r = std::max(pCollider->m_Radius * std::max({ std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z) }), 0.001f);
            const float iSphere = 0.4f * mass * r * r;
            result.m_Diagonal = { iSphere, iSphere, iSphere };
            return result;
        }

        if (pCollider->m_ColliderType == COLLIDER_CAPSULE)
        {
            float radialScale = std::max(std::fabs(scale.x), std::fabs(scale.z));
            float heightScale = std::fabs(scale.y);
            if (pCollider->m_Direction == ECapsuleDirection::X_AXIS)
            {
                radialScale = std::max(std::fabs(scale.y), std::fabs(scale.z));
                heightScale = std::fabs(scale.x);
            }
            else if (pCollider->m_Direction == ECapsuleDirection::Z_AXIS)
            {
                radialScale = std::max(std::fabs(scale.x), std::fabs(scale.y));
                heightScale = std::fabs(scale.z);
            }

            const float r = std::max(pCollider->m_Radius * radialScale, 0.001f);
            const float h = std::max(pCollider->m_Height * heightScale, r * 2.0f);
            const float iAxis = 0.5f * mass * r * r;
            const float iPerp = (1.0f / 12.0f) * mass * (3.0f * r * r + h * h);

            if (pCollider->m_Direction == ECapsuleDirection::X_AXIS)
            {
                result.m_Diagonal = { iAxis, iPerp, iPerp };
                result.m_RotationDeg = { 0.0f, 0.0f, 90.0f };
            }
            else if (pCollider->m_Direction == ECapsuleDirection::Z_AXIS)
            {
                result.m_Diagonal = { iPerp, iPerp, iAxis };
                result.m_RotationDeg = { 90.0f, 0.0f, 0.0f };
            }
            else
            {
                result.m_Diagonal = { iPerp, iAxis, iPerp };
                result.m_RotationDeg = { 0.0f, 0.0f, 0.0f };
            }
            return result;
        }

        if (pCollider->m_ColliderType == COLLIDER_MESH)
        {
            const CMeshComponent* pMeshComp = entity.GetMeshComponent();
            std::vector<Vec3> vLocalPts;
            if (pMeshComp)
            {
                if (pMeshComp->m_Model.meshCount > 0 && pMeshComp->m_Model.meshes)
                {
                    for (int m = 0; m < pMeshComp->m_Model.meshCount; ++m)
                    {
                        const Mesh& mesh = pMeshComp->m_Model.meshes[m];
                        if (!mesh.vertices || mesh.vertexCount <= 0)
                        {
                            continue;
                        }
                        vLocalPts.reserve(vLocalPts.size() + static_cast<size_t>(mesh.vertexCount));
                        for (int v = 0; v < mesh.vertexCount; ++v)
                        {
                            const Vec3 raw{
                                mesh.vertices[v * 3 + 0],
                                mesh.vertices[v * 3 + 1],
                                mesh.vertices[v * 3 + 2]
                            };
                            const Vec3 transformed = Vec3(pMeshComp->m_Model.transform * raw);
                            vLocalPts.push_back({
                                transformed.x * scale.x,
                                transformed.y * scale.y,
                                transformed.z * scale.z
                            });
                        }
                    }
                }
                else if (pMeshComp->m_IsEditableMesh && !pMeshComp->m_EditableMesh.m_vVertices.empty())
                {
                    vLocalPts.reserve(pMeshComp->m_EditableMesh.m_vVertices.size());
                    for (const SEditableVertex& vert : pMeshComp->m_EditableMesh.m_vVertices)
                    {
                        vLocalPts.push_back({
                            vert.m_Position.x * scale.x,
                            vert.m_Position.y * scale.y,
                            vert.m_Position.z * scale.z
                        });
                    }
                }
            }

            if (vLocalPts.size() >= 3)
            {
                Vec3 mean{ 0.0f, 0.0f, 0.0f };
                for (const Vec3& pt : vLocalPts)
                {
                    mean = mean + pt;
                }
                mean = mean * (1.0f / static_cast<float>(vLocalPts.size()));

                float aTensor[3][3] = {};
                const float pointMass = mass / static_cast<float>(vLocalPts.size());
                for (const Vec3& pt : vLocalPts)
                {
                    const Vec3 r = pt - mean;
                    const float rx2 = r.x * r.x;
                    const float ry2 = r.y * r.y;
                    const float rz2 = r.z * r.z;
                    aTensor[0][0] += pointMass * (ry2 + rz2);
                    aTensor[1][1] += pointMass * (rx2 + rz2);
                    aTensor[2][2] += pointMass * (rx2 + ry2);
                    aTensor[0][1] -= pointMass * (r.x * r.y);
                    aTensor[0][2] -= pointMass * (r.x * r.z);
                    aTensor[1][2] -= pointMass * (r.y * r.z);
                }
                aTensor[1][0] = aTensor[0][1];
                aTensor[2][0] = aTensor[0][2];
                aTensor[2][1] = aTensor[1][2];

                float aEigenVals[3] = {};
                float aEigenVecs[3][3] = {};
                SolveJacobi3x3(aTensor, aEigenVals, aEigenVecs);
                result.m_Diagonal = { aEigenVals[0], aEigenVals[1], aEigenVals[2] };
                result.m_RotationDeg = RotationMatrixToEulerDeg(aEigenVecs);
                return result;
            }
        }

        const float sx = std::max(std::fabs(pCollider->m_Size.x * scale.x), 0.001f);
        const float sy = std::max(std::fabs(pCollider->m_Size.y * scale.y), 0.001f);
        const float sz = std::max(std::fabs(pCollider->m_Size.z * scale.z), 0.001f);
        result.m_Diagonal = {
            (1.0f / 12.0f) * mass * (sy * sy + sz * sz),
            (1.0f / 12.0f) * mass * (sx * sx + sz * sz),
            (1.0f / 12.0f) * mass * (sx * sx + sy * sy)
        };
        return result;
    }

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
        wc.m_Convex = collider.m_Convex;

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
        else if (collider.m_ColliderType == COLLIDER_MESH)
        {
            const CEntity& entity = scene.m_vEntities[static_cast<size_t>(entityIndex)];
            const CMeshComponent* pMeshComp = entity.GetMeshComponent();
            Vec3 bMin{ FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 bMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

            auto appendTriangle = [&](const Vec3& w0, const Vec3& w1, const Vec3& w2)
            {
                const Vec3 e1 = w1 - w0;
                const Vec3 e2 = w2 - w0;
                Vec3 n = e1.cross(e2);
                const float nLen = n.length();
                if (nLen <= PHYSICS_EPSILON)
                {
                    return;
                }
                n = n * (1.0f / nLen);

                STriangle tri;
                tri.m_V0 = w0;
                tri.m_V1 = w1;
                tri.m_V2 = w2;
                tri.m_Normal = n;
                tri.m_Bounds.min = {
                    std::min({ w0.x, w1.x, w2.x }),
                    std::min({ w0.y, w1.y, w2.y }),
                    std::min({ w0.z, w1.z, w2.z })
                };
                tri.m_Bounds.max = {
                    std::max({ w0.x, w1.x, w2.x }),
                    std::max({ w0.y, w1.y, w2.y }),
                    std::max({ w0.z, w1.z, w2.z })
                };
                wc.m_vTriangles.push_back(tri);

                bMin.x = std::min(bMin.x, tri.m_Bounds.min.x);
                bMin.y = std::min(bMin.y, tri.m_Bounds.min.y);
                bMin.z = std::min(bMin.z, tri.m_Bounds.min.z);
                bMax.x = std::max(bMax.x, tri.m_Bounds.max.x);
                bMax.y = std::max(bMax.y, tri.m_Bounds.max.y);
                bMax.z = std::max(bMax.z, tri.m_Bounds.max.z);
            };

            const Vec3 centerOffset = Vec3(world * collider.m_Center) - Vec3(world * Vec3{ 0.0f, 0.0f, 0.0f });
            if (pMeshComp && pMeshComp->m_Model.meshCount > 0 && pMeshComp->m_Model.meshes)
            {
                const Mat4 meshWorld = world * pMeshComp->m_Model.transform;
                for (int m = 0; m < pMeshComp->m_Model.meshCount; ++m)
                {
                    const Mesh& mesh = pMeshComp->m_Model.meshes[m];
                    if (!mesh.vertices || mesh.vertexCount <= 0)
                    {
                        continue;
                    }
                    const auto readVertex = [&](int idx) -> Vec3
                    {
                        const Vec3 local{
                            mesh.vertices[idx * 3 + 0],
                            mesh.vertices[idx * 3 + 1],
                            mesh.vertices[idx * 3 + 2]
                        };
                        return Vec3(meshWorld * local) + centerOffset;
                    };
                    if (mesh.indices && mesh.triangleCount > 0)
                    {
                        wc.m_vTriangles.reserve(wc.m_vTriangles.size() + static_cast<size_t>(mesh.triangleCount));
                        for (int t = 0; t < mesh.triangleCount; ++t)
                        {
                            const int i0 = mesh.indices[t * 3 + 0];
                            const int i1 = mesh.indices[t * 3 + 1];
                            const int i2 = mesh.indices[t * 3 + 2];
                            if (i0 < mesh.vertexCount && i1 < mesh.vertexCount && i2 < mesh.vertexCount)
                            {
                                appendTriangle(readVertex(i0), readVertex(i1), readVertex(i2));
                            }
                        }
                    }
                    else if (mesh.vertexCount >= 3)
                    {
                        const int triCount = mesh.vertexCount / 3;
                        wc.m_vTriangles.reserve(wc.m_vTriangles.size() + static_cast<size_t>(triCount));
                        for (int t = 0; t < triCount; ++t)
                        {
                            appendTriangle(readVertex(t * 3 + 0), readVertex(t * 3 + 1), readVertex(t * 3 + 2));
                        }
                    }
                }
            }
            else if (pMeshComp && pMeshComp->m_IsEditableMesh &&
                     !pMeshComp->m_EditableMesh.m_vVertices.empty() &&
                     !pMeshComp->m_EditableMesh.m_vTriangles.empty())
            {
                const auto& vVerts = pMeshComp->m_EditableMesh.m_vVertices;
                const auto& vTris = pMeshComp->m_EditableMesh.m_vTriangles;
                wc.m_vTriangles.reserve(vTris.size());
                for (const SEditableTriangle& tri : vTris)
                {
                    if (tri.m_A >= 0 && tri.m_B >= 0 && tri.m_C >= 0 &&
                        static_cast<size_t>(tri.m_A) < vVerts.size() &&
                        static_cast<size_t>(tri.m_B) < vVerts.size() &&
                        static_cast<size_t>(tri.m_C) < vVerts.size())
                    {
                        appendTriangle(
                            Vec3(world * vVerts[static_cast<size_t>(tri.m_A)].m_Position) + centerOffset,
                            Vec3(world * vVerts[static_cast<size_t>(tri.m_B)].m_Position) + centerOffset,
                            Vec3(world * vVerts[static_cast<size_t>(tri.m_C)].m_Position) + centerOffset);
                    }
                }
            }

            if (!wc.m_vTriangles.empty())
            {
                wc.m_Type = COLLIDER_MESH;
                wc.m_BoxMin = bMin;
                wc.m_BoxMax = bMax;
                wc.m_BroadphaseBounds = { bMin, bMax };
                wc.m_Center = (bMin + bMax) * 0.5f;

                if (wc.m_Convex)
                {
                    wc.m_vConvexVertices.reserve(wc.m_vTriangles.size() * 3);
                    auto addUniqueVertex = [&](const Vec3& pt)
                    {
                        for (const Vec3& existing : wc.m_vConvexVertices)
                        {
                            if ((existing - pt).lengthSquared() <= 1e-8f)
                            {
                                return;
                            }
                        }
                        wc.m_vConvexVertices.push_back(pt);
                    };
                    for (const STriangle& tri : wc.m_vTriangles)
                    {
                        addUniqueVertex(tri.m_V0);
                        addUniqueVertex(tri.m_V1);
                        addUniqueVertex(tri.m_V2);
                    }
                }
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
                wc.m_BoxAxisX = axisX * halfLocal.x;
                wc.m_BoxAxisY = axisY * halfLocal.y;
                wc.m_BoxAxisZ = axisZ * halfLocal.z;
                wc.m_BoxMin = wc.m_Center - halfWorld;
                wc.m_BoxMax = wc.m_Center + halfWorld;
                wc.m_BroadphaseBounds = { wc.m_BoxMin, wc.m_BoxMax };
            }
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
            wc.m_BoxAxisX = axisX * halfLocal.x;
            wc.m_BoxAxisY = axisY * halfLocal.y;
            wc.m_BoxAxisZ = axisZ * halfLocal.z;
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
            SContactManifold m = TestBoxBox(a.m_BoxMin, a.m_BoxMax, b.m_BoxMin, b.m_BoxMax);
            if (m.m_HasContact)
            {
                const float volA = (a.m_BoxMax.x - a.m_BoxMin.x) *
                                   (a.m_BoxMax.y - a.m_BoxMin.y) *
                                   (a.m_BoxMax.z - a.m_BoxMin.z);
                const float volB = (b.m_BoxMax.x - b.m_BoxMin.x) *
                                   (b.m_BoxMax.y - b.m_BoxMin.y) *
                                   (b.m_BoxMax.z - b.m_BoxMin.z);
                const SWorldCollider& featureBox = (volA <= volB) ? a : b;
                const Vec3 supportDir = (volA <= volB) ? m.m_Normal : (m.m_Normal * -1.0f);

                Vec3 aCorners[8];
                int cIdx = 0;
                for (int sx = -1; sx <= 1; sx += 2)
                {
                    for (int sy = -1; sy <= 1; sy += 2)
                    {
                        for (int sz = -1; sz <= 1; sz += 2)
                        {
                            aCorners[cIdx++] = featureBox.m_Center +
                                featureBox.m_BoxAxisX * static_cast<float>(sx) +
                                featureBox.m_BoxAxisY * static_cast<float>(sy) +
                                featureBox.m_BoxAxisZ * static_cast<float>(sz);
                        }
                    }
                }

                float maxProj = -FLT_MAX;
                for (int k = 0; k < 8; ++k)
                {
                    maxProj = std::max(maxProj, aCorners[k].dot(supportDir));
                }

                Vec3 contactSum = { 0.0f, 0.0f, 0.0f };
                int contactCount = 0;
                for (int k = 0; k < 8; ++k)
                {
                    if (aCorners[k].dot(supportDir) >= maxProj - 0.02f)
                    {
                        contactSum = contactSum + aCorners[k];
                        ++contactCount;
                    }
                }
                if (contactCount > 0)
                {
                    m.m_Point = contactSum * (1.0f / static_cast<float>(contactCount));
                }
            }
            return m;
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

        auto testConvexHullVsPoints = [](const std::vector<Vec3>& vPtsA,
                                         const Vec3& centerA,
                                         const std::vector<STriangle>& vTrisA,
                                         const std::vector<Vec3>& vPtsB,
                                         const Vec3& centerB,
                                         const std::vector<STriangle>& vTrisB) -> SContactManifold
        {
            SContactManifold manifold;
            if (vPtsA.empty() || vPtsB.empty())
            {
                return manifold;
            }

            float minOverlap = FLT_MAX;
            Vec3 bestAxis = { 0.0f, 1.0f, 0.0f };
            std::vector<Vec3> vTestedAxes;
            vTestedAxes.reserve(vTrisA.size() + vTrisB.size() + 6);

            auto testAxis = [&](const Vec3& rawAxis) -> bool
            {
                const float lenSq = rawAxis.lengthSquared();
                if (lenSq <= 1e-10f)
                {
                    return true;
                }
                const Vec3 axis = rawAxis * (1.0f / std::sqrt(lenSq));
                for (const Vec3& prev : vTestedAxes)
                {
                    if (std::fabs(prev.dot(axis)) > 0.9995f)
                    {
                        return true;
                    }
                }
                vTestedAxes.push_back(axis);

                float minA = 0.0f;
                float maxA = 0.0f;
                float minB = 0.0f;
                float maxB = 0.0f;
                ProjectPointsOntoAxis(vPtsA, axis, minA, maxA);
                ProjectPointsOntoAxis(vPtsB, axis, minB, maxB);
                const float overlap = std::min(maxA, maxB) - std::max(minA, minB);
                if (overlap <= 0.0f)
                {
                    return false;
                }
                if (overlap < minOverlap)
                {
                    minOverlap = overlap;
                    bestAxis = axis;
                }
                return true;
            };

            const Vec3 aCardinal[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
            for (const Vec3& axis : aCardinal)
            {
                if (!testAxis(axis))
                {
                    return manifold;
                }
            }
            for (const STriangle& tri : vTrisA)
            {
                if (!testAxis(tri.m_Normal))
                {
                    return manifold;
                }
            }
            for (const STriangle& tri : vTrisB)
            {
                if (!testAxis(tri.m_Normal))
                {
                    return manifold;
                }
            }
            if (!testAxis(centerB - centerA))
            {
                return manifold;
            }

            if ((centerB - centerA).dot(bestAxis) < 0.0f)
            {
                bestAxis = bestAxis * -1.0f;
            }

            float minProjB = FLT_MAX;
            for (const Vec3& pt : vPtsB)
            {
                minProjB = std::min(minProjB, pt.dot(bestAxis));
            }
            Vec3 contactSum{ 0.0f, 0.0f, 0.0f };
            int contactCount = 0;
            for (const Vec3& pt : vPtsB)
            {
                if (pt.dot(bestAxis) <= minProjB + 0.01f)
                {
                    contactSum = contactSum + pt;
                    ++contactCount;
                }
            }

            manifold.m_Normal = bestAxis;
            manifold.m_Penetration = minOverlap;
            manifold.m_Point = (contactCount > 0)
                ? (contactSum * (1.0f / static_cast<float>(contactCount)))
                : ((centerA + centerB) * 0.5f);
            manifold.m_HasContact = true;
            return manifold;
        };

        auto testSphereVsMesh = [](const SWorldCollider& sphere, const SWorldCollider& mesh) -> SContactManifold
        {
            SContactManifold best;
            if (mesh.m_Convex && !mesh.m_vConvexVertices.empty())
            {
                float minOverlap = FLT_MAX;
                Vec3 bestAxis = { 0.0f, 1.0f, 0.0f };
                Vec3 closestVert = mesh.m_vConvexVertices[0];
                float closestVertDistSq = FLT_MAX;
                for (const Vec3& v : mesh.m_vConvexVertices)
                {
                    const float dSq = (v - sphere.m_Center).lengthSquared();
                    if (dSq < closestVertDistSq)
                    {
                        closestVertDistSq = dSq;
                        closestVert = v;
                    }
                }

                auto testAxis = [&](const Vec3& rawAxis) -> bool
                {
                    const float lenSq = rawAxis.lengthSquared();
                    if (lenSq <= 1e-10f)
                    {
                        return true;
                    }
                    const Vec3 axis = rawAxis * (1.0f / std::sqrt(lenSq));
                    const float sCenter = sphere.m_Center.dot(axis);
                    const float sMin = sCenter - sphere.m_Radius;
                    const float sMax = sCenter + sphere.m_Radius;
                    float mMin = 0.0f;
                    float mMax = 0.0f;
                    ProjectPointsOntoAxis(mesh.m_vConvexVertices, axis, mMin, mMax);
                    const float overlap = std::min(sMax, mMax) - std::max(sMin, mMin);
                    if (overlap <= 0.0f)
                    {
                        return false;
                    }
                    if (overlap < minOverlap)
                    {
                        minOverlap = overlap;
                        bestAxis = axis;
                    }
                    return true;
                };

                const Vec3 aCardinal[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
                for (const Vec3& axis : aCardinal)
                {
                    if (!testAxis(axis))
                    {
                        return best;
                    }
                }
                for (const STriangle& tri : mesh.m_vTriangles)
                {
                    if (!testAxis(tri.m_Normal))
                    {
                        return best;
                    }
                }
                if (!testAxis(closestVert - sphere.m_Center))
                {
                    return best;
                }
                if (!testAxis(mesh.m_Center - sphere.m_Center))
                {
                    return best;
                }

                if ((mesh.m_Center - sphere.m_Center).dot(bestAxis) < 0.0f)
                {
                    bestAxis = bestAxis * -1.0f;
                }
                best.m_Normal = bestAxis;
                best.m_Penetration = minOverlap;
                best.m_Point = sphere.m_Center + bestAxis * (sphere.m_Radius - minOverlap * 0.5f);
                best.m_HasContact = true;
                return best;
            }

            const float radiusSq = sphere.m_Radius * sphere.m_Radius;
            for (const STriangle& tri : mesh.m_vTriangles)
            {
                if (!OverlapsAabb(sphere.m_BroadphaseBounds, tri.m_Bounds))
                {
                    continue;
                }
                const Vec3 closest = ClosestPointOnTriangle(sphere.m_Center, tri.m_V0, tri.m_V1, tri.m_V2);
                const Vec3 delta = closest - sphere.m_Center;
                const float distSq = delta.lengthSquared();
                if (distSq >= radiusSq)
                {
                    continue;
                }

                const float dist = std::sqrt(distSq);
                Vec3 normal = (dist > PHYSICS_EPSILON) ? (delta * (1.0f / dist)) : (tri.m_Normal * -1.0f);
                const float pen = sphere.m_Radius - dist;
                if (!best.m_HasContact || pen > best.m_Penetration)
                {
                    best.m_HasContact = true;
                    best.m_Penetration = pen;
                    best.m_Normal = normal;
                    best.m_Point = closest;
                }
            }
            return best;
        };

        auto testCapsuleVsMesh = [&](const SWorldCollider& capsule, const SWorldCollider& mesh) -> SContactManifold
        {
            if (mesh.m_Convex && !mesh.m_vConvexVertices.empty())
            {
                const Vec3 mid = (capsule.m_SegmentA + capsule.m_SegmentB) * 0.5f;
                SWorldCollider sA = capsule;
                sA.m_Type = COLLIDER_SPHERE;
                sA.m_Center = capsule.m_SegmentA;
                SWorldCollider sB = capsule;
                sB.m_Type = COLLIDER_SPHERE;
                sB.m_Center = capsule.m_SegmentB;
                SWorldCollider sM = capsule;
                sM.m_Type = COLLIDER_SPHERE;
                sM.m_Center = mid;
                SContactManifold best = testSphereVsMesh(sA, mesh);
                const SContactManifold mB = testSphereVsMesh(sB, mesh);
                if (mB.m_HasContact && (!best.m_HasContact || mB.m_Penetration > best.m_Penetration))
                {
                    best = mB;
                }
                const SContactManifold mM = testSphereVsMesh(sM, mesh);
                if (mM.m_HasContact && (!best.m_HasContact || mM.m_Penetration > best.m_Penetration))
                {
                    best = mM;
                }
                return best;
            }

            SContactManifold best;
            const float radiusSq = capsule.m_Radius * capsule.m_Radius;
            for (const STriangle& tri : mesh.m_vTriangles)
            {
                if (!OverlapsAabb(capsule.m_BroadphaseBounds, tri.m_Bounds))
                {
                    continue;
                }

                Vec3 bestSegPt = capsule.m_SegmentA;
                Vec3 bestTriPt = ClosestPointOnTriangle(capsule.m_SegmentA, tri.m_V0, tri.m_V1, tri.m_V2);
                float bestDistSq = (bestTriPt - bestSegPt).lengthSquared();

                auto tryPair = [&](const Vec3& segPt, const Vec3& triPt)
                {
                    const float dSq = (triPt - segPt).lengthSquared();
                    if (dSq < bestDistSq)
                    {
                        bestDistSq = dSq;
                        bestSegPt = segPt;
                        bestTriPt = triPt;
                    }
                };

                tryPair(capsule.m_SegmentB, ClosestPointOnTriangle(capsule.m_SegmentB, tri.m_V0, tri.m_V1, tri.m_V2));
                const Vec3 segMid = (capsule.m_SegmentA + capsule.m_SegmentB) * 0.5f;
                tryPair(segMid, ClosestPointOnTriangle(segMid, tri.m_V0, tri.m_V1, tri.m_V2));

                const Vec3 aVerts[3] = { tri.m_V0, tri.m_V1, tri.m_V2 };
                for (int e = 0; e < 3; ++e)
                {
                    Vec3 c1{};
                    Vec3 c2{};
                    ClosestPointsBetweenSegments(capsule.m_SegmentA, capsule.m_SegmentB,
                                                 aVerts[e], aVerts[(e + 1) % 3], c1, c2);
                    tryPair(c1, c2);
                }

                if (bestDistSq >= radiusSq)
                {
                    continue;
                }

                const float dist = std::sqrt(bestDistSq);
                const Vec3 delta = bestTriPt - bestSegPt;
                const Vec3 normal = (dist > PHYSICS_EPSILON) ? (delta * (1.0f / dist)) : (tri.m_Normal * -1.0f);
                const float pen = capsule.m_Radius - dist;
                if (!best.m_HasContact || pen > best.m_Penetration)
                {
                    best.m_HasContact = true;
                    best.m_Penetration = pen;
                    best.m_Normal = normal;
                    best.m_Point = bestTriPt;
                }
            }
            return best;
        };

        auto testBoxVsMesh = [&](const SWorldCollider& box, const SWorldCollider& mesh) -> SContactManifold
        {
            if (mesh.m_Convex && !mesh.m_vConvexVertices.empty())
            {
                const std::vector<Vec3> vBoxCorners = {
                    { box.m_BoxMin.x, box.m_BoxMin.y, box.m_BoxMin.z },
                    { box.m_BoxMax.x, box.m_BoxMin.y, box.m_BoxMin.z },
                    { box.m_BoxMin.x, box.m_BoxMax.y, box.m_BoxMin.z },
                    { box.m_BoxMax.x, box.m_BoxMax.y, box.m_BoxMin.z },
                    { box.m_BoxMin.x, box.m_BoxMin.y, box.m_BoxMax.z },
                    { box.m_BoxMax.x, box.m_BoxMin.y, box.m_BoxMax.z },
                    { box.m_BoxMin.x, box.m_BoxMax.y, box.m_BoxMax.z },
                    { box.m_BoxMax.x, box.m_BoxMax.y, box.m_BoxMax.z }
                };
                const std::vector<STriangle> vNoTris;
                return testConvexHullVsPoints(vBoxCorners, box.m_Center, vNoTris,
                                              mesh.m_vConvexVertices, mesh.m_Center, mesh.m_vTriangles);
            }

            SContactManifold best;
            for (const STriangle& tri : mesh.m_vTriangles)
            {
                if (!OverlapsAabb(box.m_BroadphaseBounds, tri.m_Bounds))
                {
                    continue;
                }
                const SContactManifold m = TestBoxTriangle(box.m_BoxMin, box.m_BoxMax,
                                                           tri.m_V0, tri.m_V1, tri.m_V2, tri.m_Normal);
                if (m.m_HasContact && (!best.m_HasContact || m.m_Penetration > best.m_Penetration))
                {
                    best = m;
                }
            }
            return best;
        };

        if (a.m_Type == COLLIDER_SPHERE && b.m_Type == COLLIDER_MESH)
        {
            return testSphereVsMesh(a, b);
        }
        if (a.m_Type == COLLIDER_MESH && b.m_Type == COLLIDER_SPHERE)
        {
            SContactManifold m = testSphereVsMesh(b, a);
            m.m_Normal = m.m_Normal * -1.0f;
            return m;
        }
        if (a.m_Type == COLLIDER_CAPSULE && b.m_Type == COLLIDER_MESH)
        {
            return testCapsuleVsMesh(a, b);
        }
        if (a.m_Type == COLLIDER_MESH && b.m_Type == COLLIDER_CAPSULE)
        {
            SContactManifold m = testCapsuleVsMesh(b, a);
            m.m_Normal = m.m_Normal * -1.0f;
            return m;
        }
        if (a.m_Type == COLLIDER_BOX && b.m_Type == COLLIDER_MESH)
        {
            return testBoxVsMesh(a, b);
        }
        if (a.m_Type == COLLIDER_MESH && b.m_Type == COLLIDER_BOX)
        {
            SContactManifold m = testBoxVsMesh(b, a);
            m.m_Normal = m.m_Normal * -1.0f;
            return m;
        }
        if (a.m_Type == COLLIDER_MESH && b.m_Type == COLLIDER_MESH)
        {
            if (a.m_Convex && b.m_Convex)
            {
                return testConvexHullVsPoints(a.m_vConvexVertices, a.m_Center, a.m_vTriangles,
                                              b.m_vConvexVertices, b.m_Center, b.m_vTriangles);
            }
            if (a.m_Convex && !b.m_Convex)
            {
                SContactManifold best;
                for (const STriangle& tri : b.m_vTriangles)
                {
                    if (!OverlapsAabb(a.m_BroadphaseBounds, tri.m_Bounds))
                    {
                        continue;
                    }
                    const std::vector<Vec3> vTriPts = { tri.m_V0, tri.m_V1, tri.m_V2 };
                    const std::vector<STriangle> vOneTri = { tri };
                    const Vec3 triCenter = (tri.m_V0 + tri.m_V1 + tri.m_V2) * (1.0f / 3.0f);
                    const SContactManifold m = testConvexHullVsPoints(
                        a.m_vConvexVertices, a.m_Center, a.m_vTriangles,
                        vTriPts, triCenter, vOneTri);
                    if (m.m_HasContact && (!best.m_HasContact || m.m_Penetration > best.m_Penetration))
                    {
                        best = m;
                    }
                }
                return best;
            }
            if (!a.m_Convex && b.m_Convex)
            {
                SContactManifold best;
                for (const STriangle& tri : a.m_vTriangles)
                {
                    if (!OverlapsAabb(b.m_BroadphaseBounds, tri.m_Bounds))
                    {
                        continue;
                    }
                    const std::vector<Vec3> vTriPts = { tri.m_V0, tri.m_V1, tri.m_V2 };
                    const std::vector<STriangle> vOneTri = { tri };
                    const Vec3 triCenter = (tri.m_V0 + tri.m_V1 + tri.m_V2) * (1.0f / 3.0f);
                    SContactManifold m = testConvexHullVsPoints(
                        b.m_vConvexVertices, b.m_Center, b.m_vTriangles,
                        vTriPts, triCenter, vOneTri);
                    if (m.m_HasContact && (!best.m_HasContact || m.m_Penetration > best.m_Penetration))
                    {
                        m.m_Normal = m.m_Normal * -1.0f;
                        best = m;
                    }
                }
                return best;
            }
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
                pTransform->m_Rotation = LerpEulerAnglesDeg(pBody->m_PrevRotation, pBody->m_PhysicsRotation, clampedAlpha);
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

            if (!pBody->m_IsKinematic && !IsNonConvexMeshEntity(entity) &&
                (pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS ||
                 pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS_DYNAMIC))
            {
                const float invMass = pBody->GetInverseMass();
                Vec3 estAccel = pBody->m_AccumulatedForce * invMass;
                if (pBody->m_UseGravity)
                {
                    estAccel = estAccel + m_Gravity * pBody->m_GravityScale;
                }
                const Vec3 estVel = pBody->m_Velocity + estAccel * fixedDeltaTime;
                float effectiveSpeed = estVel.length();
                if (pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS_DYNAMIC)
                {
                    for (int j = 0; j < entityCount; ++j)
                    {
                        if (j == i)
                        {
                            continue;
                        }
                        const CRigidbodyComponent* pOtherBody = scene.m_vEntities[static_cast<size_t>(j)].GetRigidbodyComponent();
                        if (pOtherBody && pOtherBody->m_Enabled && !pOtherBody->m_IsKinematic)
                        {
                            const float relSpeed = (estVel - pOtherBody->m_Velocity).length();
                            effectiveSpeed = std::max(effectiveSpeed, relSpeed);
                        }
                    }
                }
                const float travelDist = effectiveSpeed * fixedDeltaTime;
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

                if (pBody->m_IsKinematic || IsNonConvexMeshEntity(entity))
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
                    if (pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS ||
                        pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS_DYNAMIC)
                    {
                        const float dispLen = std::sqrt(dispLenSq);
                        const float minExtent = ComputeMinColliderExtent(entity);
                        if (dispLen > minExtent * 0.4f)
                        {
                            const bool staticOnly = (pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS);
                            const Vec3 dir = displacement * (1.0f / dispLen);
                            const Vec3 rayOrigin = Vec3(quark::ComposeWorld(scene, i) * Vec3{ 0.0f, 0.0f, 0.0f });
                            SPhysicsRaycastHit sweepHit;
                            if (RaycastInternal(scene, rayOrigin, dir, dispLen + minExtent, sweepHit, false, entity.m_Id, staticOnly))
                            {
                                const float safeDist = std::max(0.0f, sweepHit.m_Distance - minExtent * 0.85f);
                                if (safeDist < dispLen)
                                {
                                    displacement = dir * safeDist;
                                }
                            }
                        }
                    }
                    else if (pBody->m_CollisionDetection == ECollisionDetectionMode::CONTINUOUS_SPECULATIVE)
                    {
                        const float dispLen = std::sqrt(dispLenSq);
                        const float minExtent = ComputeMinColliderExtent(entity);
                        const Vec3 dir = displacement * (1.0f / dispLen);
                        const Vec3 rayOrigin = Vec3(quark::ComposeWorld(scene, i) * Vec3{ 0.0f, 0.0f, 0.0f });
                        SPhysicsRaycastHit specHit;
                        if (RaycastInternal(scene, rayOrigin, dir, dispLen + minExtent, specHit, false, entity.m_Id, false))
                        {
                            const float gap = std::max(0.0f, specHit.m_Distance - minExtent);
                            const float approachSpeed = -(pBody->m_Velocity.dot(specHit.m_Normal));
                            if (approachSpeed > 0.0f && gap < dispLen)
                            {
                                const float maxAllowedSpeed = gap / std::max(subDt, PHYSICS_EPSILON);
                                if (approachSpeed > maxAllowedSpeed)
                                {
                                    const float excess = approachSpeed - maxAllowedSpeed;
                                    pBody->m_Velocity = pBody->m_Velocity + specHit.m_Normal * excess;
                                    pBody->m_Velocity = FilterFrozenAxes(pBody->m_Velocity, pBody);
                                    displacement = pBody->m_Velocity * subDt;
                                }
                            }
                        }
                    }

                    ApplyWorldDeltaToEntity(scene, i, displacement);
                    anyTransformMoved = true;
                }

                if ( pBody->m_AngularVelocity.lengthSquared() > 1e-12f )
                {
                    IntegrateWorldAngularRotation(*pTransform, pBody->m_AngularVelocity, subDt);
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

                    const float invMassA = (pBodyA && !IsNonConvexMeshEntity(entityA)) ? pBodyA->GetInverseMass() : 0.0f;
                    const float invMassB = (pBodyB && !IsNonConvexMeshEntity(entityB)) ? pBodyB->GetInverseMass() : 0.0f;
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
                    const float baseFricA = pBodyA ? pBodyA->m_Friction : 0.5f;
                    const float baseFricB = pBodyB ? pBodyB->m_Friction : 0.5f;
                    const float fricA = pColCompA ? pColCompA->GetEffectiveFriction(baseFricA) : baseFricA;
                    const float fricB = pColCompB ? pColCompB->GetEffectiveFriction(baseFricB) : baseFricB;
                    const float mu = std::sqrt(std::max(0.0f, fricA * fricB));

                    if (velAlongNormal <= 0.0f)
                    {
                        const Vec3 rAxN = rA.cross(manifold.m_Normal);
                        const Vec3 rBxN = rB.cross(manifold.m_Normal);
                        const float angularDenomA = ComponentWiseMul(invInertiaA, rAxN).cross(rA).dot(manifold.m_Normal);
                        const float angularDenomB = ComponentWiseMul(invInertiaB, rBxN).cross(rB).dot(manifold.m_Normal);
                        const float effectiveInvMass = invMassSum + angularDenomA + angularDenomB;
                        if (effectiveInvMass > PHYSICS_EPSILON)
                        {
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
                                    const float normalLoad = std::max(impulseMag, (m_Gravity.length() * subDt) / invMassSum);
                                    const float clampedJt = std::clamp(jt, -normalLoad * mu, normalLoad * mu);
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

                    if (mu > PHYSICS_EPSILON)
                    {
                        auto applySurfaceDamping = [&](CEntity& ent, CRigidbodyComponent* pBody, const CCollisionComponent* pCol, float normalYSign)
                        {
                            if (!pBody || (manifold.m_Normal.y * normalYSign) < 0.5f)
                            {
                                return;
                            }
                            const float gravMag = std::max(std::fabs(m_Gravity.y) * std::fabs(pBody->m_GravityScale), 9.81f);
                            const float slideDecel = mu * gravMag * 1.35f * subDt;
                            Vec3 horizVel = { pBody->m_Velocity.x, 0.0f, pBody->m_Velocity.z };
                            const float horizSpeed = horizVel.length();
                            if (horizSpeed > PHYSICS_EPSILON)
                            {
                                const float nextSpeed = std::max(0.0f, horizSpeed - slideDecel);
                                horizVel = horizVel * (nextSpeed / horizSpeed);
                                pBody->m_Velocity.x = horizVel.x;
                                pBody->m_Velocity.z = horizVel.z;
                            }

                            const float rollDamping = std::exp(-5.0f * mu * subDt);

                            bool isTiltedBox = false;
                            CTransformComponent* pEntTransform = ent.GetTransformComponent();
                            if (pEntTransform && (!pCol || pCol->m_ColliderType == COLLIDER_BOX) &&
                                (manifold.m_Normal.y * normalYSign) > 0.85f &&
                                !(pBody->m_FreezeRotationX && pBody->m_FreezeRotationY && pBody->m_FreezeRotationZ))
                            {
                                const Vec3 scale = pEntTransform->m_Scale;
                                const Vec3 halfLocal = {
                                    std::max(std::fabs((pCol ? pCol->m_Size.x : 1.0f) * scale.x) * 0.5f, 0.001f),
                                    std::max(std::fabs((pCol ? pCol->m_Size.y : 1.0f) * scale.y) * 0.5f, 0.001f),
                                    std::max(std::fabs((pCol ? pCol->m_Size.z : 1.0f) * scale.z) * 0.5f, 0.001f)
                                };
                                auto computeBoxHalfHeightY = [&](const Mat4& rMat) -> float
                                {
                                    return std::fabs(rMat.m[1]) * halfLocal.x +
                                           std::fabs(rMat.m[5]) * halfLocal.y +
                                           std::fabs(rMat.m[9]) * halfLocal.z;
                                };

                                const Mat4 rotMat = ComposeRotationMatrix(pEntTransform->m_Rotation);
                                const float oldHalfHeightY = computeBoxHalfHeightY(rotMat);
                                const Vec3 aFaceNormals[6] = {
                                    {  rotMat.m[0],  rotMat.m[1],  rotMat.m[2] },
                                    { -rotMat.m[0], -rotMat.m[1], -rotMat.m[2] },
                                    {  rotMat.m[4],  rotMat.m[5],  rotMat.m[6] },
                                    { -rotMat.m[4], -rotMat.m[5], -rotMat.m[6] },
                                    {  rotMat.m[8],  rotMat.m[9],  rotMat.m[10] },
                                    { -rotMat.m[8], -rotMat.m[9], -rotMat.m[10] }
                                };
                                const Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
                                Vec3 bestFace = aFaceNormals[2];
                                float bestDot = -FLT_MAX;
                                for (int f = 0; f < 6; ++f)
                                {
                                    const float d = aFaceNormals[f].dot(worldUp) + static_cast<float>(f) * 1e-4f;
                                    if (d > bestDot)
                                    {
                                        bestDot = d;
                                        bestFace = aFaceNormals[f];
                                    }
                                }

                                Vec3 alignAxis = bestFace.cross(worldUp);
                                const float sinTilt = alignAxis.length();
                                const float cosTilt = std::clamp(bestFace.dot(worldUp), -1.0f, 1.0f);
                                const float tiltAngleRad = std::atan2(sinTilt, cosTilt);

                                if (tiltAngleRad > 0.015f && sinTilt > 1e-5f)
                                {
                                    isTiltedBox = true;
                                    alignAxis = alignAxis * (1.0f / sinTilt);

                                    const float alignOmega = pBody->m_AngularVelocity.dot(alignAxis);
                                    const Vec3 orthoOmega = pBody->m_AngularVelocity - alignAxis * alignOmega;
                                    const float nextAlignOmega = std::max(0.0f, alignOmega) + gravMag * 2.8f * std::max(sinTilt, 0.35f) * subDt;
                                    pBody->m_AngularVelocity = FilterFrozenRotationAxes(
                                        orthoOmega * rollDamping + alignAxis * nextAlignOmega,
                                        pBody);

                                    if (horizSpeed < 1.2f && orthoOmega.length() < 2.5f)
                                    {
                                        const float directStepRad = std::min(tiltAngleRad, (2.2f + nextAlignOmega * 0.4f) * subDt);
                                        const Vec3 directStepVec = FilterFrozenRotationAxes(alignAxis * directStepRad, pBody);
                                        if (directStepVec.lengthSquared() > 1e-12f)
                                        {
                                            IntegrateWorldAngularRotation(*pEntTransform, directStepVec, 1.0f);
                                        }

                                        const Mat4 newRotMat = ComposeRotationMatrix(pEntTransform->m_Rotation);
                                        const float newHalfHeightY = computeBoxHalfHeightY(newRotMat);
                                        const float dropY = oldHalfHeightY - newHalfHeightY;
                                        if (dropY > 0.0f && !pBody->m_FreezePositionY)
                                        {
                                            pEntTransform->m_Position.y -= dropY;
                                            pEntTransform->ClearLocalMatrixOverride();
                                        }
                                    }
                                }
                                else
                                {
                                    pBody->m_AngularVelocity = pBody->m_AngularVelocity * rollDamping;
                                    if (sinTilt > 1e-6f && pBody->m_AngularVelocity.length() < 1.5f)
                                    {
                                        alignAxis = alignAxis * (1.0f / sinTilt);
                                        const Vec3 snapVec = FilterFrozenRotationAxes(alignAxis * tiltAngleRad, pBody);
                                        if (snapVec.lengthSquared() > 1e-12f)
                                        {
                                            IntegrateWorldAngularRotation(*pEntTransform, snapVec, 1.0f);
                                            const Mat4 newRotMat = ComposeRotationMatrix(pEntTransform->m_Rotation);
                                            const float newHalfHeightY = computeBoxHalfHeightY(newRotMat);
                                            const float dropY = oldHalfHeightY - newHalfHeightY;
                                            if (dropY > 0.0f && !pBody->m_FreezePositionY)
                                            {
                                                pEntTransform->m_Position.y -= dropY;
                                                pEntTransform->ClearLocalMatrixOverride();
                                            }
                                        }
                                        pBody->m_AngularVelocity.x = 0.0f;
                                        pBody->m_AngularVelocity.z = 0.0f;
                                    }
                                }
                            }
                            else
                            {
                                pBody->m_AngularVelocity = pBody->m_AngularVelocity * rollDamping;
                            }

                            if (!isTiltedBox &&
                                (manifold.m_Normal.y * normalYSign) > 0.85f &&
                                horizSpeed <= slideDecel * 1.5f &&
                                pBody->m_AngularVelocity.length() < 0.35f * mu)
                            {
                                pBody->m_Velocity.x = 0.0f;
                                pBody->m_Velocity.z = 0.0f;
                                pBody->m_AngularVelocity = { 0.0f, 0.0f, 0.0f };
                            }

                            pBody->m_Velocity = FilterFrozenAxes(pBody->m_Velocity, pBody);
                            pBody->m_AngularVelocity = FilterFrozenRotationAxes(pBody->m_AngularVelocity, pBody);
                        };

                        if (invMassA > 0.0f)
                        {
                            applySurfaceDamping(entityA, pBodyA, pColCompA, -1.0f);
                        }
                        if (invMassB > 0.0f)
                        {
                            applySurfaceDamping(entityB, pBodyB, pColCompB, 1.0f);
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
        return RaycastInternal(scene, origin, direction, maxDistance, outHit, includeTriggers, ignoreEntityId, false);
    }

    bool CPhysicsWorld::RaycastInternal(const CScene& scene,
                                        const Vec3& origin,
                                        const Vec3& direction,
                                        float maxDistance,
                                        SPhysicsRaycastHit& outHit,
                                        bool includeTriggers,
                                        int ignoreEntityId,
                                        bool staticOnly) const
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
            if (staticOnly)
            {
                const CRigidbodyComponent* pBody = entity.GetRigidbodyComponent();
                if (pBody && pBody->m_Enabled && !pBody->m_IsKinematic && !IsNonConvexMeshEntity(entity))
                {
                    continue;
                }
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
            else if (wc.m_Type == COLLIDER_MESH && !wc.m_vTriangles.empty())
            {
                float boxT = 0.0f;
                Vec3 boxNormal{};
                if (IntersectRayBox(origin, normDir, bestDist, wc.m_BroadphaseBounds.min, wc.m_BroadphaseBounds.max, boxT, boxNormal))
                {
                    float bestTriT = bestDist;
                    Vec3 bestTriNormal{};
                    for (const STriangle& tri : wc.m_vTriangles)
                    {
                        float triT = 0.0f;
                        Vec3 triNormal{};
                        if (IntersectRayTriangle(origin, normDir, bestTriT, tri.m_V0, tri.m_V1, tri.m_V2, triT, triNormal))
                        {
                            bestTriT = triT;
                            bestTriNormal = triNormal;
                            hit = true;
                        }
                    }
                    if (hit)
                    {
                        t = bestTriT;
                        normal = bestTriNormal;
                    }
                }
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
