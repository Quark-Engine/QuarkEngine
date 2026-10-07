#include "editor/editor_scene_picker.h"

#include "editor/editor_utils.h"
#include "engine/transform.h"
#include "models.h"
Vec3 CScenePicker::RayPlaneHit(Ray ray)
{
    if (fabsf(ray.direction.y) < 0.0001f)
    {
        return ray.position;
    }

    const float t = -ray.position.y / ray.direction.y;

    return {
        ray.position.x + ray.direction.x * t,
        0.0f,
        ray.position.z + ray.direction.z * t
    };
}

Vec2 CScenePicker::WorldToScreen(const CViewportState& viewport, const Vec3& world,
    const Camera3D& camera)
{
    const Mat4 view = Mat4::lookAt(camera.position, camera.target, camera.up);
    const Mat4 projection = Mat4::perspective(
        camera.fovy * DEG2RAD,
        viewport.m_WindowSize.x / viewport.m_WindowSize.y,
        0.1f,
        1000.0f
    );
    const Vec4 clip = projection * (view * Vec4{world.x, world.y, world.z, 1.0f});
    if (fabsf(clip.w) <= 0.000001f)
    {
        return {viewport.m_WindowPos.x, viewport.m_WindowPos.y};
    }

    const float nx = clip.x / clip.w * 0.5f + 0.5f;
    const float ny = -clip.y / clip.w * 0.5f + 0.5f;
    return {
        viewport.m_WindowPos.x + nx * viewport.m_WindowSize.x,
        viewport.m_WindowPos.y + ny * viewport.m_WindowSize.y
    };
}

Ray CScenePicker::ScreenToWorldRay(const CViewportState& viewport, const Vec2& mouse,
    const Camera3D& camera)
{
    const float nx = (mouse.x - viewport.m_WindowPos.x) / viewport.m_WindowSize.x;
    const float ny = (mouse.y - viewport.m_WindowPos.y) / viewport.m_WindowSize.y;

    const Mat4 view = Mat4::lookAt(
        camera.position,
        camera.target,
        camera.up
    );

    const Mat4 proj = Mat4::perspective(
        camera.fovy * DEG2RAD,
        viewport.m_WindowSize.x / viewport.m_WindowSize.y,
        0.1f,
        1000.0f
    );

    const Mat4 invVp = (proj * view).inverted();

    const float ndcX =  nx * 2.0f - 1.0f;
    const float ndcY = -(ny * 2.0f - 1.0f);

    auto mul = [](Mat4 m, Vec4 v) -> Vec4
    {
        return {
            m.m0*v.x + m.m4*v.y + m.m8*v.z  + m.m12*v.w,
            m.m1*v.x + m.m5*v.y + m.m9*v.z  + m.m13*v.w,
            m.m2*v.x + m.m6*v.y + m.m10*v.z + m.m14*v.w,
            m.m3*v.x + m.m7*v.y + m.m11*v.z + m.m15*v.w
        };
    };

    const Vec4 nearW = mul(invVp, {ndcX, ndcY, -1.0f, 1.0f});
    const Vec4 farW  = mul(invVp, {ndcX, ndcY,  1.0f, 1.0f});

    const Vec3 nearPos = { nearW.x/nearW.w, nearW.y/nearW.w, nearW.z/nearW.w };
    const Vec3 farPos  = { farW.x/farW.w,   farW.y/farW.w,   farW.z/farW.w  };

    Ray ray;
    ray.position  = nearPos;
    ray.direction = Vec3Normalize(Vec3Subtract(farPos, nearPos));
    return ray;
}

bool CScenePicker::PickMeshTriangle(
    const CScene& scene,
    const CEntity& entity,
    int meshIndex,
    Ray ray,
    int& outTriangleIndex,
    int& outVertexCorner
)
{
    const CMeshComponent* pMeshComponent = entity.GetMeshComponent();
    if (!pMeshComponent || !HasValidModelData(pMeshComponent->m_Model))
    {
        return false;
    }
    if (meshIndex < 0 || meshIndex >= pMeshComponent->m_Model.meshCount)
    {
        return false;
    }

    const Mesh& mesh = pMeshComponent->m_Model.meshes[meshIndex];
    if (!mesh.vertices || mesh.triangleCount <= 0)
    {
        return false;
    }

    const Mat4 transform = quark::ComposeMeshWorld(scene, entity);
    float bestDistance = FLT_MAX;
    int bestTriangle = -1;
    int bestCorner = 0;

    for (int triangleIndex = 0; triangleIndex < mesh.triangleCount; triangleIndex++)
    {
        int aIndices[3] = {};
        if (!CMeshOverrideService::GetTriangleVertexIndices(mesh, triangleIndex, aIndices))
        {
            continue;
        }

        Vec3 aVertices[3] = {};
        for (int i = 0; i < 3; i++)
        {
            aVertices[i] = transform * Vec3(
                mesh.vertices[aIndices[i] * 3 + 0],
                mesh.vertices[aIndices[i] * 3 + 1],
                mesh.vertices[aIndices[i] * 3 + 2]
            );
        }

        const RayCollision hit = GetRayCollisionTriangle(ray, aVertices[0], aVertices[1], aVertices[2]);
        if (!hit.hit || hit.distance >= bestDistance)
        {
            continue;
        }

        bestDistance = hit.distance;
        bestTriangle = triangleIndex;

        float closestCornerDistance = FLT_MAX;
        for (int i = 0; i < 3; i++)
        {
            const float distanceToCorner = (hit.point - aVertices[i]).length();
            if (distanceToCorner < closestCornerDistance)
            {
                closestCornerDistance = distanceToCorner;
                bestCorner = i;
            }
        }
    }

    if (bestTriangle < 0)
    {
        return false;
    }

    outTriangleIndex = bestTriangle;
    outVertexCorner = bestCorner;
    return true;
}

bool CScenePicker::RaycastEntity(const CScene& scene, const CEntity& entity, Ray ray,
    float& outDistance)
{
    const CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh || !HasValidModelData(pMesh->m_Model))
    {
        return false;
    }

    const Mat4 transform = quark::ComposeMeshWorld(scene, entity);

    bool hitAny = false;
    float bestDistance = FLT_MAX;

    for (int i = 0; i < pMesh->m_Model.meshCount; i++)
    {
        const Mesh& m = pMesh->m_Model.meshes[i];

        for (int j = 0; j < m.triangleCount; j++)
        {
            int aIndices[3] = {};
            if (!CMeshOverrideService::GetTriangleVertexIndices(m, j, aIndices))
            {
                continue;
            }

            Vec3 aVerts[3];

            for (int k = 0; k < 3; k++)
            {
                aVerts[k] = Vec3Transform(Vec3{
                    m.vertices[aIndices[k] * 3 + 0],
                    m.vertices[aIndices[k] * 3 + 1],
                    m.vertices[aIndices[k] * 3 + 2],
                }, transform);
            }

            const RayCollision hit = GetRayCollisionTriangle(ray, aVerts[0], aVerts[1], aVerts[2]);

            if (hit.hit && hit.distance < bestDistance)
            {
                bestDistance = hit.distance;
                hitAny = true;
            }
        }
    }

    outDistance = bestDistance;
    return hitAny;
}
