#include "editor/editor_entity_commands.h"

#include "editor/editor.h"
#include "editor/editor_state.h"
#include "editor/editor_hierarchy_utils.h"
#include "application_plugin_bridge.h"
#include "models.h"
#include "tex.h"
#include "imgui.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

std::string CSceneEntityCommands::MakeDuplicateName(const CScene& scene, const std::string& name)
{
    std::string base = name;
    int start = 1;
    const size_t open = name.find(" (");
    if (open != std::string::npos && open + 2 < name.size() && name.back() == ')')
    {
        int value = 0;
        bool numeric = true;
        for (size_t i = open + 2; i + 1 < name.size(); ++i)
        {
            const char c = name[i];
            if (c >= '0' && c <= '9')
            {
                value = value * 10 + (c - '0');
            }
            else
            {
                numeric = false;
                break;
            }
        }
        if (numeric)
        {
            base = name.substr(0, open);
            start = value + 1;
        }
    }
    auto isTaken = [&](const std::string& candidate)
    {
        for (const auto& entity : scene.m_vEntities)
        {
            if (entity.m_Name == candidate)
            {
                return true;
            }
        }
        return false;
    };
    for (int suffix = start;; ++suffix)
    {
        const std::string candidate = base + " (" + std::to_string(suffix) + ")";
        if (!isTaken(candidate))
        {
            return candidate;
        }
    }
}

CEntity CSceneEntityCommands::CloneInstance(const CEntity& source, CScene& scene)
{
    CEntity copy = source;
    CMeshComponent* pMesh = copy.GetMeshComponent();
    if (pMesh)
    {
        pMesh->m_Model = {};
        pMesh->m_OwnsModelInstance = false;
        pMesh->m_OwnsMaterials = false;

        if (pMesh->m_pAsset && (pMesh->m_pAsset->m_IsProcedural ? static_cast<bool>(pMesh->m_pAsset->pfnGenerator) : !pMesh->m_pAsset->m_FilePath.empty()))
        {
            if (pMesh->m_pAsset->m_IsProcedural)
            {
                pMesh->m_Model = pMesh->m_pAsset->pfnGenerator(pMesh->m_Segments);
            }
            else
            {
                CModelService::LoadInstance(*pMesh->m_pAsset, pMesh->m_Model);
            }
            pMesh->m_OwnsModelInstance = pMesh->m_Model.meshes != nullptr;
            CEntityTextureService::StoreUV(&copy);
            CEntityTextureService::StoreMaterialTextures(&copy);
            CMeshOverrideService::Apply(copy);
            CEntityTextureService::RefreshEntityRenderState(copy);
        }
    }

    if (auto pLight = copy.GetLightComponent())
    {
        const int lightType = pLight->m_Light.m_Light.type;
        pLight->m_Created = false;
        pLight->m_Light.m_Id = -1;
        pLight->m_Light.m_Light = {};
        pLight->m_Light.m_Light.type = lightType;
        pLight->m_Light.m_Light.enabled = pLight->m_Light.m_Enabled;
        pLight->m_Light.m_Light.color = pLight->m_Light.m_Color;
        pLight->m_Light.m_Light.position = pLight->m_Light.m_Position;
        pLight->m_Light.m_Light.target = pLight->m_Light.m_Target;
    }

    copy.m_Id = static_cast<int>(scene.m_vEntities.size());
    copy.m_Name = MakeDuplicateName(scene, copy.m_Name);
    return copy;
}

std::vector<int> CSceneEntityCommands::GetRootSelectedIndices(const CScene& scene)
{
    std::vector<int> vValid;
    std::unordered_set<int> selectedSet;

    if (!scene.m_vSelectedEntities.empty())
    {
        for (int idx : scene.m_vSelectedEntities)
        {
            if (idx >= 0 && idx < static_cast<int>(scene.m_vEntities.size()) && selectedSet.insert(idx).second)
            {
                vValid.push_back(idx);
            }
        }
    }
    else if (scene.m_Selected >= 0 && scene.m_Selected < static_cast<int>(scene.m_vEntities.size()))
    {
        vValid.push_back(scene.m_Selected);
        selectedSet.insert(scene.m_Selected);
    }

    std::vector<int> vRoots;
    vRoots.reserve(vValid.size());
    for (int idx : vValid)
    {
        bool hasSelectedAncestor = false;
        int parentIdx = scene.m_vEntities[idx].m_ParentId;
        int guard = 0;
        while (parentIdx >= 0 && parentIdx < static_cast<int>(scene.m_vEntities.size()) && guard++ < 128)
        {
            if (selectedSet.count(parentIdx) > 0)
            {
                hasSelectedAncestor = true;
                break;
            }
            parentIdx = scene.m_vEntities[parentIdx].m_ParentId;
        }
        if (!hasSelectedAncestor)
        {
            vRoots.push_back(idx);
        }
    }

    return vRoots;
}

namespace
{
    int CloneSubtreeRecursive(CScene& scene, int sourceIndex, int newParentId, std::vector<int>& vAllCreated)
    {
        if (sourceIndex < 0 || sourceIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            return -1;
        }

        const std::vector<int> vSourceChildren = GetEntityChildren(scene, sourceIndex);

        CEntity copy = CSceneEntityCommands::CloneInstance(scene.m_vEntities[sourceIndex], scene);
        copy.m_ParentId = newParentId;
        scene.m_vEntities.push_back(std::move(copy));
        const int createdIndex = static_cast<int>(scene.m_vEntities.size()) - 1;
        vAllCreated.push_back(createdIndex);
        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, createdIndex);

        for (int childIndex : vSourceChildren)
        {
            CloneSubtreeRecursive(scene, childIndex, createdIndex, vAllCreated);
        }

        return createdIndex;
    }
} // anonymous

void CSceneEntityCommands::Duplicate(CEditor& editor, CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    const int sourceIndex = static_cast<int>(pEntity - editor.m_Scene.m_vEntities.data());
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
    {
        return;
    }

    editor.SaveState();
    const int parentId = editor.m_Scene.m_vEntities[sourceIndex].m_ParentId;
    std::vector<int> vAllCreated;
    const int rootCreated = CloneSubtreeRecursive(editor.m_Scene, sourceIndex, parentId, vAllCreated);
    if (rootCreated >= 0)
    {
        editor.m_Scene.MarkHierarchyDirty();
        editor.m_Scene.SelectEntity(rootCreated, false);
        editor.m_SceneDirty = true;
        editor.RequestSceneRedraw();
    }
}

void CSceneEntityCommands::DuplicateSelection(CEditor& editor)
{
    const std::vector<int> vRoots = GetRootSelectedIndices(editor.m_Scene);
    if (vRoots.empty())
    {
        return;
    }

    editor.SaveState();

    std::vector<int> vCreatedRoots;
    std::vector<int> vAllCreated;
    vCreatedRoots.reserve(vRoots.size());
    for (int rootIndex : vRoots)
    {
        if (rootIndex < 0 || rootIndex >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
        {
            continue;
        }
        const int parentId = editor.m_Scene.m_vEntities[rootIndex].m_ParentId;
        const int createdRoot = CloneSubtreeRecursive(editor.m_Scene, rootIndex, parentId, vAllCreated);
        if (createdRoot >= 0)
        {
            vCreatedRoots.push_back(createdRoot);
        }
    }

    if (!vCreatedRoots.empty())
    {
        editor.m_Scene.MarkHierarchyDirty();
        editor.m_Scene.m_vSelectedEntities = vCreatedRoots;
        editor.m_Scene.m_Selected = vCreatedRoots.back();
        editor.m_SceneDirty = true;
        editor.RequestSceneRedraw();
    }
}

void CSceneEntityCommands::Erase(CEditor& editor, int index)
{
    EraseEntityAndDescendants(
        editor.m_Scene, index,
        [&editor](CEntity& entity, int)
        {
            if (auto pLight = entity.GetLightComponent(); pLight && pLight->m_Created)
            {
                pLight->m_Light.m_Enabled = false;
                editor.m_Lights.Free(pLight->m_Light.m_Id);
                pLight->m_Created = false;
                pLight->m_Light.m_Id = -1;
            }

            if (auto pMesh = entity.GetMeshComponent())
            {
                CEntityTextureService::RestoreModelTextures(&entity);
                pMesh->ReleaseOwnedResources();
            }
        });
}

void CSceneEntityCommands::EraseSelection(CEditor& editor)
{
    const std::vector<int> vRoots = GetRootSelectedIndices(editor.m_Scene);
    if (vRoots.empty())
    {
        return;
    }

    EraseEntitiesAndDescendants(
        editor.m_Scene, vRoots,
        [&editor](CEntity& entity, int)
        {
            if (auto pLight = entity.GetLightComponent(); pLight && pLight->m_Created)
            {
                pLight->m_Light.m_Enabled = false;
                editor.m_Lights.Free(pLight->m_Light.m_Id);
                pLight->m_Created = false;
                pLight->m_Light.m_Id = -1;
            }

            if (auto pMesh = entity.GetMeshComponent())
            {
                CEntityTextureService::RestoreModelTextures(&entity);
                pMesh->ReleaseOwnedResources();
            }
        });

    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
}

void CSceneEntityCommands::Delete(CEditor& editor, CEntity* pEntity)
{
    SModalState& modal = editor.m_Ui.m_Modal;
    if (!pEntity)
    {
        return;
    }
    if (editor.m_Preferences.m_ConfirmDelete)
    {
        modal.m_PendingDelete.m_pEditor = &editor;
        modal.m_PendingDelete.m_pEntity = pEntity;
        modal.m_PendingDelete.m_DeleteSelection = false;
        return;
    }

    const int index = static_cast<int>(pEntity - editor.m_Scene.m_vEntities.data());
    editor.SaveState();
    Erase(editor, index);
}

void CSceneEntityCommands::DeleteSelection(CEditor& editor)
{
    const std::vector<int> vRoots = GetRootSelectedIndices(editor.m_Scene);
    if (vRoots.empty())
    {
        return;
    }
    if (vRoots.size() == 1)
    {
        Delete(editor, &editor.m_Scene.m_vEntities[vRoots.front()]);
        return;
    }

    if (editor.m_Preferences.m_ConfirmDelete)
    {
        SModalState& modal = editor.m_Ui.m_Modal;
        modal.m_PendingDelete.m_pEditor = &editor;
        modal.m_PendingDelete.m_pEntity = &editor.m_Scene.m_vEntities[vRoots.front()];
        modal.m_PendingDelete.m_DeleteSelection = true;
        return;
    }

    editor.SaveState();
    EraseSelection(editor);
}

int CSceneEntityCommands::GroupSelection(CEditor& editor)
{
    const std::vector<int> vRoots = GetRootSelectedIndices(editor.m_Scene);
    if (vRoots.empty())
    {
        return -1;
    }

    int commonParent = editor.m_Scene.m_vEntities[vRoots.front()].m_ParentId;
    for (size_t i = 1; i < vRoots.size(); ++i)
    {
        if (editor.m_Scene.m_vEntities[vRoots[i]].m_ParentId != commonParent)
        {
            commonParent = -1;
            break;
        }
    }

    editor.SaveState();

    const int groupIndex = CreateGroup(editor.m_Scene, "Group", commonParent);
    if (groupIndex < 0)
    {
        return -1;
    }

    for (int rootIndex : vRoots)
    {
        if (rootIndex != groupIndex)
        {
            MoveEntityToParent(editor.m_Scene, rootIndex, groupIndex);
        }
    }

    editor.m_Scene.SelectEntity(groupIndex, false);
    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
    return groupIndex;
}
