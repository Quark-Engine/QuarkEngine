#include "editor/editor_scene_file.h"

#include "editor/editor.h"
#include "editor/editor_desktop.h"
#include "application_plugin_bridge.h"
#include "project.h"

#include <filesystem>
#include <string>

namespace
{

constexpr const char* kSceneExtension = ".scene";

} // anonymous

std::string CSceneFileService::BrowseSceneSavePath(const std::string& defaultName)
{
    return CDesktopIntegration::SaveFileDialog("Quark Scene",
        "Quark Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0", "scene", defaultName);
}

void CSceneFileService::SaveAs(CEditor& editor)
{
    const std::string defaultName =
        std::filesystem::path(editor.m_ProjectPath).filename().string() + kSceneExtension;
    const std::string selectedFile = BrowseSceneSavePath(defaultName);
    if (selectedFile.empty())
    {
        return;
    }

    std::filesystem::path savePath(selectedFile);
    if (savePath.extension() != kSceneExtension)
    {
        savePath += kSceneExtension;
    }

    const std::filesystem::path rootPath = savePath.parent_path();
    std::error_code error;
    std::filesystem::create_directories(rootPath, error);

    CProjectService::SaveScene(savePath.string(), editor.m_Scene);
    DispatchPluginEvent(PLUGIN_EVENT_SCENE_SAVED);
    editor.m_ProjectPath = rootPath.string();
    editor.m_CurrentAssetPath = rootPath / "resources";
    std::filesystem::create_directories(editor.m_CurrentAssetPath, error);
    editor.m_SceneDirty = false;
}