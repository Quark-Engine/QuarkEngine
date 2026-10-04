#include "editor/editor_layout.h"

#include "editor/editor.h"
#include "editor/editor_state.h"
#include "editor/editor_preferences.h"
#include "language_manager.h"
#include "imgui_internal.h"

#include <filesystem>
#include <fstream>
#include <string>

#define lang CLanguageManager::Get()

void CEditorLayout::Reset(ImGuiID dockspaceId, CPreferences& preferences)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags(ImGuiDockNodeFlags_PassthruCentralNode) | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

    ImGuiID dockMainId = dockspaceId;
    ImGuiID dockIdLeft = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Left, 0.20f, nullptr, &dockMainId);
    ImGuiID dockIdRight = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Right, 0.25f, nullptr, &dockMainId);
    ImGuiID dockIdBottom = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Down, 0.30f, nullptr, &dockMainId);

    ImGui::DockBuilderDockWindow(lang.Word("hierarchy"), dockIdLeft);
    ImGui::DockBuilderDockWindow(lang.Word("inspector"), dockIdRight);
    ImGui::DockBuilderDockWindow(lang.Word("assets"), dockIdBottom);
    ImGui::DockBuilderDockWindow(lang.Word("scene"), dockMainId);

    ImGui::DockBuilderFinish(dockspaceId);

    preferences.m_ShowHierarchy = true;
    preferences.m_ShowInspector = true;
    preferences.m_ShowAssets = true;
    preferences.m_ShowScene = true;
}

void CEditorLayout::EnsureInitialized(CEditor& editor, ImGuiID dockspaceId)
{
    const char* pIniFilename = ImGui::GetIO().IniFilename;
    const std::filesystem::path iniPath = pIniFilename ? pIniFilename : "imgui.ini";
    const std::filesystem::path layoutMarker = iniPath.parent_path() / ".quark_layout_initialized";
    if (!editor.m_Ui.m_Layout.LayoutMarkerChecked && !std::filesystem::exists(layoutMarker))
    {
        Reset(dockspaceId, editor.m_Preferences);
        std::ofstream(layoutMarker).close();
    }
    editor.m_Ui.m_Layout.LayoutMarkerChecked = true;

    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
    {
        Reset(dockspaceId, editor.m_Preferences);
    }
}