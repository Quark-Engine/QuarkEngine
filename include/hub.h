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
    std::string Name;
    std::string Path;
};

struct SHubPlugin
{
    std::string Name;
    std::string Path;
    std::string Description;
    bool Enabled = false;
    qc::Texture2D Icon = { 0 };
};

struct SHubState
{
    std::vector<SHubProject> vProjects;
    int SelectedProject = -1;

    bool ShowCreate = false;
    char aCreateName[256] = "";
    char aCreatePath[512] = "";

    bool ShowRename = false;
    int RenameProjectIndex = -1;
    char aRenameBuffer[256] = "";

    bool ShowDelete = false;

    bool ShowVersionWarning = false;
    std::string PendingOpenPath;
    std::string SavedVersion;

    bool ShowPluginManager = false;
    std::vector<SHubPlugin> vPlugins;
    int SelectedPlugin = -1;
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

    ImU32 CardColor(bool selected) const;
    ImU32 CardBorderColor(bool selected) const;
    ImU32 CardHoverColor() const;

    bool UsesLightTheme() const;
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
