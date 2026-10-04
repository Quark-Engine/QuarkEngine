#include "scene.h"
#include "models.h"
#include "application_plugin_bridge.h"
#include <algorithm>
#include <unordered_set>

using namespace qc;

CEntity* CScene::GetSelected()
{
    if (m_Selected < 0 || m_Selected >= static_cast<int>(m_vEntities.size())) return nullptr;
    return &m_vEntities[m_Selected];
}

bool CScene::IsSelected(int entityIndex) const
{
    return std::find(m_vSelectedEntities.begin(), m_vSelectedEntities.end(), entityIndex) != m_vSelectedEntities.end();
}

void CScene::SelectEntity(int entityIndex, bool additive)
{
    if (entityIndex < 0 || entityIndex >= static_cast<int>(m_vEntities.size())) return;
    if (!additive) m_vSelectedEntities.clear();

    auto it = std::find(m_vSelectedEntities.begin(), m_vSelectedEntities.end(), entityIndex);
    if (additive && it != m_vSelectedEntities.end())
    {
        m_vSelectedEntities.erase(it);
        m_Selected = m_vSelectedEntities.empty() ? -1 : m_vSelectedEntities.back();
        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_SELECTED, m_Selected);
        return;
    }

    if (it == m_vSelectedEntities.end()) m_vSelectedEntities.push_back(entityIndex);
    m_Selected = entityIndex;
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_SELECTED, entityIndex);
}

std::string CScene::MakeUniqueName(const std::string& baseName) const
{
    auto isNameTaken = [this](const std::string& candidate)
    {
        for (const auto& entity : m_vEntities)
        {
            if (entity.m_Name == candidate) return true;
        }
        return false;
    };

    if (!isNameTaken(baseName)) return baseName;

    for (int suffix = 1;; ++suffix)
    {
        const std::string candidate = baseName + " (" + std::to_string(suffix) + ")";
        if (!isNameTaken(candidate)) return candidate;
    }
}

std::string CScene::MakeDefaultNameFor(const CEntity& entity) const
{
    const CLightComponent* pLight = entity.GetLightComponent();
    const CMeshComponent* pMesh = entity.GetMeshComponent();
    const std::string baseName = (pLight && pLight->m_Enabled)
        ? "Light"
        : ObjectTypeName(pMesh ? pMesh->m_Type : OBJECT_CUBE);
    return MakeUniqueName(baseName);
}

void CScene::ReleaseResources()
{
    for (auto& entity : m_vEntities)
    {
        CMeshComponent* pMesh = entity.GetMeshComponent();
        CMaterialComponent* pMaterial = entity.GetMaterialComponent();
        if (!pMesh) continue;

        pMesh->ReleaseOwnedResources();
        if (pMaterial) pMaterial->m_Texture = {0};
    }

    m_vEntities.clear();
    m_Selected = -1;
    m_vSelectedEntities.clear();
}
