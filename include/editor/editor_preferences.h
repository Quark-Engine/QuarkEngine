#ifndef __EDITOR_PREFERENCES_H__
#define __EDITOR_PREFERENCES_H__

#include <string>
#include <vector>

class CPreferences
{
public:
    void Load();

    void Save() const;

    bool m_WireframeEnabled = false;
    bool m_ShowGrid = true;
    bool m_ShowAxes = false;
    bool m_ShowColliders = false;
    bool m_LimitFps = true;
    int m_TargetFps = 0;
    float m_CameraSpeed = 2.0f;
    float m_CameraSensitivity = 0.003f;
    float m_CameraZoomSensitivity = 1.0f;
    float m_CameraFov = 45.0f;
    int m_BackgroundRed = 36;
    int m_BackgroundGreen = 38;
    int m_BackgroundBlue = 42;
    bool m_AutosaveEnabled = false;
    bool m_AutosaveBackupEnabled = true;
    int m_AutosaveIntervalMinutes = 5;
    bool m_GizmoSnapEnabled = false;
    float m_GizmoTranslationSnap = 0.5f;
    float m_GizmoRotationSnap = 15.0f;
    float m_GizmoScaleSnap = 0.1f;
    bool m_ShowBoundingBoxes = false;
    bool m_ShowSelectionVisualization = true;
    int m_SelectionRed = 80;
    int m_SelectionGreen = 140;
    int m_SelectionBlue = 255;
    int m_WireframeRed = 80;
    int m_WireframeGreen = 80;
    int m_WireframeBlue = 80;
    int m_BoundsRed = 255;
    int m_BoundsGreen = 220;
    int m_BoundsBlue = 40;
    bool m_ConfirmDelete = true;
    bool m_FocusOnSelection = false;
    bool m_ShadowsEnabled = true;
    int m_ShadowMapSize = 1024;
    float m_ShadowBias = 0.004f;
    int m_ShadowFilterQuality = 1;
    int m_UndoHistoryLimit = 100;
    bool m_VsyncEnabled = true;
    float m_InterfaceScale = 1.0f;
    bool m_LightTheme = false;
    std::string m_ThemeName = "quark-dark";
    bool m_ShowLightHelpers = false;
    bool m_ShowCameras = false;
    int m_RendererBackend = 1;
    int m_MsaaSamples = 1;
    int m_TextureFilter = 1;
    bool m_ConfirmExit = true;
    bool m_OpenLastProject = false;
    std::string m_LastProjectPath;
    bool m_ShowHierarchy = true;
    bool m_ShowInspector = true;
    bool m_ShowAssets = true;
    bool m_ShowScene = true;
    int m_AssetPreviewSize = 64;
    int m_AssetFilter = 0;
    std::vector<std::string> m_vDisabledPlugins;
};

#endif // __EDITOR_PREFERENCES_H__
