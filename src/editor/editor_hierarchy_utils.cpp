#include "editor/editor_hierarchy_utils.h"
#include "application_plugin_bridge.h"
#include "engine/transform.h"
using namespace qc;
#include "imgui.h"
#include <algorithm>

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
        auto descendants = GetEntityDescendants(scene, entityId);
        for (int desc : descendants)
        {
            if (desc == newParentId) return;
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
        auto children = GetEntityChildren(scene, groupId);
        for (int child : children)
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
