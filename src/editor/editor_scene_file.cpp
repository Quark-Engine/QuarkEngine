#include "editor/editor_scene_file.h"

#include "editor/editor.h"
#include "editor/editor_desktop.h"
#include "project.h"

#include <filesystem>
#include <string>

namespace
{

constexpr const char* SCENE_EXTENSION = ".json";

} // anonymous

std::string CSceneFileService::BrowseSceneSavePath(const std::string& defaultName)
{
    return CDesktopIntegration::SaveFileDialog("Scene JSON",
        "Scene JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0", "json", defaultName);
}

void CSceneFileService::SaveAs(CEditor& editor)
{
    const std::string selectedFile = BrowseSceneSavePath("scene.json");
    if (selectedFile.empty())
    {
        return;
    }

    std::filesystem::path savePath(selectedFile);
    if (savePath.extension() != SCENE_EXTENSION)
    {
        savePath.replace_extension(SCENE_EXTENSION);
    }

    if (!CProjectService::SaveSceneDocument(savePath.string(), editor.m_Scene))
    {
        editor.SetStatusMessage("Failed to save scene");
        return;
    }
}
