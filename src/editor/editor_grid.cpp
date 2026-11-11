#include "editor/editor_grid.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    constexpr float PLANE_EPSILON = 1e-5f;
    constexpr float GRID_FAR_DISTANCE = 400.0f;
    constexpr int MAX_LINES_PER_AXIS = 256;

    struct SPlane
    {
        Vec3 m_Normal{};
        Vec3 m_Point{};
        float m_Offset = 0.0f;
    };

    float EvaluateAtGround(const SPlane& plane, float x, float z)
    {
        return plane.m_Normal.x * (x - plane.m_Point.x) + plane.m_Normal.z * (z - plane.m_Point.z) - plane.m_Normal.y * plane.m_Point.y + plane.m_Offset;
    }

    void ClipPolygon(const SPlane& plane, std::vector<Vec2>& vPolygon)
    {
        if (vPolygon.empty())
        {
            return;
        }

        std::vector<Vec2> vClipped;
        vClipped.reserve(vPolygon.size() + 4);

        Vec2 last = vPolygon.back();
        float previousDistance = EvaluateAtGround(plane, last.x, last.y);
        bool previousInside = previousDistance >= 0.0f;

        for (const Vec2& current : vPolygon)
        {
            const float currentDistance = EvaluateAtGround(plane, current.x, current.y);
            const bool currentInside = currentDistance >= 0.0f;

            if (currentInside != previousInside)
            {
                const float denominator = previousDistance - currentDistance;
                if (std::fabs(denominator) > PLANE_EPSILON)
                {
                    const float t = previousDistance / denominator;
                    vClipped.push_back({ last.x + (current.x - last.x) * t,
                                         last.y + (current.y - last.y) * t });
                }
            }

            if (currentInside)
            {
                vClipped.push_back(current);
            }

            last = current;
            previousDistance = currentDistance;
            previousInside = currentInside;
        }

        vPolygon.swap(vClipped);
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

    Vec3 forward = camera.target - camera.position;
    float length = forward.length();
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

    std::vector<SPlane> vPlanes = {
        MakeSidePlane(camera.position, rayBottomLeft, rayTopLeft),
        MakeSidePlane(camera.position, rayBottomRight, rayTopRight),
        MakeSidePlane(camera.position, rayBottomLeft, rayBottomRight),
        MakeSidePlane(camera.position, rayTopLeft, rayTopRight),
        { forward * -1.0f, camera.position, GRID_FAR_DISTANCE }
    };

    const Vec3 viewCentre = camera.position + forward;
    for (SPlane& plane : vPlanes)
    {
        const float side = plane.m_Normal.dot(viewCentre - plane.m_Point) + plane.m_Offset;
        if (side < 0.0f)
        {
            plane.m_Normal = plane.m_Normal * -1.0f;
        }
    }

    const float centerX = std::floor(camera.position.x / spacing) * spacing;
    const float centerZ = std::floor(camera.position.z / spacing) * spacing;

    std::vector<Vec2> vPolygon = {
        { centerX - maxExtent, centerZ - maxExtent },
        { centerX + maxExtent, centerZ - maxExtent },
        { centerX + maxExtent, centerZ + maxExtent },
        { centerX - maxExtent, centerZ + maxExtent }
    };

    for (const SPlane& plane : vPlanes)
    {
        ClipPolygon(plane, vPolygon);
    }

    if (vPolygon.size() < 3)
    {
        return;
    }

    float minX = vPolygon.front().x;
    float maxX = minX;
    float minZ = vPolygon.front().y;
    float maxZ = minZ;
    for (const Vec2& vertex : vPolygon)
    {
        minX = std::fmin(minX, vertex.x);
        maxX = std::fmax(maxX, vertex.x);
        minZ = std::fmin(minZ, vertex.y);
        maxZ = std::fmax(maxZ, vertex.y);
    }

    if (minX > maxX || minZ > maxZ)
    {
        return;
    }

    float step = spacing;
    for (int i = 0; i < 24; ++i)
    {
        if ((maxX - minX) / step + (maxZ - minZ) / step <= static_cast<float>(MAX_LINES_PER_AXIS * 2))
        {
            break;
        }
        step *= 2.0f;
    }

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
