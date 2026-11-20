#include "editor/editor_grid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
    constexpr float PLANE_EPSILON = 1e-5f;
    constexpr float GRID_FAR_DISTANCE = 400.0f;
    constexpr int MAX_LINES_PER_AXIS = 128;
    constexpr size_t MAX_POLYGON_VERTS = 16;

    struct SPlane
    {
        Vec3 m_Normal{};
        Vec3 m_Point{};
        float m_Offset = 0.0f;
    };

    struct SGridPolygon
    {
        std::array<Vec2, MAX_POLYGON_VERTS> m_aVertices{};
        size_t m_Count = 0;

        void Push(const Vec2& vertex)
        {
            if (m_Count < MAX_POLYGON_VERTS)
            {
                m_aVertices[m_Count++] = vertex;
            }
        }
    };

    struct SGridBoundsCache
    {
        Vec3 m_CameraPosition{};
        Vec3 m_CameraTarget{};
        Vec3 m_CameraUp{};
        float m_Fovy = 0.0f;
        int m_ViewportWidth = 0;
        int m_ViewportHeight = 0;
        float m_Spacing = 0.0f;
        float m_MaxExtent = 0.0f;
        bool m_Valid = false;
        bool m_HasBounds = false;
        float m_MinX = 0.0f;
        float m_MaxX = 0.0f;
        float m_MinZ = 0.0f;
        float m_MaxZ = 0.0f;
        float m_Step = 1.0f;

        bool Matches(const Camera3D& camera, int viewportWidth, int viewportHeight,
                     float spacing, float maxExtent) const
        {
            return m_Valid &&
                m_ViewportWidth == viewportWidth &&
                m_ViewportHeight == viewportHeight &&
                m_Fovy == camera.fovy &&
                m_Spacing == spacing &&
                m_MaxExtent == maxExtent &&
                m_CameraPosition.x == camera.position.x &&
                m_CameraPosition.y == camera.position.y &&
                m_CameraPosition.z == camera.position.z &&
                m_CameraTarget.x == camera.target.x &&
                m_CameraTarget.y == camera.target.y &&
                m_CameraTarget.z == camera.target.z &&
                m_CameraUp.x == camera.up.x &&
                m_CameraUp.y == camera.up.y &&
                m_CameraUp.z == camera.up.z;
        }
    };

    float EvaluateAtGround(const SPlane& plane, float x, float z)
    {
        return plane.m_Normal.x * (x - plane.m_Point.x) +
            plane.m_Normal.z * (z - plane.m_Point.z) -
            plane.m_Normal.y * plane.m_Point.y + plane.m_Offset;
    }

    void ClipPolygon(const SPlane& plane, SGridPolygon& polygon)
    {
        if (polygon.m_Count == 0)
        {
            return;
        }

        SGridPolygon clipped;
        Vec2 last = polygon.m_aVertices[polygon.m_Count - 1];
        float previousDistance = EvaluateAtGround(plane, last.x, last.y);
        bool previousInside = previousDistance >= 0.0f;

        for (size_t i = 0; i < polygon.m_Count; ++i)
        {
            const Vec2 current = polygon.m_aVertices[i];
            const float currentDistance = EvaluateAtGround(plane, current.x, current.y);
            const bool currentInside = currentDistance >= 0.0f;

            if (currentInside != previousInside)
            {
                const float denominator = previousDistance - currentDistance;
                if (std::fabs(denominator) > PLANE_EPSILON)
                {
                    const float t = previousDistance / denominator;
                    clipped.Push({ last.x + (current.x - last.x) * t,
                                   last.y + (current.y - last.y) * t });
                }
            }

            if (currentInside)
            {
                clipped.Push(current);
            }

            last = current;
            previousDistance = currentDistance;
            previousInside = currentInside;
        }

        polygon = clipped;
    }

    SPlane MakeSidePlane(const Vec3& eye, const Vec3& first, const Vec3& second)
    {
        SPlane plane;
        plane.m_Normal = first.cross(second);
        const float length = plane.m_Normal.length();
        if (length > PLANE_EPSILON)
        {
            plane.m_Normal = plane.m_Normal * (1.0f / length);
        }
        plane.m_Point = eye;
        return plane;
    }
} // anonymous

void CInfiniteGrid::Draw(const Camera3D& camera, int viewportWidth, int viewportHeight,
                         float spacing, Color color, float maxExtent)
{
    if (spacing <= 0.0f || viewportWidth <= 0 || viewportHeight <= 0 || maxExtent <= 0.0f)
    {
        return;
    }

    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    if (aspect <= 0.0f)
    {
        return;
    }

    static SGridBoundsCache s_Cache;
    if (!s_Cache.Matches(camera, viewportWidth, viewportHeight, spacing, maxExtent))
    {
        s_Cache.m_CameraPosition = camera.position;
        s_Cache.m_CameraTarget = camera.target;
        s_Cache.m_CameraUp = camera.up;
        s_Cache.m_Fovy = camera.fovy;
        s_Cache.m_ViewportWidth = viewportWidth;
        s_Cache.m_ViewportHeight = viewportHeight;
        s_Cache.m_Spacing = spacing;
        s_Cache.m_MaxExtent = maxExtent;
        s_Cache.m_Valid = true;
        s_Cache.m_HasBounds = false;

        Vec3 forward = camera.target - camera.position;
        const float length = forward.length();
        forward = (length > PLANE_EPSILON) ? forward * (1.0f / length) : Vec3{ 0.0f, 0.0f, -1.0f };

        Vec3 up = camera.up;
        if (std::fabs(forward.dot(up)) > 0.999f)
        {
            up = (std::fabs(up.y) > 0.5f) ? Vec3{ 0.0f, 0.0f, 1.0f } : Vec3{ 0.0f, 1.0f, 0.0f };
        }

        const Vec3 right = forward.cross(up).normalized();
        const Vec3 cameraUp = right.cross(forward);

        const float tanHalfFovy = std::tan(camera.fovy * DEG2RAD * 0.5f);
        const float tanHalfFovx = tanHalfFovy * aspect;

        const auto rayDirection = [&](float ndcX, float ndcY)
        {
            return right * (ndcX * tanHalfFovx) + cameraUp * (ndcY * tanHalfFovy) + forward;
        };

        const Vec3 rayBottomLeft = rayDirection(-1.0f, -1.0f);
        const Vec3 rayBottomRight = rayDirection(1.0f, -1.0f);
        const Vec3 rayTopRight = rayDirection(1.0f, 1.0f);
        const Vec3 rayTopLeft = rayDirection(-1.0f, 1.0f);

        std::array<SPlane, 5> aPlanes = {
            MakeSidePlane(camera.position, rayBottomLeft, rayTopLeft),
            MakeSidePlane(camera.position, rayBottomRight, rayTopRight),
            MakeSidePlane(camera.position, rayBottomLeft, rayBottomRight),
            MakeSidePlane(camera.position, rayTopLeft, rayTopRight),
            SPlane{ forward * -1.0f, camera.position, GRID_FAR_DISTANCE }
        };

        const Vec3 viewCentre = camera.position + forward;
        for (SPlane& plane : aPlanes)
        {
            const float side = plane.m_Normal.dot(viewCentre - plane.m_Point) + plane.m_Offset;
            if (side < 0.0f)
            {
                plane.m_Normal = plane.m_Normal * -1.0f;
            }
        }

        const float centerX = std::floor(camera.position.x / spacing) * spacing;
        const float centerZ = std::floor(camera.position.z / spacing) * spacing;

        SGridPolygon polygon;
        polygon.Push({ centerX - maxExtent, centerZ - maxExtent });
        polygon.Push({ centerX + maxExtent, centerZ - maxExtent });
        polygon.Push({ centerX + maxExtent, centerZ + maxExtent });
        polygon.Push({ centerX - maxExtent, centerZ + maxExtent });

        for (const SPlane& plane : aPlanes)
        {
            ClipPolygon(plane, polygon);
        }

        if (polygon.m_Count >= 3)
        {
            float minX = polygon.m_aVertices[0].x;
            float maxX = minX;
            float minZ = polygon.m_aVertices[0].y;
            float maxZ = minZ;
            for (size_t i = 1; i < polygon.m_Count; ++i)
            {
                const Vec2& vertex = polygon.m_aVertices[i];
                minX = std::fmin(minX, vertex.x);
                maxX = std::fmax(maxX, vertex.x);
                minZ = std::fmin(minZ, vertex.y);
                maxZ = std::fmax(maxZ, vertex.y);
            }

            if (minX <= maxX && minZ <= maxZ)
            {
                float step = spacing;
                for (int i = 0; i < 24; ++i)
                {
                    if ((maxX - minX) / step + (maxZ - minZ) / step <= static_cast<float>(MAX_LINES_PER_AXIS * 2))
                    {
                        break;
                    }
                    step *= 2.0f;
                }
                s_Cache.m_MinX = minX;
                s_Cache.m_MaxX = maxX;
                s_Cache.m_MinZ = minZ;
                s_Cache.m_MaxZ = maxZ;
                s_Cache.m_Step = step;
                s_Cache.m_HasBounds = true;
            }
        }
    }

    if (!s_Cache.m_HasBounds)
    {
        return;
    }

    const float minX = s_Cache.m_MinX;
    const float maxX = s_Cache.m_MaxX;
    const float minZ = s_Cache.m_MinZ;
    const float maxZ = s_Cache.m_MaxZ;
    const float step = s_Cache.m_Step;

    const float firstX = std::ceil(minX / step) * step;
    const float firstZ = std::ceil(minZ / step) * step;

    for (float x = firstX; x <= maxX; x += step)
    {
        DrawLine3D({ x, 0.0f, minZ }, { x, 0.0f, maxZ }, color);
    }
    for (float z = firstZ; z <= maxZ; z += step)
    {
        DrawLine3D({ minX, 0.0f, z }, { maxX, 0.0f, z }, color);
    }

    const Color axisColor(
        static_cast<unsigned char>(std::min(255, static_cast<int>(color.r) + 55)),
        static_cast<unsigned char>(std::min(255, static_cast<int>(color.g) + 55)),
        static_cast<unsigned char>(std::min(255, static_cast<int>(color.b) + 55)),
        color.a);

    if (0.0f >= minX && 0.0f <= maxX)
    {
        DrawLine3D({ 0.0f, 0.0f, minZ }, { 0.0f, 0.0f, maxZ }, axisColor);
    }
    if (0.0f >= minZ && 0.0f <= maxZ)
    {
        DrawLine3D({ minX, 0.0f, 0.0f }, { maxX, 0.0f, 0.0f }, axisColor);
    }
}
