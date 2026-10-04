#ifndef __EDITOR_H__
#define __EDITOR_H__

#include "QuarkCore/QuarkCore.hpp"

#include "assets/asset_library.h"
#include "assets/preview_cache.h"
#include "assets/texture_cache.h"
#include "editor/editor_preferences.h"
#include "editor/editor_state.h"
#include "engine/component_factory_registry.h"
#include "engine/scene_document.h"
#include "lighting.h"
#include "scene.h"
#include "text_mesh.h"

#include <filesystem>
#include <stack>
#include <string>

class CFlyCamera;
class CPluginManager;

struct SPluginContext;

class CEditor
{
public:
    CEditor();
    ~CEditor();

    CEditor(const CEditor&) = delete;
    CEditor& operator=(const CEditor&) = delete;

    void DrawUi(qc::Shader shader, CFlyCamera& camera, SPluginContext* pCtx);
    void DrawAssetsUi();
    void HandleInput();
    void SaveState();
    void Undo();
    void Redo();

    void Unload();

    CAssetLibrary m_Assets;
    CPreviewCache m_Previews;

    CTextureCache m_Textures;
    CLightRegistry m_Lights;

    CComponentFactoryRegistry m_ComponentFactories;

    CPreferences m_Preferences;

    CFreetypeTextMesh m_Text;

    CScene m_Scene;
    std::string m_ProjectPath = "projects/TestProject";

    CEditorUiState m_Ui;

    int m_SelectedAssetIndex = -1;
    std::string m_SelectedAssetName;
    bool m_SceneDirty = false;

    std::stack<quark::SSceneSnapshot> m_UndoStack;
    std::stack<quark::SSceneSnapshot> m_RedoStack;

    CPluginManager* m_pPluginManager = nullptr;

    std::filesystem::path m_CurrentAssetPath;

private:
    void DrawMainMenuBar(SPluginContext* pCtx, ImGuiID dockspaceId);
    void DrawHierarchyPanel(SPluginContext* pCtx);
    void DrawInspectorPanel(qc::Shader shader, SPluginContext* pCtx);
    void DrawScenePanel(CFlyCamera& camera, SPluginContext* pCtx);
    void DrawRenameModal();
    void DrawAboutModal();
    void DrawPreferencesUi(CFlyCamera& camera);
    void DrawConfirmationModals();

    void HierarchyAcceptEntityDrop(int targetIndex);
    void HierarchyDrawCreateMenu(int parentIndex);
    void HierarchyDrawEntityItem(int entityIndex);
    void HierarchyDrawEntityTree(int parentId);

    bool DrawPreferencesGeneralTab();
    bool DrawPreferencesRenderingTab(CFlyCamera& camera);
    bool DrawPreferencesInterfaceTab();
};

#endif // __EDITOR_H__
