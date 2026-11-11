#ifndef __HUB_H__
#define __HUB_H__
#include "QuarkCore/QuarkCore.hpp"
#include "editor/editor_preferences.h"
#include "qcImGui.h"

#include "imgui.h"

#include <filesystem>
#include <string>
#include <vector>

struct SHubProject
{
    std::string m_Name;
    std::string m_Path;
};

struct SHubPlugin
{
    std::string m_Name;
    std::string m_Path;
    std::string m_Description;
    bool m_Enabled = false;
    Texture2D m_Icon = { 0 };
};

struct SHubState
{
    std::vector<SHubProject> m_vProjects;
    int m_SelectedProject = -1;

    bool m_ShowCreate = false;
    char m_aCreateName[256] = "";
    char m_aCreatePath[512] = "";

    bool m_ShowRename = false;
    int m_RenameProjectIndex = -1;
    char m_aRenameBuffer[256] = "";

    bool m_ShowDelete = false;

    bool m_ShowVersionWarning = false;
    std::string m_PendingOpenPath;
    std::string m_SavedVersion;

    bool m_ShowPluginManager = false;
    std::vector<SHubPlugin> m_vPlugins;
    int m_SelectedPlugin = -1;
};

class CHubApp
{
public:
    CHubApp() = default;
    ~CHubApp();

    CHubApp(const CHubApp&) = delete;
    CHubApp& operator=(const CHubApp&) = delete;

    std::string Run(CPreferences& preferences);

    const SHubState& State() const
    {
        return m_State;
    }

private:
    static const char* ProjectsRoot();
    static const char* RegistryFile();

    static ImVec4 PluginBadgeColor(const std::string& name);

    static std::filesystem::path PluginDisabledSentinel(const std::string& pluginPath);
    static bool PluginIsEnabled(const std::string& pluginPath);
    static void PluginSetEnabled(const std::string& pluginPath, bool enabled);
    static std::string PluginReadMetaDescription(const std::filesystem::path& pluginPath);
    static std::string BrowseFolder();
    static std::string BrowseProjectFile();

    void SaveRegistry();
    void Refresh();
    void RefreshPlugins();

    void CreateProject(const std::string& name, const std::string& base);
    void DeleteProject(const std::string& path);
    void RenameProject(const std::string& oldPath, const std::string& newName);
    void ImportProject(const std::string& manifestOrPath);

    void DrawProjectCard(int index);
    void DrawProjectList();
    void DrawHeader();
    void DrawCreatePopup();
    void DrawRenamePopup();
    void DrawDeletePopup();
    void DrawVersionWarningPopup();
    void DrawPluginManager();

    SHubState m_State;

    std::string m_PendingResult;
    bool m_ShouldExit = false;

    CPreferences* m_pPreferences = nullptr;
};

#endif // __HUB_H__
