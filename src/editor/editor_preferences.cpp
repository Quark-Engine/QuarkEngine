#include "editor/editor_preferences.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

static void ReadPreferences(CPreferences& preferences, const json& data)
{
    const json* pPreferences = &data;
    if (data.contains("editor_preferences") && data["editor_preferences"].is_object())
        pPreferences = &data["editor_preferences"];

    if (pPreferences->contains("wireframe_enabled")) preferences.m_WireframeEnabled = (*pPreferences)["wireframe_enabled"].get<bool>();
    if (pPreferences->contains("show_grid")) preferences.m_ShowGrid = (*pPreferences)["show_grid"].get<bool>();
    if (pPreferences->contains("show_axes")) preferences.m_ShowAxes = (*pPreferences)["show_axes"].get<bool>();
    if (pPreferences->contains("show_colliders")) preferences.m_ShowColliders = (*pPreferences)["show_colliders"].get<bool>();
    if (pPreferences->contains("limit_fps")) preferences.m_LimitFps = (*pPreferences)["limit_fps"].get<bool>();
    if (pPreferences->contains("target_fps")) preferences.m_TargetFps = (*pPreferences)["target_fps"].get<int>();
    if (pPreferences->contains("camera_speed")) preferences.m_CameraSpeed = (*pPreferences)["camera_speed"].get<float>();
    if (pPreferences->contains("camera_sensitivity")) preferences.m_CameraSensitivity = (*pPreferences)["camera_sensitivity"].get<float>();
    if (pPreferences->contains("camera_zoom_sensitivity")) preferences.m_CameraZoomSensitivity = (*pPreferences)["camera_zoom_sensitivity"].get<float>();
    if (pPreferences->contains("camera_fov")) preferences.m_CameraFov = (*pPreferences)["camera_fov"].get<float>();
    if (pPreferences->contains("background_red")) preferences.m_BackgroundRed = (*pPreferences)["background_red"].get<int>();
    if (pPreferences->contains("background_green")) preferences.m_BackgroundGreen = (*pPreferences)["background_green"].get<int>();
    if (pPreferences->contains("background_blue")) preferences.m_BackgroundBlue = (*pPreferences)["background_blue"].get<int>();
    if (pPreferences->contains("autosave_enabled")) preferences.m_AutosaveEnabled = (*pPreferences)["autosave_enabled"].get<bool>();
    if (pPreferences->contains("autosave_backup_enabled")) preferences.m_AutosaveBackupEnabled = (*pPreferences)["autosave_backup_enabled"].get<bool>();
    if (pPreferences->contains("autosave_interval_minutes")) preferences.m_AutosaveIntervalMinutes = (*pPreferences)["autosave_interval_minutes"].get<int>();
    if (pPreferences->contains("gizmo_snap_enabled")) preferences.m_GizmoSnapEnabled = (*pPreferences)["gizmo_snap_enabled"].get<bool>();
    if (pPreferences->contains("gizmo_translation_snap")) preferences.m_GizmoTranslationSnap = (*pPreferences)["gizmo_translation_snap"].get<float>();
    if (pPreferences->contains("gizmo_rotation_snap")) preferences.m_GizmoRotationSnap = (*pPreferences)["gizmo_rotation_snap"].get<float>();
    if (pPreferences->contains("gizmo_scale_snap")) preferences.m_GizmoScaleSnap = (*pPreferences)["gizmo_scale_snap"].get<float>();
    if (pPreferences->contains("show_bounding_boxes")) preferences.m_ShowBoundingBoxes = (*pPreferences)["show_bounding_boxes"].get<bool>();
    if (pPreferences->contains("show_selection_visualization")) preferences.m_ShowSelectionVisualization = (*pPreferences)["show_selection_visualization"].get<bool>();
    if (pPreferences->contains("selection_red")) preferences.m_SelectionRed = (*pPreferences)["selection_red"].get<int>();
    if (pPreferences->contains("selection_green")) preferences.m_SelectionGreen = (*pPreferences)["selection_green"].get<int>();
    if (pPreferences->contains("selection_blue")) preferences.m_SelectionBlue = (*pPreferences)["selection_blue"].get<int>();
    if (pPreferences->contains("wireframe_red")) preferences.m_WireframeRed = (*pPreferences)["wireframe_red"].get<int>();
    if (pPreferences->contains("wireframe_green")) preferences.m_WireframeGreen = (*pPreferences)["wireframe_green"].get<int>();
    if (pPreferences->contains("wireframe_blue")) preferences.m_WireframeBlue = (*pPreferences)["wireframe_blue"].get<int>();
    if (pPreferences->contains("bounds_red")) preferences.m_BoundsRed = (*pPreferences)["bounds_red"].get<int>();
    if (pPreferences->contains("bounds_green")) preferences.m_BoundsGreen = (*pPreferences)["bounds_green"].get<int>();
    if (pPreferences->contains("bounds_blue")) preferences.m_BoundsBlue = (*pPreferences)["bounds_blue"].get<int>();
    if (pPreferences->contains("confirm_delete")) preferences.m_ConfirmDelete = (*pPreferences)["confirm_delete"].get<bool>();
    if (pPreferences->contains("focus_on_selection")) preferences.m_FocusOnSelection = (*pPreferences)["focus_on_selection"].get<bool>();
    if (pPreferences->contains("shadows_enabled")) preferences.m_ShadowsEnabled = (*pPreferences)["shadows_enabled"].get<bool>();
    if (pPreferences->contains("shadow_map_size")) preferences.m_ShadowMapSize = (*pPreferences)["shadow_map_size"].get<int>();
    if (pPreferences->contains("shadow_bias")) preferences.m_ShadowBias = (*pPreferences)["shadow_bias"].get<float>();
    if (pPreferences->contains("shadow_filter_quality")) preferences.m_ShadowFilterQuality = (*pPreferences)["shadow_filter_quality"].get<int>();
    if (pPreferences->contains("undo_history_limit")) preferences.m_UndoHistoryLimit = (*pPreferences)["undo_history_limit"].get<int>();
    if (pPreferences->contains("vsync_enabled")) preferences.m_VsyncEnabled = (*pPreferences)["vsync_enabled"].get<bool>();
    if (pPreferences->contains("interface_scale")) preferences.m_InterfaceScale = (*pPreferences)["interface_scale"].get<float>();
    if (pPreferences->contains("light_theme")) preferences.m_LightTheme = (*pPreferences)["light_theme"].get<bool>();
    if (pPreferences->contains("show_light_helpers")) preferences.m_ShowLightHelpers = (*pPreferences)["show_light_helpers"].get<bool>();
    if (pPreferences->contains("show_cameras")) preferences.m_ShowCameras = (*pPreferences)["show_cameras"].get<bool>();
    if (pPreferences->contains("renderer_backend")) preferences.m_RendererBackend = (*pPreferences)["renderer_backend"].get<int>();
    if (pPreferences->contains("msaa_samples")) preferences.m_MsaaSamples = (*pPreferences)["msaa_samples"].get<int>();
    if (pPreferences->contains("texture_filter")) preferences.m_TextureFilter = (*pPreferences)["texture_filter"].get<int>();
    if (pPreferences->contains("confirm_exit")) preferences.m_ConfirmExit = (*pPreferences)["confirm_exit"].get<bool>();
    if (pPreferences->contains("open_last_project")) preferences.m_OpenLastProject = (*pPreferences)["open_last_project"].get<bool>();
    if (pPreferences->contains("last_project_path")) preferences.m_LastProjectPath = (*pPreferences)["last_project_path"].get<std::string>();
    if (pPreferences->contains("show_hierarchy")) preferences.m_ShowHierarchy = (*pPreferences)["show_hierarchy"].get<bool>();
    if (pPreferences->contains("show_inspector")) preferences.m_ShowInspector = (*pPreferences)["show_inspector"].get<bool>();
    if (pPreferences->contains("show_assets")) preferences.m_ShowAssets = (*pPreferences)["show_assets"].get<bool>();
    if (pPreferences->contains("show_scene")) preferences.m_ShowScene = (*pPreferences)["show_scene"].get<bool>();
    if (pPreferences->contains("asset_preview_size")) preferences.m_AssetPreviewSize = (*pPreferences)["asset_preview_size"].get<int>();
    if (pPreferences->contains("asset_filter")) preferences.m_AssetFilter = (*pPreferences)["asset_filter"].get<int>();
    if (pPreferences->contains("disabled_plugins") && (*pPreferences)["disabled_plugins"].is_array())
    {
        preferences.m_vDisabledPlugins = (*pPreferences)["disabled_plugins"].get<std::vector<std::string>>();
    }

    if (preferences.m_TargetFps > 0)
        preferences.m_TargetFps = std::clamp(preferences.m_TargetFps, 30, 240);
    preferences.m_CameraSpeed = std::clamp(preferences.m_CameraSpeed, 0.1f, 20.0f);
    preferences.m_CameraSensitivity = std::clamp(preferences.m_CameraSensitivity, 0.0005f, 0.02f);
    preferences.m_CameraZoomSensitivity = std::clamp(preferences.m_CameraZoomSensitivity, 0.1f, 5.0f);
    preferences.m_CameraFov = std::clamp(preferences.m_CameraFov, 20.0f, 120.0f);
    if (preferences.m_RendererBackend < 0 || preferences.m_RendererBackend > 3) preferences.m_RendererBackend = 0;
    if (preferences.m_MsaaSamples != 1 && preferences.m_MsaaSamples != 2 && preferences.m_MsaaSamples != 4 && preferences.m_MsaaSamples != 8) preferences.m_MsaaSamples = 1;
    if (preferences.m_TextureFilter < 0 || preferences.m_TextureFilter > 1) preferences.m_TextureFilter = 1;
    preferences.m_InterfaceScale = std::clamp(preferences.m_InterfaceScale, 0.75f, 2.0f);
    preferences.m_BackgroundRed = std::clamp(preferences.m_BackgroundRed, 0, 255);
    preferences.m_BackgroundGreen = std::clamp(preferences.m_BackgroundGreen, 0, 255);
    preferences.m_BackgroundBlue = std::clamp(preferences.m_BackgroundBlue, 0, 255);
    preferences.m_ShadowBias = std::clamp(preferences.m_ShadowBias, 0.0001f, 0.05f);
    preferences.m_ShadowFilterQuality = std::clamp(preferences.m_ShadowFilterQuality, 0, 2);
    preferences.m_UndoHistoryLimit = std::clamp(preferences.m_UndoHistoryLimit, 10, 500);
    preferences.m_AssetPreviewSize = std::clamp(preferences.m_AssetPreviewSize, 32, 128);
    preferences.m_AssetFilter = std::clamp(preferences.m_AssetFilter, 0, 4);
    preferences.m_AutosaveIntervalMinutes = std::clamp(preferences.m_AutosaveIntervalMinutes, 1, 60);
    preferences.m_GizmoTranslationSnap = std::clamp(preferences.m_GizmoTranslationSnap, 0.01f, 10.0f);
    preferences.m_GizmoRotationSnap = std::clamp(preferences.m_GizmoRotationSnap, 1.0f, 90.0f);
    preferences.m_GizmoScaleSnap = std::clamp(preferences.m_GizmoScaleSnap, 0.01f, 1.0f);
    if (preferences.m_ShadowMapSize != 512 &&
        preferences.m_ShadowMapSize != 1024 &&
        preferences.m_ShadowMapSize != 2048)
        preferences.m_ShadowMapSize = 1024;
}

void CPreferences::Save() const
{
    try
    {
        json preferences = {
        {"wireframe_enabled", m_WireframeEnabled},
        {"show_grid", m_ShowGrid},
        {"show_axes", m_ShowAxes},
        {"show_colliders", m_ShowColliders},
        {"limit_fps", m_LimitFps},
        {"target_fps", m_TargetFps},
        {"camera_speed", m_CameraSpeed},
        {"camera_sensitivity", m_CameraSensitivity},
        {"camera_zoom_sensitivity", m_CameraZoomSensitivity},
        {"camera_fov", m_CameraFov},
        {"background_red", m_BackgroundRed},
        {"background_green", m_BackgroundGreen},
        {"background_blue", m_BackgroundBlue},
        {"autosave_enabled", m_AutosaveEnabled},
        {"autosave_backup_enabled", m_AutosaveBackupEnabled},
        {"autosave_interval_minutes", m_AutosaveIntervalMinutes},
        {"gizmo_snap_enabled", m_GizmoSnapEnabled},
        {"gizmo_translation_snap", m_GizmoTranslationSnap},
        {"gizmo_rotation_snap", m_GizmoRotationSnap},
        {"gizmo_scale_snap", m_GizmoScaleSnap},
        {"show_bounding_boxes", m_ShowBoundingBoxes},
        {"show_selection_visualization", m_ShowSelectionVisualization},
        {"selection_red", m_SelectionRed},
        {"selection_green", m_SelectionGreen},
        {"selection_blue", m_SelectionBlue},
        {"wireframe_red", m_WireframeRed},
        {"wireframe_green", m_WireframeGreen},
        {"wireframe_blue", m_WireframeBlue},
        {"bounds_red", m_BoundsRed},
        {"bounds_green", m_BoundsGreen},
        {"bounds_blue", m_BoundsBlue},
        {"confirm_delete", m_ConfirmDelete},
        {"focus_on_selection", m_FocusOnSelection},
        {"shadows_enabled", m_ShadowsEnabled},
        {"shadow_map_size", m_ShadowMapSize},
        {"shadow_bias", m_ShadowBias},
        {"shadow_filter_quality", m_ShadowFilterQuality},
        {"undo_history_limit", m_UndoHistoryLimit},
        {"vsync_enabled", m_VsyncEnabled},
        {"interface_scale", m_InterfaceScale},
        {"light_theme", m_LightTheme},
        {"show_light_helpers", m_ShowLightHelpers},
        {"show_cameras", m_ShowCameras},
        {"renderer_backend", m_RendererBackend},
        {"msaa_samples", m_MsaaSamples},
        {"texture_filter", m_TextureFilter},
        {"confirm_exit", m_ConfirmExit},
        {"open_last_project", m_OpenLastProject},
        {"last_project_path", m_LastProjectPath},
        {"show_hierarchy", m_ShowHierarchy},
        {"show_inspector", m_ShowInspector},
        {"show_assets", m_ShowAssets},
        {"show_scene", m_ShowScene},
        {"asset_preview_size", m_AssetPreviewSize},
        {"asset_filter", m_AssetFilter}
        };

        preferences["disabled_plugins"] = json::array();
        for (const std::string& pluginPath : m_vDisabledPlugins)
        {
            preferences["disabled_plugins"].push_back(pluginPath);
        }

        json config;
        std::ifstream in("config.json");
        if (in.is_open())
        {
            try
            {
                in >> config;
            }
            catch (...)
            {
                config = json::object();
            }
        }
        config["editor_preferences"] = preferences;

        std::ofstream out("config.json");
        if (out.is_open())
        {
            out << config.dump(4);
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Failed to save config.json: " << exception.what() << '\n';
    }
}

void CPreferences::Load()
{
    std::ifstream configFile("config.json");
    if (configFile.is_open())
    {
        try
        {
            json config;
            configFile >> config;
            if (config.contains("editor_preferences"))
            {
                ReadPreferences(*this, config);
                std::filesystem::remove("editor_preferences.json");
                return;
            }
        }
        catch (...)
        {
        }
    }

    std::ifstream legacyFile("editor_preferences.json");
    if (legacyFile.is_open())
    {
        try
        {
            json legacyPreferences;
            legacyFile >> legacyPreferences;
            ReadPreferences(*this, legacyPreferences);
            Save();
            std::filesystem::remove("editor_preferences.json");
            return;
        }
        catch (...)
        {
        }
    }

    Save();
}
