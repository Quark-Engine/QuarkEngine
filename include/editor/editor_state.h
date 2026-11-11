#ifndef __EDITOR_EDITOR_STATE_H__
#define __EDITOR_EDITOR_STATE_H__

#include "QuarkCore/QuarkCore.hpp"
#include "../entity.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

struct SKeyRepeat
{
    static constexpr float HOLD_DELAY = 0.5f;
    static constexpr float REPEAT_INTERVAL = 0.15f;

    bool m_WasPressed = false;
    float m_HoldStart = 0.0f;
    float m_LastFire = 0.0f;

    bool ShouldFire(float now)
    {
        if (!m_WasPressed)
        {
            m_WasPressed = true;
            m_HoldStart = now;
            m_LastFire = now;
            return true;
        }

        if (now - m_HoldStart > HOLD_DELAY && now - m_LastFire > REPEAT_INTERVAL)
        {
            m_LastFire = now;
            return true;
        }

        return false;
    }

    void Release()
    {
        m_WasPressed = false;
        m_HoldStart = 0.0f;
    }
};

struct SMeshEditState
{
    ImGuizmo::OPERATION m_GizmoMode = ImGuizmo::TRANSLATE;
    int m_RenamingIndex = -1;
    char m_aRenameBuffer[128] = "";
    bool m_HasClipboard = false;
    CEntity m_ClipboardData;

    SKeyRepeat m_Undo;
    SKeyRepeat m_Redo;
    SKeyRepeat m_Copy;
    SKeyRepeat m_Paste;
    SKeyRepeat m_Duplicate;
};

enum EPolygonEditMode
{
    POLY_NONE,
    POLY_CREATE,
    POLY_MOVE
};

struct SVertexEditState
{
    bool m_Enabled = false;
    int m_EntityIndex = -1;
    int m_MeshIndex = 0;
    int m_TriangleIndex = 0;
    int m_VertexCorner = 0;

    std::vector<int> m_vSelectedVertices;
};

class CViewportState
{
public:
    CViewportState() = default;
    CViewportState(const CViewportState&) = delete;
    CViewportState& operator=(const CViewportState&) = delete;
    ~CViewportState() = default;

    RenderTexture2D m_RenderTexture = { 0 };
    ImVec2 m_WindowPos = { 0, 0 };
    ImVec2 m_WindowSize = { 0, 0 };
    bool m_Hovered = false;
    bool m_Focused = false;

    void Release();
    void Unload();
};

class CEditor;

struct SPendingEntityDelete
{
    CEditor* m_pEditor = nullptr;
    CEntity* m_pEntity = nullptr;
};

struct SDeferredHierarchyDelete
{
    CEditor* m_pEditor = nullptr;
    int m_Index = -1;
};

struct SModalState
{
    bool m_ShowExitConfirmation = false;
    bool m_ShowPreferences = false;
    SPendingEntityDelete m_PendingDelete;
    SDeferredHierarchyDelete m_DeferredHierarchyDelete;
};

struct SGizmoState
{
    bool m_WasUsingTransform = false;
    bool m_WasUsingMeshEdit = false;
    bool m_WasUsingPolygon = false;
};

struct SInspectorUiState
{
    char m_aTagBuffer[128] = {};

    int m_ComponentToRemove = -1;
    int m_SelectedTextureIndex = 0;
    int m_SelectedNormalIndex = 0;
};

struct SPreferencesUiState
{
    int m_LanguageIndex = -1;
};

struct SLayoutState
{
    bool m_ShowAboutWindow = false;

    bool m_SceneAssetDragging = false;
    std::string m_DraggedSceneAssetName;

    bool m_FileDragging = false;
    int m_DraggedFileIndex = -1;
    int m_DraggedTargetFolderIndex = -1;

    ImGuiDockNodeFlags m_DockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    bool m_LayoutMarkerChecked = false;
};

struct SAssetBrowserMarquee
{
    ImVec2 m_Start = {};
    ImVec2 m_End = {};
    bool m_Active = false;
};

struct SAssetBrowserDrag
{
    std::filesystem::path m_FilePath;
    std::string m_FileName;
    ImVec2 m_StartPos = {};
};

struct SAssetBrowserCreatePopup
{
    bool m_Open = false;
    bool m_CreatingFolder = false;
    bool m_CreatingScript = false;
    char m_aNewItemName[128] = "";
};

struct SLocalEntry
{
    std::string m_FileName;
    bool m_IsDirectory = false;
    bool m_IsDirectoryEmpty = false;
    bool m_IsImage = false;
    bool m_IsModel = false;
    bool m_IsMaterial = false;
    bool m_IsTextureMeta = false;
    bool m_IsPrefab = false;
    Texture m_PreviewTexture = {};
    std::string m_Extension;
};

struct SAssetBrowserState
{
    SAssetBrowserMarquee m_Marquee;
    SAssetBrowserDrag m_Drag;

    char m_aSearchBuffer[128] = "";
    std::string m_DependencyCachePath;
    std::vector<std::string> m_vCachedFileDependencies;
    bool m_DependencyCacheReady = false;
    int m_RenameTarget = -1;
    std::string m_LastAppliedRename;

    bool m_DuplicatePopupOpen = false;
    std::string m_DuplicateName;

    std::filesystem::path m_CachedPath;
    std::string m_CachedSearch;
    int m_CachedFilter = -1;
    bool m_AssetListCacheDirty = true;
    std::vector<SLocalEntry> m_vCachedEntries;

    SAssetBrowserCreatePopup m_Create;
};

struct SMaterialPickerState
{
    std::vector<std::string> m_vMaterialPaths;
    std::vector<std::string> m_vDisplayNames;
    std::vector<const char*> m_vMaterialNames;
    int m_SelectedIndex = -1;
    bool m_NeedsUpdate = true;
};

struct SFontPickerState
{
    std::vector<std::pair<std::string, std::string>> m_vFonts;
    std::vector<const char*> m_vFontNames;
    int m_SelectedIndex = -1;
    bool m_Scanned = false;
};

struct SEntityRenameState
{
    int m_EntityId = -1;
    bool m_StateSaved = false;
    std::string m_LastName;
};

struct SInterfaceStyleState
{
    ImGuiStyle m_BaseStyle = {};
    float m_BaseScale = 1.0f;
    bool m_Initialized = false;
};

struct SPreviewOrbit
{
    Vec3 m_Target = { 0.0f, 0.0f, 0.0f };
    Vec3 m_ModelRotation = { 0.0f, 0.0f, 0.0f };
    float m_Phi = 20.0f;
    float m_Theta = 45.0f;
    float m_Radius = 5.0f;
};

class CModelViewerState
{
public:
    CModelViewerState() = default;
    CModelViewerState(const CModelViewerState&) = delete;
    CModelViewerState& operator=(const CModelViewerState&) = delete;
    ~CModelViewerState() = default;

    bool m_Visible = false;
    Model m_PreviewModel;
    std::string m_AssetName;
    RenderTexture2D m_RenderTexture = { 0 };
    Vec3 m_ModelCenter = { 0.0f, 0.0f, 0.0f };
    SPreviewOrbit m_Orbit;

    void ReleasePreviewModel();
    void Unload();
};

class CMaterialViewerState
{
public:
    CMaterialViewerState() = default;
    CMaterialViewerState(const CMaterialViewerState&) = delete;
    CMaterialViewerState& operator=(const CMaterialViewerState&) = delete;
    ~CMaterialViewerState() = default;

    bool m_Visible = false;

    int m_PreviewPrimitive = 0;
    Color m_Albedo = WHITE;
    float m_aAlbedo[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float m_Brightness = 1.0f;

    Texture2D m_DiffuseTexture = { 0 };
    std::string m_DiffuseTextureName;
    std::filesystem::path m_CurrentPath;

    Model m_PreviewSphere;
    RenderTexture2D m_RenderTexture = { 0 };

    bool m_TexturePickerVisible = false;
    std::vector<std::string> m_vTextureFilesInDir;
    std::string m_SelectedTextureName;

    bool m_TextureStretch = true;
    float m_TextureRepeatU = 1.0f;
    float m_TextureRepeatV = 1.0f;
    float m_UvScaleX = 1.0f;
    float m_UvScaleY = 1.0f;
    Color m_OutlineColor = LIGHTGRAY;
    float m_aOutlineColor[4] = { 0.827f, 0.827f, 0.827f, 1.0f };

    SPreviewOrbit m_Orbit;

    void ReleasePreviewModel();
    void Unload();
};

class CEditorUiState
{
public:
    CEditorUiState() = default;
    ~CEditorUiState() = default;

    CEditorUiState(const CEditorUiState&) = delete;
    CEditorUiState& operator=(const CEditorUiState&) = delete;

    void Unload();

    SLayoutState m_Layout;
    SMeshEditState m_MeshEdit;
    SVertexEditState m_VertexEdit;
    EPolygonEditMode m_PolygonEditMode = POLY_NONE;
    SAssetBrowserState m_AssetBrowser;
    SMaterialPickerState m_MaterialPicker;
    SFontPickerState m_FontPicker;
    SInspectorUiState m_Inspector;
    SEntityRenameState m_EntityRename;
    SPreferencesUiState m_Preferences;
    SModalState m_Modal;
    SInterfaceStyleState m_InterfaceStyle;
    SGizmoState m_Gizmo;

    CViewportState m_Viewport;
    CViewportState m_GameViewport;
    CModelViewerState m_ModelViewer;
    CMaterialViewerState m_MaterialViewer;
};

#endif // __EDITOR_EDITOR_STATE_H__
