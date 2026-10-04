#include "editor/editor_entity_commands.h"

#include "editor/editor.h"
#include "editor/editor_state.h"
#include "models.h"
#include "tex.h"
#include "imgui.h"

#include <string>

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
    for (int suffix = start; ; ++suffix)
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

        if (pMesh->m_pAsset && (pMesh->m_pAsset->m_IsProcedural
            ? static_cast<bool>(pMesh->m_pAsset->pfnGenerator)
            : !pMesh->m_pAsset->m_FilePath.empty()))
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

void CSceneEntityCommands::Duplicate(CEditor& editor, CEntity* pEntity)
{
    if (!pEntity)
    {
        return;
    }
    editor.SaveState();
    CEntity copy = CloneInstance(*pEntity, editor.m_Scene);
    editor.m_Scene.m_vEntities.push_back(std::move(copy));
    editor.m_Scene.m_Selected = static_cast<int>(editor.m_Scene.m_vEntities.size()) - 1;
}

void CSceneEntityCommands::Erase(CEditor& editor, int index)
{
    if (index < 0 || index >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
    {
        return;
    }

    CEntity& entity = editor.m_Scene.m_vEntities[index];
    if (auto pLight = entity.GetLightComponent(); pLight && pLight->m_Created)
    {
        pLight->m_Light.m_Enabled = false;
        editor.m_Lights.Free(pLight->m_Light.m_Id);
        pLight->m_Created = false;
        pLight->m_Light.m_Id = -1;
    }

    if (auto pMesh = entity.GetMeshComponent())
    {
        pMesh->ReleaseOwnedResources();
    }

    editor.m_Scene.m_vEntities.erase(editor.m_Scene.m_vEntities.begin() + index);
    for (int current = 0; current < static_cast<int>(editor.m_Scene.m_vEntities.size()); ++current)
    {
        CEntity& remaining = editor.m_Scene.m_vEntities[current];
        remaining.m_Id = current;
        if (remaining.m_ParentId == index)
        {
            remaining.m_ParentId = -1;
        }
        else if (remaining.m_ParentId > index)
        {
            remaining.m_ParentId--;
        }
    }

    editor.m_Scene.m_Selected = -1;
    editor.m_Scene.m_vSelectedEntities.clear();
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
        modal.PendingDelete.pEditor = &editor;
        modal.PendingDelete.pEntity = pEntity;
        ImGui::OpenPopup("Confirm Delete");
        return;
    }

    const int index = static_cast<int>(pEntity - editor.m_Scene.m_vEntities.data());
    Erase(editor, index);
}
