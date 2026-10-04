#include "engine/transform.h"

#include <algorithm>
#include <cmath>

namespace quark
{

qc::Mat4 ComposeLocal(const CTransformComponent& transform)
{
    if (transform.m_HasLocalMatrixOverride)
    {
        return transform.m_LocalMatrixOverride;
    }

    return qc::Mat4::translation(transform.m_Position.x, transform.m_Position.y, transform.m_Position.z) *
        qc::Mat4::rotationX(transform.m_Rotation.x * DEG2RAD) *
        qc::Mat4::rotationY(transform.m_Rotation.y * DEG2RAD) *
        qc::Mat4::rotationZ(transform.m_Rotation.z * DEG2RAD) *
        qc::Mat4::scale(transform.m_Scale.x, transform.m_Scale.y, transform.m_Scale.z);
}

qc::Mat4 ComposeLocal(const CEntity& entity)
{
    const CTransformComponent* pTransform = entity.GetTransformComponent();
    if (!pTransform)
    {
        return qc::Mat4::identity();
    }
    return ComposeLocal(*pTransform);
}

int IndexOfEntity(const CScene& scene, const CEntity& entity)
{
    const std::vector<CEntity>& vEntities = scene.m_vEntities;
    for (int i = 0; i < static_cast<int>(vEntities.size()); ++i)
    {
        if (&vEntities[i] == &entity)
        {
            return i;
        }
    }
    return -1;
}

qc::Mat4 ComposeWorld(const CScene& scene, int entityIndex, std::vector<int>& vStack)
{
    if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
    {
        return qc::Mat4::identity();
    }

    if (std::find(vStack.begin(), vStack.end(), entityIndex) != vStack.end())
    {
        return ComposeLocal(scene.m_vEntities[entityIndex]);
    }

    vStack.push_back(entityIndex);
    const CEntity& entity = scene.m_vEntities[entityIndex];
    qc::Mat4 world = ComposeLocal(entity);
    if (entity.m_ParentId != entityIndex &&
        entity.m_ParentId >= 0 && entity.m_ParentId < static_cast<int>(scene.m_vEntities.size()))
    {
        world = ComposeWorld(scene, entity.m_ParentId, vStack) * world;
    }
    vStack.pop_back();
    return world;
}

qc::Mat4 ComposeWorld(const CScene& scene, int entityIndex)
{
    std::vector<int> vStack;
    return ComposeWorld(scene, entityIndex, vStack);
}

qc::Mat4 ComposeWorld(const CScene& scene, const CEntity& entity)
{
    const int index = IndexOfEntity(scene, entity);
    if (index < 0)
    {
        return ComposeLocal(entity);
    }
    return ComposeWorld(scene, index);
}

qc::Mat4 ComposeMeshWorld(const CScene& scene, const CEntity& entity)
{
    const CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return ComposeWorld(scene, entity);
    }
    return ComposeWorld(scene, entity) * pMesh->m_Model.transform;
}

qc::Mat4 ParentWorld(const CScene& scene, const CEntity& entity)
{
    if (IndexOfEntity(scene, entity) < 0)
    {
        return qc::Mat4::identity();
    }
    if (entity.m_ParentId < 0 || entity.m_ParentId >= static_cast<int>(scene.m_vEntities.size()))
    {
        return qc::Mat4::identity();
    }
    if (&scene.m_vEntities[entity.m_ParentId] == &entity)
    {
        return qc::Mat4::identity();
    }
    return ComposeWorld(scene, entity.m_ParentId);
}

bool TryInvertAffine(const qc::Mat4& matrix, qc::Mat4& inverse)
{
    const float a00 = matrix.m[0];
    const float a01 = matrix.m[4];
    const float a02 = matrix.m[8];
    const float a10 = matrix.m[1];
    const float a11 = matrix.m[5];
    const float a12 = matrix.m[9];
    const float a20 = matrix.m[2];
    const float a21 = matrix.m[6];
    const float a22 = matrix.m[10];

    const float determinant =
        a00 * (a11 * a22 - a12 * a21) -
        a01 * (a10 * a22 - a12 * a20) +
        a02 * (a10 * a21 - a11 * a20);
    if (!std::isfinite(determinant) || std::fabs(determinant) <= 1e-8f)
    {
        return false;
    }

    const float inverseDeterminant = 1.0f / determinant;
    inverse = qc::Mat4::identity();
    inverse.m[0] = (a11 * a22 - a12 * a21) * inverseDeterminant;
    inverse.m[4] = (a02 * a21 - a01 * a22) * inverseDeterminant;
    inverse.m[8] = (a01 * a12 - a02 * a11) * inverseDeterminant;
    inverse.m[1] = (a12 * a20 - a10 * a22) * inverseDeterminant;
    inverse.m[5] = (a00 * a22 - a02 * a20) * inverseDeterminant;
    inverse.m[9] = (a02 * a10 - a00 * a12) * inverseDeterminant;
    inverse.m[2] = (a10 * a21 - a11 * a20) * inverseDeterminant;
    inverse.m[6] = (a01 * a20 - a00 * a21) * inverseDeterminant;
    inverse.m[10] = (a00 * a11 - a01 * a10) * inverseDeterminant;

    const float tx = matrix.m[12];
    const float ty = matrix.m[13];
    const float tz = matrix.m[14];
    inverse.m[12] = -(inverse.m[0] * tx + inverse.m[4] * ty + inverse.m[8] * tz);
    inverse.m[13] = -(inverse.m[1] * tx + inverse.m[5] * ty + inverse.m[9] * tz);
    inverse.m[14] = -(inverse.m[2] * tx + inverse.m[6] * ty + inverse.m[10] * tz);
    return true;
}

void DecomposeLocal(const qc::Mat4& parentWorld, const qc::Mat4& world, CTransformComponent& out)
{
    qc::Mat4 inverseParent;
    const qc::Mat4 local = TryInvertAffine(parentWorld, inverseParent)
        ? inverseParent * world
        : world;
    out.m_Position = qc::Vec3(local.m[12], local.m[13], local.m[14]);

    qc::Mat4 parent3x3{};
    qc::Mat4 world3x3{};
    for (int column = 0; column < 3; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            parent3x3.m[column * 4 + row] = parentWorld.m[column * 4 + row];
            world3x3.m[column * 4 + row] = world.m[column * 4 + row];
        }
    }

    qc::Vec3 scale = {
        qc::Mat4Column(local, 0).length(),
        qc::Mat4Column(local, 1).length(),
        qc::Mat4Column(local, 2).length()
    };
    if (scale.x < 1e-6f)
    {
        scale.x = 1.0f;
    }
    if (scale.y < 1e-6f)
    {
        scale.y = 1.0f;
    }
    if (scale.z < 1e-6f)
    {
        scale.z = 1.0f;
    }

    for (int iteration = 0; iteration < 16; ++iteration)
    {
        qc::Mat4 inverseScale{};
        inverseScale.m[0] = (scale.x > 1e-6f) ? 1.0f / scale.x : 1.0f;
        inverseScale.m[5] = (scale.y > 1e-6f) ? 1.0f / scale.y : 1.0f;
        inverseScale.m[10] = (scale.z > 1e-6f) ? 1.0f / scale.z : 1.0f;

        qc::Mat4 rotation = qc::Mat4PolarRotation(qc::Mat4Transpose(parent3x3) * world3x3 * inverseScale);

        const qc::Mat4 scaledRotation = parent3x3 * rotation;
        for (int column = 0; column < 3; ++column)
        {
            const qc::Vec3 a = qc::Mat4Column(scaledRotation, column);
            const qc::Vec3 b = qc::Mat4Column(world3x3, column);
            const float denominator = qc::Vec3SquaredLength(a);
            float next = (denominator > 1e-6f) ? qc::Vec3Dot(a, b) / denominator : 0.0f;
            if (next < 0.0f)
            {
                next = 0.0f;
            }
            if (column == 0)
            {
                scale.x = next;
            }
            else if (column == 1)
            {
                scale.y = next;
            }
            else
            {
                scale.z = next;
            }
        }

        if (iteration == 15)
        {
            const qc::Vec3 right = qc::Mat4Column(rotation, 0);
            const qc::Vec3 up = qc::Mat4Column(rotation, 1);
            const qc::Vec3 dir = qc::Mat4Column(rotation, 2);
            out.m_Rotation = qc::Vec3(
                std::atan2(-dir.y, dir.z) * RAD2DEG,
                std::asin(dir.x) * RAD2DEG,
                std::atan2(-up.x, right.x) * RAD2DEG
            );
        }
    }
    out.m_Scale = scale;

    constexpr float K_EPSILON = 0.0001f;
    auto cleanup = [](float& value)
    {
        if (std::fabs(value) < K_EPSILON)
        {
            value = 0.0f;
        }
        if (std::fabs(value - 1.0f) < K_EPSILON)
        {
            value = 1.0f;
        }
    };
    cleanup(out.m_Position.x); cleanup(out.m_Position.y); cleanup(out.m_Position.z);
    cleanup(out.m_Rotation.x); cleanup(out.m_Rotation.y); cleanup(out.m_Rotation.z);
    cleanup(out.m_Scale.x); cleanup(out.m_Scale.y); cleanup(out.m_Scale.z);
}

void DecomposeWorld(const qc::Mat4& world, CTransformComponent& out)
{
    DecomposeLocal(qc::Mat4::identity(), world, out);
}

} // quark
