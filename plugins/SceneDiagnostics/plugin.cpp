#include "plugins/plugin.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
struct SDiagnosticsState
{
    bool m_Dirty = true;
    int m_ComponentCount = 0;
    int m_MaterialCount = 0;
    std::vector<int> m_vMissingTransform;
    std::vector<int> m_vMissingMesh;
    std::vector<int> m_vDuplicateNames;
    std::vector<int> m_vBrokenAssets;
    std::vector<std::pair<int, std::string>> m_vDuplicateTags;
};

SDiagnosticsState g_State;

void MarkDiagnosticsDirty(SPluginContext* pCtx, EPluginEvent event, int entityIndex)
{
    (void)pCtx;
    (void)entityIndex;
    if (event == PLUGIN_EVENT_ENTITY_CREATED ||
        event == PLUGIN_EVENT_ENTITY_DELETED ||
        event == PLUGIN_EVENT_TRANSFORM_CHANGED ||
        event == PLUGIN_EVENT_SCENE_LOADED)
    {
        g_State.m_Dirty = true;
    }
}

void ResetReport()
{
    g_State.m_ComponentCount = 0;
    g_State.m_MaterialCount = 0;
    g_State.m_vMissingTransform.clear();
    g_State.m_vMissingMesh.clear();
    g_State.m_vDuplicateNames.clear();
    g_State.m_vBrokenAssets.clear();
    g_State.m_vDuplicateTags.clear();
}

void RebuildReport(SPluginContext* pCtx)
{
    if (!pCtx || !pCtx->pScene)
    {
        return;
    }

    ResetReport();
    std::unordered_map<std::string, std::vector<int>> nameToEntities;

    for (int index = 0; index < pCtx->entityCount; ++index)
    {
        const int componentCount = pCtx->pfnEntityGetComponentCount(pCtx->pScene, index);
        g_State.m_ComponentCount += componentCount;

        if (pCtx->pfnEntityHasComponent(pCtx->pScene, index, "Material"))
        {
            ++g_State.m_MaterialCount;
        }
        if (!pCtx->pfnEntityHasComponent(pCtx->pScene, index, "Transform"))
        {
            g_State.m_vMissingTransform.push_back(index);
        }
        if (!pCtx->pfnEntityHasComponent(pCtx->pScene, index, "Mesh"))
        {
            g_State.m_vMissingMesh.push_back(index);
        }

        const char* pName = pCtx->pfnEntityGetName(pCtx->pScene, index);
        if (pName && pName[0] != '\0')
        {
            nameToEntities[pName].push_back(index);
        }

        if (pCtx->pfnEntityGetAssetName && pCtx->pfnAssetExists)
        {
            const char* pAssetName = pCtx->pfnEntityGetAssetName(pCtx->pScene, index);
            if (pAssetName && pAssetName[0] != '\0' &&
                !pCtx->pfnAssetExists(pCtx->pAssets, pAssetName))
            {
                g_State.m_vBrokenAssets.push_back(index);
            }
        }

        std::unordered_set<std::string> tags;
        const int tagCount = pCtx->pfnEntityGetTagCount(pCtx->pScene, index);
        for (int tagIndex = 0; tagIndex < tagCount; ++tagIndex)
        {
            const char* pTag = pCtx->pfnEntityGetTag(pCtx->pScene, index, tagIndex);
            if (pTag && !tags.insert(pTag).second)
            {
                g_State.m_vDuplicateTags.emplace_back(index, pTag);
            }
        }
    }

    for (const auto& [name, vEntities] : nameToEntities)
    {
        if (vEntities.size() > 1)
        {
            g_State.m_vDuplicateNames.insert(g_State.m_vDuplicateNames.end(),
                vEntities.begin(), vEntities.end());
        }
    }

    std::sort(g_State.m_vDuplicateNames.begin(), g_State.m_vDuplicateNames.end());
    g_State.m_Dirty = false;
}

std::string EntityLabel(SPluginContext* pCtx, int index)
{
    const char* pName = pCtx->pfnEntityGetName(pCtx->pScene, index);
    return std::string(pName ? pName : "<unnamed>") + " [" + std::to_string(index) + "]";
}

void DrawEntityList(SPluginContext* pCtx, const char* pTitle, const std::vector<int>& vIndices,
                    const char* pId)
{
    if (vIndices.empty())
    {
        return;
    }

    pCtx->pfnUiText(pTitle);
    for (int index : vIndices)
    {
        const std::string label = "Focus " + EntityLabel(pCtx, index) + "##" + pId +
            std::to_string(index);
        if (pCtx->pfnUiButton(label.c_str()))
        {
            pCtx->pfnEditorFocusEntity(index);
        }
    }
}

void FixMissingComponents(SPluginContext* pCtx)
{
    if (g_State.m_vMissingTransform.empty() && g_State.m_vMissingMesh.empty())
    {
        return;
    }

    pCtx->pfnSceneBeginCommand(pCtx->pScene, "Fix missing scene components");
    for (int index : g_State.m_vMissingTransform)
    {
        pCtx->pfnEntityAddComponent(pCtx->pScene, index, "Transform");
    }
    for (int index : g_State.m_vMissingMesh)
    {
        pCtx->pfnEntityAddComponent(pCtx->pScene, index, "Mesh");
    }
    pCtx->pfnSceneEndCommand(pCtx->pScene);
    g_State.m_Dirty = true;
}

void FixDuplicateNames(SPluginContext* pCtx)
{
    std::unordered_map<std::string, int> seen;
    pCtx->pfnSceneBeginCommand(pCtx->pScene, "Fix duplicate entity names");
    for (int index = 0; index < pCtx->entityCount; ++index)
    {
        const char* pName = pCtx->pfnEntityGetName(pCtx->pScene, index);
        const std::string name = pName ? pName : "Entity";
        const int occurrence = seen[name]++;
        if (occurrence > 0)
        {
            const std::string fixedName = name + " (Diagnostics " + std::to_string(occurrence + 1) + ")";
            pCtx->pfnEntitySetName(pCtx->pScene, index, fixedName.c_str());
        }
    }
    pCtx->pfnSceneEndCommand(pCtx->pScene);
    g_State.m_Dirty = true;
}

void FixDuplicateTags(SPluginContext* pCtx)
{
    if (g_State.m_vDuplicateTags.empty())
    {
        return;
    }

    pCtx->pfnSceneBeginCommand(pCtx->pScene, "Fix duplicate entity tags");
    for (const auto& [index, tag] : g_State.m_vDuplicateTags)
    {
        pCtx->pfnEntityRemoveTag(pCtx->pScene, index, tag.c_str());
    }
    pCtx->pfnSceneEndCommand(pCtx->pScene);
    g_State.m_Dirty = true;
}

void FixAll(SPluginContext* pCtx)
{
    FixMissingComponents(pCtx);
    FixDuplicateNames(pCtx);
    FixDuplicateTags(pCtx);
    if (pCtx->pfnEditorSetStatusMessage)
    {
        pCtx->pfnEditorSetStatusMessage("Scene diagnostics fixes applied. Use Undo to revert them.");
    }
}

void OnLoad(SPluginContext* pCtx)
{
    pCtx->pfnRegisterEventCallback(pCtx, PLUGIN_EVENT_ENTITY_CREATED, MarkDiagnosticsDirty);
    pCtx->pfnRegisterEventCallback(pCtx, PLUGIN_EVENT_ENTITY_DELETED, MarkDiagnosticsDirty);
    pCtx->pfnRegisterEventCallback(pCtx, PLUGIN_EVENT_TRANSFORM_CHANGED, MarkDiagnosticsDirty);
    pCtx->pfnRegisterEventCallback(pCtx, PLUGIN_EVENT_SCENE_LOADED, MarkDiagnosticsDirty);
    RebuildReport(pCtx);
}

void OnUnload()
{
    g_State = {};
}

void OnUpdate(SPluginContext* pCtx)
{
    if (g_State.m_Dirty)
    {
        RebuildReport(pCtx);
    }
}

void OnDrawUI(SPluginContext* pCtx)
{
    if (g_State.m_Dirty)
    {
        RebuildReport(pCtx);
    }

    const bool isOpen = pCtx->pfnUiBegin("Scene Diagnostics");
    if (isOpen)
    {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Entities: %d", pCtx->entityCount);
        pCtx->pfnUiText(buffer);
        std::snprintf(buffer, sizeof(buffer), "Components: %d", g_State.m_ComponentCount);
        pCtx->pfnUiText(buffer);
        std::snprintf(buffer, sizeof(buffer), "Materials: %d", g_State.m_MaterialCount);
        pCtx->pfnUiText(buffer);
        pCtx->pfnUiSeparator();

        std::snprintf(buffer, sizeof(buffer), "Missing Transform: %zu", g_State.m_vMissingTransform.size());
        pCtx->pfnUiText(buffer);
        DrawEntityList(pCtx, "Entities without Transform", g_State.m_vMissingTransform, "missing_transform_");
        if (!g_State.m_vMissingTransform.empty() && pCtx->pfnUiButton("Fix missing components"))
        {
            FixMissingComponents(pCtx);
        }

        std::snprintf(buffer, sizeof(buffer), "Missing Mesh: %zu", g_State.m_vMissingMesh.size());
        pCtx->pfnUiText(buffer);
        DrawEntityList(pCtx, "Entities without Mesh", g_State.m_vMissingMesh, "missing_mesh_");

        std::snprintf(buffer, sizeof(buffer), "Duplicate names: %zu", g_State.m_vDuplicateNames.size());
        pCtx->pfnUiText(buffer);
        DrawEntityList(pCtx, "Entities with duplicate names", g_State.m_vDuplicateNames, "duplicate_name_");
        if (!g_State.m_vDuplicateNames.empty() && pCtx->pfnUiButton("Fix duplicate names"))
        {
            FixDuplicateNames(pCtx);
        }

        std::snprintf(buffer, sizeof(buffer), "Broken asset references: %zu", g_State.m_vBrokenAssets.size());
        pCtx->pfnUiText(buffer);
        DrawEntityList(pCtx, "Entities with broken assets", g_State.m_vBrokenAssets, "broken_asset_");

        std::snprintf(buffer, sizeof(buffer), "Duplicate tags: %zu", g_State.m_vDuplicateTags.size());
        pCtx->pfnUiText(buffer);
        if (!g_State.m_vDuplicateTags.empty() && pCtx->pfnUiButton("Fix duplicate tags"))
        {
            FixDuplicateTags(pCtx);
        }

        pCtx->pfnUiSeparator();
        if (pCtx->pfnUiButton("Fix all diagnostics"))
        {
            FixAll(pCtx);
        }
        if (pCtx->pfnUiButton("Undo diagnostics fix"))
        {
            pCtx->pfnSceneUndo();
            g_State.m_Dirty = true;
        }
        if (pCtx->pfnUiButton("Redo diagnostics fix"))
        {
            pCtx->pfnSceneRedo();
            g_State.m_Dirty = true;
        }
        if (pCtx->pfnUiButton("Refresh diagnostics"))
        {
            RebuildReport(pCtx);
        }
    }
    pCtx->pfnUiEnd();
}

SPlugin g_Plugin
{
    "SceneDiagnostics", "1.0.0",
    OnLoad, OnUnload, OnUpdate, OnDrawUI
};

} // anonymous

PLUGIN_EXPORT SPlugin* GetPlugin()
{
    return &g_Plugin;
}
