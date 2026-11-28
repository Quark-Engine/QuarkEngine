#include "editor/editor_scene_drop.h"

#include "editor/editor.h"
#include "application_plugin_bridge.h"
#include "editor/editor_entity.h"
#include "editor/editor_scene_picker.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "engine/scene_document.h"
#include "engine/transform.h"
#include "models.h"
#include "scripting/script_component.h"
#include "scripting/script_engine.h"
#include "tex.h"
#include "ImGuizmo.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace
{

    fs::path ResolvePrefabDropPath(const CEditor& editor, const std::string& assetName)
    {
        std::error_code ec;
        const fs::path directPath(assetName);
        if (fs::is_regular_file(directPath, ec) && !ec)
        {
            return directPath;
        }

        if (!editor.m_CurrentAssetPath.empty())
        {
            const fs::path currentDirCandidate = editor.m_CurrentAssetPath / directPath;
            if (fs::is_regular_file(currentDirCandidate, ec) && !ec)
            {
                return currentDirCandidate;
            }
        }

        if (!editor.m_ProjectPath.empty())
        {
            const fs::path resourcesCandidate = fs::path(editor.m_ProjectPath) / "resources" / directPath;
            if (fs::is_regular_file(resourcesCandidate, ec) && !ec)
            {
                return resourcesCandidate;
            }
        }

        return directPath;
    }

    bool CanParsePrefabFile(const fs::path& prefabPath)
    {
        std::ifstream file(prefabPath);
        if (!file.is_open())
        {
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return quark::CSceneDocument::ParsePrefab(buffer.str()).m_IsValid;
    }

    std::string LowerExtension(const fs::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    }

    CEntity* RaycastSceneEntity(CEditor& editor, const Camera3D& camera, const ImVec2& mouse)
    {
        const Ray ray = GetScreenToWorldRay({ mouse.x, mouse.y }, camera);

        CEntity* pHitEntity = nullptr;
        float bestDistance = FLT_MAX;

        for (CEntity& entity : editor.m_Scene.m_vEntities)
        {
            float entityDistance = FLT_MAX;
            if (!CScenePicker::RaycastEntity(editor.m_Scene, entity, ray, entityDistance))
            {
                continue;
            }
            if (entityDistance < bestDistance)
            {
                bestDistance = entityDistance;
                pHitEntity = &entity;
            }
        }
        return pHitEntity;
    }

} // anonymous

bool CSceneAssetDrop::IsTextureExtension(const fs::path& path)
{
    const std::string ext = LowerExtension(path);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

bool CSceneAssetDrop::IsMaterialExtension(const fs::path& path)
{
    return LowerExtension(path) == ".mtl";
}

bool CSceneAssetDrop::IsScriptExtension(const fs::path& path)
{
    const std::string ext = LowerExtension(path);
    return ext == ".hpp" || ext == ".h" || ext == ".cpp";
}

bool CSceneAssetDrop::IsModelPath(const CEditor& editor, const std::string& assetName, const fs::path& path)
{
    if (CModelService::IsModelFile(path))
    {
        return true;
    }
    return editor.m_Assets.FindModelByName(assetName) != nullptr;
}

bool CSceneAssetDrop::ApplyTextureToEntity(CEditor& editor, CEntity& entity,
                                           const fs::path& texturePath, bool asNormalMap)
{
    CMaterialComponent* pMat = entity.GetMaterialComponent();
    if (!pMat)
    {
        return false;
    }

    const std::string fileName = texturePath.filename().string();
    const std::string assetName = CAssetLibrary::AssetNameForPath(fs::path(editor.m_ProjectPath), texturePath);

    int matchedIndex = -1;
    for (size_t i = 1; i < editor.m_Assets.TextureCount(); ++i)
    {
        const auto& texEntry = editor.m_Assets.TextureByIndex(i);
        if (texEntry.m_Name == assetName || texEntry.m_Name == fileName ||
            fs::path(texEntry.m_Name).filename().string() == fileName)
        {
            matchedIndex = static_cast<int>(i);
            break;
        }
    }

    Texture2D tex = { 0 };
    std::string resolvedName = fileName;
    if (matchedIndex >= 0)
    {
        resolvedName = editor.m_Assets.TextureByIndex(matchedIndex).m_Name;
        tex = editor.m_Assets.TextureByIndex(matchedIndex).m_Texture;
    }
    else
    {
        std::error_code ec;
        if (!fs::is_regular_file(texturePath, ec) || ec)
        {
            return false;
        }
        const Texture2D* pLoaded = editor.m_Textures.Load(texturePath.string());
        if (!pLoaded || !pLoaded->valid)
        {
            return false;
        }
        tex = *pLoaded;
        resolvedName = assetName.empty() ? fileName : assetName;
    }

    editor.SaveState();
    if (asNormalMap)
    {
        pMat->m_NormalTextureName = resolvedName;
        if (CMeshComponent* pMesh = entity.GetMeshComponent())
        {
            for (int m = 0; m < pMesh->m_Model.materialCount; ++m)
            {
                if (pMesh->m_Model.materials && pMesh->m_Model.materials[m].maps)
                {
                    pMesh->m_Model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = tex;
                }
            }
        }
    }
    else
    {
        pMat->m_AlbedoTextureName = resolvedName;
        pMat->m_TextureName.clear();
        pMat->m_Texture = tex;
        pMat->m_TextureSource = TEXTURE_EXTERNAL;
        CEntityTextureService::MarkEntityUVDirty(&entity);
    }

    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
    return true;
}

bool CSceneAssetDrop::ApplyMaterialToEntity(CEditor& editor, CEntity& entity,
                                            const fs::path& materialPath, int slot)
{
    CMaterialComponent* pMat = entity.GetMaterialComponent();
    if (!pMat)
    {
        return false;
    }

    std::error_code ec;
    fs::path resolvedPath = materialPath;
    if (!fs::is_regular_file(resolvedPath, ec) || ec)
    {
        return false;
    }

    editor.SaveState();
    if (slot >= 0)
    {
        LoadMaterialToEntity(&entity, resolvedPath.string(), slot);
        if (CMeshComponent* pMesh = entity.GetMeshComponent(); pMesh && pMesh->m_Model.materialCount > slot)
        {
            if (static_cast<int>(pMat->m_vMaterialSlotSources.size()) <= slot)
            {
                pMat->m_vMaterialSlotSources.resize(pMesh->m_Model.materialCount);
            }
            pMat->m_vMaterialSlotSources[slot] = resolvedPath.generic_string();
        }
    }
    else
    {
        LoadMaterialToEntity(&entity, resolvedPath.string());
    }
    CEntityTextureService::MarkEntityUVDirty(&entity);
    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
    return true;
}

bool CSceneAssetDrop::ApplyScriptToEntity(CEditor& editor, CEntity& entity,
                                          const fs::path& scriptPath)
{
    std::error_code ec;
    fs::path headerPath = scriptPath;
    if (LowerExtension(headerPath) == ".cpp")
    {
        fs::path hppCandidate = headerPath;
        hppCandidate.replace_extension(".hpp");
        if (fs::is_regular_file(hppCandidate, ec) && !ec)
        {
            headerPath = hppCandidate;
        }
        else
        {
            fs::path hCandidate = headerPath;
            hCandidate.replace_extension(".h");
            if (fs::is_regular_file(hCandidate, ec) && !ec)
            {
                headerPath = hCandidate;
            }
        }
    }

    if (!fs::is_regular_file(headerPath, ec) || ec)
    {
        return false;
    }

    quark::script::SParsedScriptInfo parsed;
    if (!quark::script::CScriptHeaderParser::ParseHeader(headerPath.string(), parsed) || parsed.m_ClassName.empty())
    {
        return false;
    }

    editor.SaveState();
    CScriptComponent* pScript = entity.GetScriptComponent();
    if (!pScript)
    {
        auto pNewScript = std::make_shared<CScriptComponent>();
        entity.GetComponents()->AddComponent(pNewScript);
        pScript = pNewScript.get();
    }

    pScript->m_ScriptName = parsed.m_ClassName;
    pScript->m_ScriptPath = headerPath.string();
    pScript->SyncWithHeaderFields(parsed.m_vFields);

    quark::script::CScriptEngine::Get().RefreshAvailableScripts(editor.m_ProjectPath);
    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
    return true;
}

bool CSceneAssetDrop::ApplyModelToEntity(CEditor& editor, CEntity& entity,
                                         const std::string& assetName, const fs::path& assetPath)
{
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return false;
    }

    CModelAsset* pAsset = editor.m_Assets.FindModelByName(assetName);
    if (!pAsset && !assetPath.empty())
    {
        const std::string normalizedName = CAssetLibrary::AssetNameForPath(fs::path(editor.m_ProjectPath), assetPath);
        pAsset = editor.m_Assets.FindModelByName(normalizedName);
    }
    if (!pAsset)
    {
        return false;
    }

    Model newModel = {};
    if (pAsset->m_IsProcedural)
    {
        if (!pAsset->pfnGenerator)
        {
            return false;
        }
        newModel = pAsset->pfnGenerator(pMesh->m_Segments);
    }
    else
    {
        CModelService::LoadInstance(*pAsset, newModel);
    }

    if (!HasValidModelData(newModel))
    {
        return false;
    }

    editor.SaveState();
    CEntityTextureService::RestoreModelTextures(&entity);
    pMesh->ReleaseOwnedResources();
    pMesh->m_Model = newModel;
    pMesh->m_OwnsModelInstance = true;
    pMesh->m_OwnsMaterials = false;
    pMesh->m_pAsset = pAsset;
    pMesh->m_AssetName = pAsset->m_Name;
    pMesh->m_Type = pAsset->m_Type;
    pMesh->m_IsEditableMesh = false;
    CEntityTextureService::StoreUV(&entity);
    CEntityTextureService::StoreMaterialTextures(&entity);
    CEntityTextureService::MarkEntityBoundsDirty(&entity);
    CEntityTextureService::MarkEntityUVDirty(&entity);
    CEntityTextureService::RefreshEntityRenderState(entity);

    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
    return true;
}

bool CSceneAssetDrop::ApplyAssetToEntity(CEditor& editor, CEntity& entity,
                                         const std::string& assetName, const std::string& assetPath)
{
    const fs::path path = !assetPath.empty() ? fs::path(assetPath) : fs::path(assetName);
    if (IsMaterialExtension(path))
    {
        return ApplyMaterialToEntity(editor, entity, path);
    }
    if (IsTextureExtension(path))
    {
        return ApplyTextureToEntity(editor, entity, path, false);
    }
    if (IsScriptExtension(path))
    {
        return ApplyScriptToEntity(editor, entity, path);
    }
    if (IsModelPath(editor, assetName, path))
    {
        return ApplyModelToEntity(editor, entity, assetName, path);
    }
    return false;
}

void CSceneAssetDrop::Handle(CEditor& editor, const Camera3D& camera)
{
    CViewportState& viewport = editor.m_Ui.m_Viewport;
    if (!editor.m_Ui.m_Layout.m_SceneAssetDragging)
    {
        return;
    }
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const std::string assetName = editor.m_Ui.m_Layout.m_DraggedSceneAssetName;
    const std::string assetPath = editor.m_Ui.m_Layout.m_DraggedSceneAssetPath;
    editor.m_Ui.m_Layout.m_SceneAssetDragging = false;
    editor.m_Ui.m_Layout.m_DraggedSceneAssetName.clear();
    editor.m_Ui.m_Layout.m_DraggedSceneAssetPath.clear();

    if (ImGuizmo::IsUsing())
    {
        return;
    }

    const ImVec2 mouse = { (float)GetMouseX(), (float)GetMouseY() };

    const bool mouseOverScene =
        mouse.x >= viewport.m_WindowPos.x && mouse.x <= viewport.m_WindowPos.x + viewport.m_WindowSize.x &&
        mouse.y >= viewport.m_WindowPos.y && mouse.y <= viewport.m_WindowPos.y + viewport.m_WindowSize.y;

    if (!mouseOverScene)
    {
        return;
    }

    const fs::path dropPath = !assetPath.empty() ? fs::path(assetPath) : fs::path(assetName);

    if (IsMaterialExtension(dropPath) || IsTextureExtension(dropPath) || IsScriptExtension(dropPath))
    {
        if (CEntity* pHitEntity = RaycastSceneEntity(editor, camera, mouse))
        {
            ApplyAssetToEntity(editor, *pHitEntity, assetName, assetPath);
        }
        return;
    }

    if (assetName.size() >= 7 && assetName.substr(assetName.size() - 7) == ".prefab")
    {
        const fs::path prefabPath = ResolvePrefabDropPath(editor, assetName);
        if (!CanParsePrefabFile(prefabPath))
        {
            return;
        }

        editor.SaveState();
        const std::vector<int> vCreatedIndices = CEntityFactory::InstantiatePrefab(
            editor.m_Scene, editor.m_Assets, prefabPath, editor.m_ComponentFactories, -1);
        if (vCreatedIndices.empty())
        {
            return;
        }

        const int rootIndex = vCreatedIndices.front();
        CEntity& rootEntity = editor.m_Scene.m_vEntities[rootIndex];
        CTransformComponent* pRootTransform = rootEntity.GetTransformComponent();
        const Vec3 dropPosition = GetSceneDropPosition(camera);

        if (pRootTransform)
        {
            if (rootEntity.m_IsGroup && vCreatedIndices.size() > 1 &&
                !pRootTransform->m_HasLocalMatrixOverride &&
                std::fabs(pRootTransform->m_Position.x) <= 0.0001f &&
                std::fabs(pRootTransform->m_Position.y) <= 0.0001f &&
                std::fabs(pRootTransform->m_Position.z) <= 0.0001f)
            {
                Vec3 childPivot = { 0.0f, 0.0f, 0.0f };
                int directChildCount = 0;
                for (size_t i = 1; i < vCreatedIndices.size(); ++i)
                {
                    const CEntity& child = editor.m_Scene.m_vEntities[vCreatedIndices[i]];
                    if (child.m_ParentId != rootIndex)
                    {
                        continue;
                    }
                    if (const CTransformComponent* pChildTransform = child.GetTransformComponent())
                    {
                        childPivot = childPivot + pChildTransform->m_Position;
                        ++directChildCount;
                    }
                }
                if (directChildCount > 0)
                {
                    childPivot = childPivot * (1.0f / static_cast<float>(directChildCount));
                    const Vec3 horizontalOffset = { childPivot.x, 0.0f, childPivot.z };
                    for (size_t i = 1; i < vCreatedIndices.size(); ++i)
                    {
                        CEntity& child = editor.m_Scene.m_vEntities[vCreatedIndices[i]];
                        if (child.m_ParentId != rootIndex)
                        {
                            continue;
                        }
                        if (CTransformComponent* pChildTransform = child.GetTransformComponent())
                        {
                            pChildTransform->m_Position = pChildTransform->m_Position - horizontalOffset;
                            if (pChildTransform->m_HasLocalMatrixOverride)
                            {
                                pChildTransform->m_LocalMatrixOverride.m[12] -= horizontalOffset.x;
                                pChildTransform->m_LocalMatrixOverride.m[14] -= horizontalOffset.z;
                            }
                        }
                    }
                }
            }

            pRootTransform->ClearLocalMatrixOverride();
            pRootTransform->m_Position = dropPosition;
        }

        editor.m_Scene.SelectEntity(rootIndex, false);
        for (int createdIndex : vCreatedIndices)
        {
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, createdIndex);
        }
        editor.m_SceneDirty = true;
        editor.RequestSceneRedraw();

        return;
    }

    CModelAsset* pAsset = editor.m_Assets.FindModelByName(assetName);
    if (!pAsset)
    {
        return;
    }

    CEntity entity = CEntityFactory::FromAsset(editor.m_Scene, *pAsset);
    CMeshComponent* pMesh = entity.GetMeshComponent();
    CTransformComponent* pTransform = entity.GetTransformComponent();

    if (!pMesh || !pTransform || !HasValidModelData(pMesh->m_Model))
    {
        return;
    }

    editor.SaveState();
    pTransform->ClearLocalMatrixOverride();
    pTransform->m_Position = GetSceneDropPosition(camera);

    editor.m_Scene.m_vEntities.push_back(std::move(entity));
    const int entityIndex = static_cast<int>(editor.m_Scene.m_vEntities.size()) - 1;
    editor.m_Scene.SelectEntity(entityIndex, false);
    editor.m_SceneDirty = true;
    editor.RequestSceneRedraw();
    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
}
