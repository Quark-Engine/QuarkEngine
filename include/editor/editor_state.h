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

    bool WasPressed = false;
    float HoldStart = 0.0f;
    float LastFire = 0.0f;

    bool ShouldFire(float now)
    {
        if (!WasPressed)
        {
            WasPressed = true;
            HoldStart = now;
            LastFire = now;
            return true;
        }

        if (now - HoldStart > HOLD_DELAY && now - LastFire > REPEAT_INTERVAL)
        {
            LastFire = now;
            return true;
        }

        return false;
    }

    void Release()
    {
        WasPressed = false;
        HoldStart = 0.0f;
    }
};

struct SMeshEditState
{
    ImGuizmo::OPERATION GizmoMode = ImGuizmo::TRANSLATE;
    int RenamingIndex = -1;
    char aRenameBuffer[128] = "";
    bool HasClipboard = false;
    CEntity ClipboardData;

    SKeyRepeat Undo;
    SKeyRepeat Redo;
    SKeyRepeat Copy;
    SKeyRepeat Paste;
    SKeyRepeat Duplicate;
};

enum EPolygonEditMode
{
    POLY_NONE,
    POLY_CREATE,
    POLY_MOVE
};

struct SVertexEditState
{
    bool Enabled = false;
    int EntityIndex = -1;
    int MeshIndex = 0;
    int TriangleIndex = 0;
    int VertexCorner = 0;

    std::vector<int> vSelectedVertices;
};

class CViewportState
{
public:
    CViewportState() = default;
    CViewportState(const CViewportState&) = delete;
    CViewportState& operator=(const CViewportState&) = delete;
    ~CViewportState() = default;

    qc::RenderTexture2D m_RenderTexture = { 0 };
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
    CEditor* pEditor = nullptr;
    CEntity* pEntity = nullptr;
};

struct SDeferredHierarchyDelete
{
    CEditor* pEditor = nullptr;
    int Index = -1;
};

struct SModalState
{
    bool ShowExitConfirmation = false;
    bool ShowPreferences = false;
    SPendingEntityDelete PendingDelete;
    SDeferredHierarchyDelete DeferredHierarchyDelete;
};

struct SGizmoState
{
    bool WasUsingTransform = false;
    bool WasUsingMeshEdit = false;
    bool WasUsingPolygon = false;
};

struct SInspectorUiState
{
    char aTagBuffer[128] = {};

    int ComponentToRemove = -1;
    int SelectedTextureIndex = 0;
    int SelectedNormalIndex = 0;
};

struct SPreferencesUiState
{
    int LanguageIndex = -1;
};

struct SLayoutState
{
    bool ShowAboutWindow = false;

    bool SceneAssetDragging = false;
    std::string DraggedSceneAssetName;

    bool FileDragging = false;
    int DraggedFileIndex = -1;
    int DraggedTargetFolderIndex = -1;

    ImGuiDockNodeFlags DockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    bool LayoutMarkerChecked = false;
};

struct SAssetBrowserMarquee
{
    ImVec2 Start = {};
    ImVec2 End = {};
    bool Active = false;
};

struct SAssetBrowserDrag
{
    std::filesystem::path FilePath;
    std::string FileName;
    ImVec2 StartPos = {};
};

struct SAssetBrowserCreatePopup
{
    bool Open = false;
    bool CreatingFolder = false;
    char aNewItemName[128] = "";
};

struct SAssetBrowserState
{
    SAssetBrowserMarquee Marquee;
    SAssetBrowserDrag Drag;

    int RenameTarget = -1;
    std::string LastAppliedRename;

    bool DuplicatePopupOpen = false;
    std::string DuplicateName;

    SAssetBrowserCreatePopup Create;
};

struct SMaterialPickerState
{
    std::vector<std::string> vMaterialPaths;
    std::vector<std::string> vDisplayNames;
    std::vector<const char*> vMaterialNames;
    int SelectedIndex = -1;
    bool NeedsUpdate = true;
};

struct SFontPickerState
{
    std::vector<std::pair<std::string, std::string>> vFonts;
    std::vector<const char*> vFontNames;
    int SelectedIndex = -1;
    bool Scanned = false;
};

struct SEntityRenameState
{
    int EntityId = -1;
    bool StateSaved = false;
    std::string LastName;
};

struct SInterfaceStyleState
{
    ImGuiStyle BaseStyle = {};
    float BaseScale = 1.0f;
    bool Initialized = false;
};

struct SPreviewOrbit
{
    qc::Vec3 Target = { 0.0f, 0.0f, 0.0f };
    qc::Vec3 ModelRotation = { 0.0f, 0.0f, 0.0f };
    float Phi = 20.0f;
    float Theta = 45.0f;
    float Radius = 5.0f;
};

class CModelViewerState
{
public:
    CModelViewerState() = default;
    CModelViewerState(const CModelViewerState&) = delete;
    CModelViewerState& operator=(const CModelViewerState&) = delete;
    ~CModelViewerState() = default;

    bool m_Visible = false;
    qc::Model m_PreviewModel;
    qc::RenderTexture2D m_RenderTexture = { 0 };
    qc::Vec3 m_ModelCenter = { 0.0f, 0.0f, 0.0f };
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
    qc::Color m_Albedo = qc::WHITE;
    float m_aAlbedo[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float m_Brightness = 1.0f;

    qc::Texture2D m_DiffuseTexture = { 0 };
    std::string m_DiffuseTextureName;
    std::filesystem::path m_CurrentPath;

    qc::Model m_PreviewSphere;
    qc::RenderTexture2D m_RenderTexture = { 0 };

    bool m_TexturePickerVisible = false;
    std::vector<std::string> m_vTextureFilesInDir;
    std::string m_SelectedTextureName;

    bool m_TextureStretch = true;
    float m_TextureRepeatU = 1.0f;
    float m_TextureRepeatV = 1.0f;
    float m_UvScaleX = 1.0f;
    float m_UvScaleY = 1.0f;
    qc::Color m_OutlineColor = qc::LIGHTGRAY;
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
    CModelViewerState m_ModelViewer;
    CMaterialViewerState m_MaterialViewer;
};

#endif // __EDITOR_EDITOR_STATE_H__
