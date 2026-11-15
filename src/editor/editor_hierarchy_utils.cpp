#include "editor/editor_hierarchy_utils.h"
#include "application_plugin_bridge.h"
#include "engine/transform.h"
#include "imgui.h"
#include <algorithm>
#include <functional>

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
    std::vector<bool> vVisited(scene.m_vEntities.size(), false);
    if (entityId >= 0 && entityId < static_cast<int>(vVisited.size()))
    {
        vVisited[entityId] = true;
    }

    while (!vToProcess.empty())
    {
        int current = vToProcess.back();
        vToProcess.pop_back();
        if (current < 0 || current >= static_cast<int>(vVisited.size()) || vVisited[current])
        {
            continue;
        }
        vVisited[current] = true;

        vDescendants.push_back(current);

        auto vChildren = GetEntityChildren(scene, current);
        for (int child : vChildren)
        {
            vToProcess.push_back(child);
        }
    }

    return vDescendants;
}

void EraseEntityAndDescendants(
    CScene& scene, int entityIndex,
    const std::function<void(CEntity&, int)>& beforeErase)
{
    if (entityIndex < 0 || entityIndex >= static_cast<int>(scene.m_vEntities.size()))
    {
        return;
    }

    std::vector<int> vIndices = GetEntityDescendants(scene, entityIndex);
    vIndices.push_back(entityIndex);
    std::sort(vIndices.begin(), vIndices.end(), std::greater<int>());

    for (int index : vIndices)
    {
        if (index < 0 || index >= static_cast<int>(scene.m_vEntities.size()))
        {
            continue;
        }
        CEntity& entity = scene.m_vEntities[index];
        if (beforeErase)
        {
            beforeErase(entity, index);
        }
        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_DELETED, index);
        scene.m_vEntities.erase(scene.m_vEntities.begin() + index);
        for (int current = 0; current < static_cast<int>(scene.m_vEntities.size()); ++current)
        {
            CEntity& remaining = scene.m_vEntities[current];
            remaining.m_Id = current;
            if (remaining.m_ParentId == index)
            {
                remaining.m_ParentId = -1;
            }
            else if (remaining.m_ParentId > index)
            {
                --remaining.m_ParentId;
            }
        }
    }

    scene.m_Selected = -1;
    scene.m_vSelectedEntities.clear();
}

void MoveEntityToParent(CScene& scene, int entityId, int newParentId)
{
    if (entityId < 0 || entityId >= static_cast<int>(scene.m_vEntities.size()))
    {
        return;
    }
    if (newParentId < -1 || newParentId >= static_cast<int>(scene.m_vEntities.size()))
    {
        return;
    }
    if (newParentId == entityId)
    {
        return;
    }
    if (scene.m_vEntities[entityId].m_ParentId == newParentId)
    {
        return;
    }

    if (newParentId >= 0)
    {
        auto vDescendants = GetEntityDescendants(scene, entityId);
        for (int desc : vDescendants)
        {
            if (desc == newParentId)
            {
                return;
            }
        }
    }

    const Mat4 worldTransform = quark::ComposeWorld(scene, entityId);
    const Mat4 parentTransform = newParentId >= 0
        ? quark::ComposeWorld(scene, newParentId)
        : Mat4::identity();
    Mat4 inverseParent;
    if (!quark::TryInvertAffine(parentTransform, inverseParent))
    {
        return;
    }

    scene.m_vEntities[entityId].m_ParentId = newParentId;
    if (CTransformComponent* pTransform = scene.m_vEntities[entityId].GetTransformComponent())
    {
        const Mat4 localTransform = inverseParent * worldTransform;
        quark::DecomposeWorld(localTransform, *pTransform);
        pTransform->SetLocalMatrixOverride(localTransform);
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
        auto vChildren = GetEntityChildren(scene, groupId);
        for (int child : vChildren)
        {
            MoveEntityToParent(scene, child, parentOfGroup);
        }
    }

    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_DELETED, groupId);
    scene.m_vEntities.erase(scene.m_vEntities.begin() + groupId);

    for (int i = 0; i < static_cast<int>(scene.m_vEntities.size()); i++)
    {
        CEntity& entity = scene.m_vEntities[i];
        entity.m_Id = i;
        if (entity.m_ParentId > groupId)
        {
            --entity.m_ParentId;
        }
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
