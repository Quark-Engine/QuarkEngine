#include "editor/editor_hierarchy_utils.h"
#include "application_plugin_bridge.h"
using namespace qc;
#include "imgui.h"
#include <algorithm>
#include <cmath>

static Mat4 ComposeLocalTransform(const CEntity& entity) 
{
    const CTransformComponent* pTransform = entity.GetTransformComponent();
    if (!pTransform) 
    {
        return Mat4::identity();
    }

    return Mat4::translation(pTransform->m_Position.x, pTransform->m_Position.y, pTransform->m_Position.z) *
        Mat4::rotationX(pTransform->m_Rotation.x * DEG2RAD) *
        Mat4::rotationY(pTransform->m_Rotation.y * DEG2RAD) *
        Mat4::rotationZ(pTransform->m_Rotation.z * DEG2RAD) *
        Mat4::scale(pTransform->m_Scale.x, pTransform->m_Scale.y, pTransform->m_Scale.z);
}

static Mat4 ComposeWorldTransform(const CScene& scene, int entityIndex, std::vector<int>& vStack)
{
    if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
    {
        return Mat4::identity();
    }

    if (std::find(vStack.begin(), vStack.end(), entityIndex) != vStack.end())
    {
        return ComposeLocalTransform(scene.m_vEntities[entityIndex]);
    }

    vStack.push_back(entityIndex);
    const CEntity& entity = scene.m_vEntities[entityIndex];
    Mat4 world = ComposeLocalTransform(entity);
    if (entity.m_ParentId >= 0 && entity.m_ParentId < static_cast<int>(scene.m_vEntities.size()))
    {
        world = ComposeWorldTransform(scene, entity.m_ParentId, vStack) * world;
    }
    vStack.pop_back();
    return world;
}

static Mat4 ComposeWorldTransform(const CScene& scene, int entityIndex)
{
    std::vector<int> vStack;
    return ComposeWorldTransform(scene, entityIndex, vStack);
}

static Vec3 NormalizeOrForward(const Vec3& v)
{
    const float length = v.length();
    if (length < 1e-9f)
    {
        return Vec3{0.0f, 1.0f, 0.0f};
    }
    return v * (1.0f / length);
}

static Mat4 PolarRotation(const Mat4& matrix)
{
    Mat4 q = matrix;
    for (int iteration = 0; iteration < 24; ++iteration)
    {
        const Mat4 inverted = q.inverted();
        const Mat4 invertedTranspose = Mat4Transpose(inverted);
        for (int i = 0; i < 16; ++i) q.m[i] = 0.5f * (q.m[i] + invertedTranspose.m[i]);
    }
    for (int column = 0; column < 3; ++column)
    {
        const Vec3 axis = NormalizeOrForward(Mat4Column(q, column));
        q.m[column * 4] = axis.x;
        q.m[column * 4 + 1] = axis.y;
        q.m[column * 4 + 2] = axis.z;
    }
    return q;
}

static void DecomposeTransform(const Mat4& parentTransform, const Mat4& worldTransform, CTransformComponent& transform)
{
    const Mat4 local = parentTransform.inverted() * worldTransform;
    transform.m_Position =
    {
        local.m[12],
        local.m[13],
        local.m[14]
    };

    Mat4 parent3x3{};
    Mat4 world3x3{};
    for (int column = 0; column < 3; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            parent3x3.m[column * 4 + row] = parentTransform.m[column * 4 + row];
            world3x3.m[column * 4 + row] = worldTransform.m[column * 4 + row];
        }
    }

    Vec3 scale =
    {
        Mat4Column(local, 0).length(),
        Mat4Column(local, 1).length(),
        Mat4Column(local, 2).length()
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
        Mat4 inverseScale{};
        inverseScale.m[0] = (scale.x > 1e-6f) ? 1.0f / scale.x : 1.0f;
        inverseScale.m[5] = (scale.y > 1e-6f) ? 1.0f / scale.y : 1.0f;
        inverseScale.m[10] = (scale.z > 1e-6f) ? 1.0f / scale.z : 1.0f;

        Mat4 rotation = PolarRotation(Mat4Transpose(parent3x3) * world3x3 * inverseScale);

        const Mat4 scaledRotation = parent3x3 * rotation;
        for (int column = 0; column < 3; ++column)
        {
            const Vec3 a = Mat4Column(scaledRotation, column);
            const Vec3 b = Mat4Column(world3x3, column);

            const float denominator = Vec3SquaredLength(a);
            float next = (denominator > 1e-6f) ? Vec3Dot(a, b) / denominator : 0.0f;

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
            const Vec3 right = Mat4Column(rotation, 0);
            const Vec3 up = Mat4Column(rotation, 1);
            const Vec3 dir = Mat4Column(rotation, 2);

            transform.m_Rotation =
            {
                atan2f(-dir.y, dir.z) * RAD2DEG,
                asinf(dir.x) * RAD2DEG,
                atan2f(-up.x, right.x) * RAD2DEG
            };
        }
    }
    transform.m_Scale = scale;

    constexpr float kEpsilon = 0.0001f;
    auto cleanup = [](float& value)
    {
        if (fabsf(value) < kEpsilon)
        {
            value = 0.0f;
        }
        if (fabsf(value - 1.0f) < kEpsilon)
        {
            value = 1.0f;
        }
    };
    cleanup(transform.m_Position.x); cleanup(transform.m_Position.y); cleanup(transform.m_Position.z);
    cleanup(transform.m_Rotation.x); cleanup(transform.m_Rotation.y); cleanup(transform.m_Rotation.z);
    cleanup(transform.m_Scale.x); cleanup(transform.m_Scale.y); cleanup(transform.m_Scale.z);
}

std::vector<int> GetEntityChildren(const CScene& scene, int parentId)
{
    std::vector<int> vChildren;
    for (int i = 0; i < static_cast<int>(scene.m_vEntities.size()); i++)
    {
        if (scene.m_vEntities[i].m_ParentId == parentId)
        {
            vChildren.push_back(i);
        }
    }
    return vChildren;
}

std::vector<int> GetEntityDescendants(const CScene& scene, int entityId)
{
    std::vector<int> vDescendants;
    std::vector<int> vToProcess = GetEntityChildren(scene, entityId);
    
    while (!vToProcess.empty())
    {
        int current = vToProcess.back();
        vToProcess.pop_back();
        
        vDescendants.push_back(current);
        
        auto children = GetEntityChildren(scene, current);
        for (int child : children)
        {
            vToProcess.push_back(child);
        }
    }
    
    return vDescendants;
}

void MoveEntityToParent(CScene& scene, int entityId, int newParentId)
{
    if (entityId < 0 || entityId >= static_cast<int>(scene.m_vEntities.size()))
    {
        return;
    }
    if (newParentId == entityId)
    {
        return;
    }
    
    if (newParentId >= 0)
    {
        auto descendants = GetEntityDescendants(scene, entityId);
        for (int desc : descendants)
        {
            if (desc == newParentId) return;
        }
    }
    
    const Mat4 worldTransform = ComposeWorldTransform(scene, entityId);
    const Mat4 parentTransform = newParentId >= 0
        ? ComposeWorldTransform(scene, newParentId)
        : Mat4::identity();

    scene.m_vEntities[entityId].m_ParentId = newParentId;
    if (CTransformComponent* pTransform = scene.m_vEntities[entityId].GetTransformComponent())
    {
        DecomposeTransform(parentTransform, worldTransform, *pTransform);
    }
}

int CreateGroup(CScene& scene, const std::string& name, int parentId)
{
    CEntity group;
    group.m_Id = static_cast<int>(scene.m_vEntities.size());
    group.m_Name = name;
    group.m_ParentId = parentId;
    group.m_IsGroup = true;
    
    scene.m_vEntities.push_back(group);
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, group.m_Id);
    return group.m_Id;
}

void DeleteGroup(CScene& scene, int groupId, bool reparentToParent)
{
    if (groupId < 0 || groupId >= static_cast<int>(scene.m_vEntities.size()))
    {
        return;
    }
    
    CEntity& group = scene.m_vEntities[groupId];
    if (!group.m_IsGroup)
    {
        return;
    }

    int parentOfGroup = group.m_ParentId;
    
    if (reparentToParent)
    {
        auto children = GetEntityChildren(scene, groupId);
        for (int child : children)
        {
            scene.m_vEntities[child].m_ParentId = parentOfGroup;
        }
    }
    
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_DELETED, groupId);
    scene.m_vEntities.erase(scene.m_vEntities.begin() + groupId);
    
    for (int i = groupId; i < static_cast<int>(scene.m_vEntities.size()); i++)
    {
        scene.m_vEntities[i].m_Id = i;
    }
}

bool IsEntityGroup(const CEntity& entity)
{
    return entity.m_IsGroup;
}

std::vector<int> GetRootEntities(const CScene& scene) 
{
    return GetEntityChildren(scene, -1);
}
