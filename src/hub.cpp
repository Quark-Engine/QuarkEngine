#define NOMINMAX

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN

    #define CloseWindow WinCloseWindow
    #define ShowCursor WinShowCursor
    #define Rectangle WinRectangle

    #include <windows.h>
    #include <commdlg.h>
    #include <shlobj.h>
    #include <ole2.h>

    #undef CloseWindow
    #undef ShowCursor
    #undef Rectangle
    #undef near
    #undef far
#endif

#include "hub.h"

#include "version.h"
#include "language_manager.h"
#include "editor/editor_preferences.h"
#include "project.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;

using json = nlohmann::json;

#define lang CLanguageManager::Get()

namespace
{

const char* const HUB_PROJECTS_ROOT = "projects";
const char* const HUB_REGISTRY_FILE = "config.json";

bool IsSupportedPluginExtension(const std::string& extension)
{
#ifdef _WIN32
    return extension == ".dll";
#elif __APPLE__
    return extension == ".dylib";
#else
    return extension == ".so";
#endif
}

std::string ReadMetaLine(const fs::path& metaPath, const char* pKey)
{
    const std::string prefix = std::string(pKey) + "=";
    if (!fs::exists(metaPath))
    {
        return "";
    }

    std::ifstream file(metaPath);
    std::string line;
    while (std::getline(file, line))
    {
        if (line.rfind(prefix, 0) == 0)
        {
            return line.substr(prefix.size());
        }
    }

    return "";
}

Color ToQuarkColor(const ImVec4& color)
{
    return Color{
        static_cast<unsigned char>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f)
    };
}

} // anonymous

CHubApp::~CHubApp()
{
    for (auto& plugin : m_State.vPlugins)
    {
        if (plugin.Icon.id != 0)
        {
            UnloadTexture(plugin.Icon);
        }
    }
}

const char* CHubApp::ProjectsRoot()
{
    return HUB_PROJECTS_ROOT;
}

const char* CHubApp::RegistryFile()
{
    return HUB_REGISTRY_FILE;
}

ImVec4 CHubApp::PluginBadgeColor(const std::string& name)
{
    static const ImVec4 s_aPalette[] = {
        { 0.20f, 0.55f, 0.95f, 1.0f },
        { 0.18f, 0.72f, 0.56f, 1.0f },
        { 0.85f, 0.45f, 0.20f, 1.0f },
        { 0.65f, 0.35f, 0.90f, 1.0f },
        { 0.90f, 0.70f, 0.10f, 1.0f },
        { 0.85f, 0.25f, 0.35f, 1.0f },
    };

    size_t hash = 0;
    for (char c : name)
    {
        hash = hash * 31 + static_cast<unsigned char>(c);
    }
    return s_aPalette[hash % 6];
}

fs::path CHubApp::PluginDisabledSentinel(const std::string& pluginPath)
{
    const fs::path path(pluginPath);
    return path.parent_path() / (path.stem().string() + ".disabled");
}

bool CHubApp::PluginIsEnabled(const std::string& pluginPath)
{
    return !fs::exists(PluginDisabledSentinel(pluginPath));
}

void CHubApp::PluginSetEnabled(const std::string& pluginPath, bool enabled)
{
    const fs::path sentinel = PluginDisabledSentinel(pluginPath);
    if (enabled)
    {
        if (fs::exists(sentinel))
        {
            fs::remove(sentinel);
        }
    }
    else
    {
        std::ofstream file(sentinel);
    }
}

std::string CHubApp::PluginReadMetaDescription(const fs::path& pluginPath)
{
    const fs::path meta = pluginPath.parent_path() / (pluginPath.stem().string() + ".meta");
    return ReadMetaLine(meta, "description");
}

std::string CHubApp::BrowseFolder()
{
#ifdef _WIN32
    char path[MAX_PATH] = {};
    BROWSEINFOA info = {};
    info.lpszTitle = lang.Word("select_project_loc");
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&info);
    if (!pidl)
    {
        return "";
    }

    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    return path;

#elif __linux__
    FILE* pPipe = popen("zenity --file-selection --directory 2>/dev/null", "r");
    if (!pPipe)
    {
        return "";
    }

    char aResult[512] = {};
    if (fgets(aResult, sizeof(aResult), pPipe))
    {
        size_t length = strlen(aResult);
        if (length > 0 && aResult[length - 1] == '\n')
        {
            aResult[length - 1] = '\0';
        }
    }
    pclose(pPipe);
    return aResult;
#else
    return "";
#endif
}

std::string CHubApp::BrowseProjectFile()
{
#ifdef _WIN32
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Quark Project (*.quarkproj)\0*.quarkproj\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "quarkproj";
    if (GetOpenFileNameA(&ofn))
    {
        return path;
    }
    return "";

#elif __linux__
    FILE* pPipe = popen("zenity --file-selection --file-filter='*.quarkproj' 2>/dev/null", "r");
    if (!pPipe)
    {
        return "";
    }

    char aResult[512] = {};
    if (fgets(aResult, sizeof(aResult), pPipe))
    {
        size_t length = strlen(aResult);
        if (length > 0 && aResult[length - 1] == '\n')
        {
            aResult[length - 1] = '\0';
        }
    }
    pclose(pPipe);
    return aResult;
#else
    return "";
#endif
}

void CHubApp::SaveRegistry()
{
    json root;

    if (fs::exists(RegistryFile()))
    {
        std::ifstream file(RegistryFile());
        try
        {
            file >> root;
        }
        catch (...)
        {
        }
    }

    if (!root.contains("language"))
    {
        root["language"] = "english";
    }

    json projects = json::array();
    for (auto& project : m_State.vProjects)
    {
        projects.push_back({ { "name", project.Name }, { "path", project.Path } });
    }
    root["projects"] = projects;

    std::ofstream file(RegistryFile());
    file << root.dump(4);
}

void CHubApp::Refresh()
{
    m_State.vProjects.clear();

    if (fs::exists(RegistryFile()))
    {
        std::ifstream file(RegistryFile());
        json root;
        try
        {
            file >> root;
            json projects = root.contains("projects") ? root["projects"] : json::array();
            if (projects.is_array())
            {
                for (auto& entry : projects)
                {
                    std::string path = entry["path"];
                    if (!CProjectService::IsValid(path))
                    {
                        continue;
                    }

                    SHubProject project;
                    project.Name = entry["name"];
                    project.Path = CProjectService::ResolveRoot(path);
                    m_State.vProjects.push_back(project);
                }
            }
        }
        catch (...)
        {
        }
    }

    std::sort(m_State.vProjects.begin(), m_State.vProjects.end(),
        [](const SHubProject& a, const SHubProject& b)
        {
            return a.Name < b.Name;
        });
}

void CHubApp::RefreshPlugins()
{
    for (auto& plugin : m_State.vPlugins)
    {
        if (plugin.Icon.id != 0)
        {
            UnloadTexture(plugin.Icon);
        }
    }

    m_State.vPlugins.clear();
    m_State.SelectedPlugin = -1;

    const std::string pluginsDir = "plugins";
    if (!fs::exists(pluginsDir))
    {
        return;
    }

    for (auto& entry : fs::directory_iterator(pluginsDir))
    {
        SHubPlugin plugin;

        if (entry.is_directory())
        {
            fs::path binary;
            for (auto& file : fs::directory_iterator(entry.path()))
            {
                if (IsSupportedPluginExtension(file.path().extension().string()))
                {
                    binary = file.path();
                    break;
                }
            }
            if (binary.empty())
            {
                continue;
            }

            plugin.Path = binary.string();
            plugin.Name = entry.path().filename().string();

            const fs::path iconPath = entry.path() / "icon.png";
            const fs::path metaPath = entry.path() / "meta.txt";
            plugin.Icon = fs::exists(iconPath) ? LoadTexture(iconPath.string().c_str()) : Texture2D{ 0 };
            plugin.Description = ReadMetaLine(metaPath, "description");
        }
        else if (entry.is_regular_file())
        {
            if (!IsSupportedPluginExtension(entry.path().extension().string()))
            {
                continue;
            }

            plugin.Path = entry.path().string();
            plugin.Name = entry.path().stem().string();

            const fs::path iconPath = entry.path().parent_path() / (plugin.Name + ".png");
            const fs::path metaPath = entry.path().parent_path() / (plugin.Name + ".meta");
            plugin.Icon = fs::exists(iconPath) ? LoadTexture(iconPath.string().c_str()) : Texture2D{ 0 };
            plugin.Description = ReadMetaLine(metaPath, "description");
        }
        else
        {
            continue;
        }

        plugin.Enabled = PluginIsEnabled(plugin.Path);
        if (plugin.Description.empty())
        {
            plugin.Description = "No description provided.";
        }
        m_State.vPlugins.push_back(plugin);
    }

    std::sort(m_State.vPlugins.begin(), m_State.vPlugins.end(),
        [](const SHubPlugin& a, const SHubPlugin& b)
        {
            return a.Name < b.Name;
        });
}

void CHubApp::CreateProject(const std::string& name, const std::string& base)
{
    const fs::path project = fs::path(base) / name;
    CScene emptyScene;
    CProjectService::CreateNew(project.string(), emptyScene);

    SHubProject entry;
    entry.Name = name;
    entry.Path = fs::absolute(project).string();
    m_State.vProjects.push_back(entry);
    SaveRegistry();
}

void CHubApp::DeleteProject(const std::string& path)
{
    fs::remove_all(path);
    m_State.vProjects.erase(
        std::remove_if(m_State.vProjects.begin(), m_State.vProjects.end(),
            [&](const SHubProject& project)
            {
                return project.Path == path;
            }),
        m_State.vProjects.end());
    SaveRegistry();
}

void CHubApp::RenameProject(const std::string& oldPath, const std::string& newName)
{
    const fs::path oldProject(oldPath);
    const fs::path newPath = oldProject.parent_path() / newName;
    fs::rename(oldProject, newPath);

    for (auto& project : m_State.vProjects)
    {
        if (project.Path != oldPath)
        {
            continue;
        }
        project.Name = newName;
        project.Path = fs::absolute(newPath).string();
        break;
    }
    SaveRegistry();
}

void CHubApp::ImportProject(const std::string& manifestOrPath)
{
    if (!CProjectService::IsValid(manifestOrPath))
    {
        return;
    }

    const std::string rootPath = CProjectService::ResolveRoot(manifestOrPath);
    const fs::path root(rootPath);
    const std::string name = root.filename().string().empty()
        ? root.stem().string()
        : root.filename().string();

    for (auto& existing : m_State.vProjects)
    {
        if (existing.Path == rootPath)
        {
            return;
        }
    }

    SHubProject project;
    project.Name = name;
    project.Path = rootPath;
    m_State.vProjects.push_back(project);
    SaveRegistry();
}

void CHubApp::DrawProjectCard(int index)
{
    ImGui::PushID(index);

    const bool isSelected = m_State.SelectedProject == index;
    const ImVec2 cardPos = ImGui::GetCursorScreenPos();
    const float cardWidth = ImGui::GetContentRegionAvail().x;
    const float cardHeight = 62.0f;
    const ImVec2 cardMax = ImVec2(cardPos.x + cardWidth, cardPos.y + cardHeight);

    ImGui::InvisibleButton("##card", ImVec2(cardWidth, cardHeight));
    const bool isHovered = ImGui::IsItemHovered() && !isSelected;
    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const ImVec4& fill = isSelected ? style.HubCardSelected :
        (isHovered ? style.HubCardHovered : style.HubCard);
    const ImVec4& border = isSelected ? style.HubCardSelectedBorder : style.HubCardBorder;

    pDrawList->AddRectFilled(
        cardPos, cardMax, ImGui::ColorConvertFloat4ToU32(fill), style.HubCardRounding);
    if (style.HubCardBorderSize > 0.0f)
    {
        pDrawList->AddRect(
            cardPos, cardMax, ImGui::ColorConvertFloat4ToU32(border),
            style.HubCardRounding, 0, style.HubCardBorderSize);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_State.SelectedProject = index;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        const std::string savedVersion = CProjectService::GetVersion(m_State.vProjects[index].Path);
        if (!savedVersion.empty() && savedVersion != QUARK_ENGINE_VERSION)
        {
            m_State.PendingOpenPath = m_State.vProjects[index].Path;
            m_State.SavedVersion = savedVersion;
            m_State.ShowVersionWarning = true;
        }
        else
        {
            m_PendingResult = m_State.vProjects[index].Path;
            m_ShouldExit = true;
        }
    }

    if (ImGui::BeginPopupContextItem("##ctx"))
    {
        if (ImGui::MenuItem(lang.Word("open")))
        {
            m_PendingResult = m_State.vProjects[index].Path;
            m_ShouldExit = true;
        }

        ImGui::Separator();
        if (ImGui::MenuItem(lang.Word("rename")))
        {
            m_State.RenameProjectIndex = index;
            snprintf(m_State.aRenameBuffer, sizeof(m_State.aRenameBuffer), "%s",
                m_State.vProjects[index].Name.c_str());
            m_State.ShowRename = true;
        }

        if (ImGui::MenuItem(lang.Word("delete")))
        {
            m_State.SelectedProject = index;
            m_State.ShowDelete = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 14, cardPos.y + 11));
    ImGui::Text("%s", m_State.vProjects[index].Name.c_str());

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 14, cardPos.y + 36));
    ImGui::TextDisabled("%s", m_State.vProjects[index].Path.c_str());

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x, cardPos.y + cardHeight + 4));
    ImGui::Dummy(ImVec2(cardWidth, 0));

    ImGui::PopID();
}

void CHubApp::DrawProjectList()
{
    ImGui::BeginChild("##list", ImVec2(0, static_cast<float>(GetScreenHeight()) - 90), false);

    if (m_State.vProjects.empty())
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const char* pMessage = lang.Word("no_projects");
        const ImVec2 textSize = ImGui::CalcTextSize(pMessage);
        ImGui::SetCursorPos(ImVec2(
            (available.x - textSize.x) * 0.5f,
            (available.y - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", pMessage);
    }

    for (int index = 0; index < static_cast<int>(m_State.vProjects.size()); index++)
    {
        DrawProjectCard(index);
    }

    ImGui::EndChild();

    if (m_State.SelectedProject < 0 ||
        m_State.SelectedProject >= static_cast<int>(m_State.vProjects.size()))
    {
        return;
    }

    if (ImGui::Button(lang.Word("open_selected"), ImVec2(140, 30)))
    {
        m_PendingResult = m_State.vProjects[m_State.SelectedProject].Path;
        m_ShouldExit = true;
    }
}

void CHubApp::DrawHeader()
{
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::Text("QUARK HUB");
    ImGui::SameLine();
    ImGui::TextDisabled("  %s", lang.Word("project_manager"));
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 354);

    if (ImGui::Button(lang.Word("import_project"), ImVec2(120, 28)))
    {
        const std::string picked = BrowseProjectFile();
        if (!picked.empty())
        {
            ImportProject(picked);
            Refresh();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(("+ %s", lang.Word("create_project")), ImVec2(134, 28)))
    {
        memset(m_State.aCreateName, 0, sizeof(m_State.aCreateName));
        snprintf(m_State.aCreatePath, sizeof(m_State.aCreatePath), "%s", ProjectsRoot());
        m_State.ShowCreate = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Plugins", ImVec2(80, 28)))
    {
        RefreshPlugins();
        m_State.ShowPluginManager = true;
    }

    ImGui::Separator();
    ImGui::Spacing();
}

void CHubApp::DrawCreatePopup()
{
    if (m_State.ShowCreate)
    {
        ImGui::OpenPopup(lang.Word("create_project"));
        m_State.ShowCreate = false;
    }

    ImGui::SetNextWindowSize(ImVec2(460, 182), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(lang.Word("create_project"), nullptr, ImGuiWindowFlags_NoResize))
    {
        return;
    }

    ImGui::Spacing();
    ImGui::Text("%s", lang.Word("project_name"));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##cname", m_State.aCreateName, sizeof(m_State.aCreateName));

    ImGui::Spacing();
    ImGui::Text("%s", lang.Word("location"));
    ImGui::SetNextItemWidth(-64);
    ImGui::InputText("##cpath", m_State.aCreatePath, sizeof(m_State.aCreatePath));
    ImGui::SameLine();

    if (ImGui::Button("Browse", ImVec2(56, 0)))
    {
        const std::string picked = BrowseFolder();
        if (!picked.empty())
        {
            snprintf(m_State.aCreatePath, sizeof(m_State.aCreatePath), "%s", picked.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool canCreate = m_State.aCreateName[0] != '\0' && m_State.aCreatePath[0] != '\0';
    if (!canCreate)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button(lang.Word("create"), ImVec2(110, 30)))
    {
        CreateProject(m_State.aCreateName, m_State.aCreatePath);
        Refresh();
        ImGui::CloseCurrentPopup();
    }

    if (!canCreate)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button(lang.Word("cancel"), ImVec2(110, 30)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void CHubApp::DrawRenamePopup()
{
    if (m_State.ShowRename)
    {
        ImGui::OpenPopup(lang.Word("rename_project"));
        m_State.ShowRename = false;
    }

    ImGui::SetNextWindowSize(ImVec2(380, 130), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(lang.Word("rename_project"), nullptr, ImGuiWindowFlags_NoResize))
    {
        return;
    }

    ImGui::Spacing();
    ImGui::Text("%s", lang.Word("new_name"));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##rname", m_State.aRenameBuffer, sizeof(m_State.aRenameBuffer));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(lang.Word("rename"), ImVec2(110, 28)))
    {
        if (m_State.RenameProjectIndex >= 0 && m_State.aRenameBuffer[0] != '\0')
        {
            RenameProject(
                m_State.vProjects[m_State.RenameProjectIndex].Path,
                m_State.aRenameBuffer);
            Refresh();
            m_State.SelectedProject = -1;
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(lang.Word("cancel"), ImVec2(110, 28)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void CHubApp::DrawDeletePopup()
{
    if (m_State.ShowDelete)
    {
        ImGui::OpenPopup(lang.Word("delete_project"));
        m_State.ShowDelete = false;
    }

    ImGui::SetNextWindowSize(ImVec2(380, 105), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(lang.Word("delete_project"), nullptr, ImGuiWindowFlags_NoResize))
    {
        return;
    }

    ImGui::Spacing();
    if (m_State.SelectedProject >= 0 &&
        m_State.SelectedProject < static_cast<int>(m_State.vProjects.size()))
    {
        ImGui::Text(lang.Word("delete_project_ask"),
            m_State.vProjects[m_State.SelectedProject].Name.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(lang.Word("delete"), ImVec2(110, 28)))
    {
        if (m_State.SelectedProject >= 0)
        {
            DeleteProject(m_State.vProjects[m_State.SelectedProject].Path);
            Refresh();
            m_State.SelectedProject = -1;
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(lang.Word("cancel"), ImVec2(110, 28)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void CHubApp::DrawVersionWarningPopup()
{
    if (m_State.ShowVersionWarning)
    {
        ImGui::OpenPopup(lang.Word("version_mismatch"));
        m_State.ShowVersionWarning = false;
    }

    ImGui::SetNextWindowSize(ImVec2(480, 155), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(lang.Word("version_mismatch"), nullptr, ImGuiWindowFlags_NoResize))
    {
        return;
    }

    ImGui::Spacing();
    ImGui::TextWrapped(lang.Word("version_mismatch_msg"),
        m_State.SavedVersion.c_str(), QUARK_ENGINE_VERSION);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(lang.Word("open_anyway"), ImVec2(130, 28)))
    {
        m_PendingResult = m_State.PendingOpenPath;
        m_ShouldExit = true;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(lang.Word("cancel"), ImVec2(110, 28)))
    {
        m_State.PendingOpenPath.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void CHubApp::DrawPluginManager()
{
    if (!m_State.ShowPluginManager)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float screenWidth = io.DisplaySize.x;
    const float screenHeight = io.DisplaySize.y;

    ImGui::SetNextWindowSize(ImVec2(720.0f, 480.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(screenWidth * 0.5f, screenHeight * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    bool open = true;
    ImGui::Begin("Plugin Manager##pmgr", &open);

    if (!open)
    {
        m_State.ShowPluginManager = false;
        m_State.SelectedPlugin = -1;
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##pmgr_tabs"))
    {
        if (ImGui::BeginTabItem("Installed"))
        {
            ImGui::BeginChild("##pmgr_list", ImVec2(220, -1), true);

            if (m_State.vPlugins.empty())
            {
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const char* pMessage = "No plugins installed.";
                const ImVec2 textSize = ImGui::CalcTextSize(pMessage);
                ImGui::SetCursorPos(ImVec2(
                    (available.x - textSize.x) * 0.5f,
                    (available.y - textSize.y) * 0.5f));
                ImGui::TextDisabled("%s", pMessage);
            }

            for (int index = 0; index < static_cast<int>(m_State.vPlugins.size()); index++)
            {
                SHubPlugin& plugin = m_State.vPlugins[index];
                ImGui::PushID(index);

                const bool isSelected = m_State.SelectedPlugin == index;
                const ImVec2 cardPos = ImGui::GetCursorScreenPos();
                const float cardWidth = ImGui::GetContentRegionAvail().x;
                const float cardHeight = 46.0f;
                const ImVec2 cardMax = ImVec2(cardPos.x + cardWidth, cardPos.y + cardHeight);

                ImGui::SetCursorScreenPos(cardPos);
                ImGui::InvisibleButton("##card", ImVec2(cardWidth, cardHeight));
                const bool isHovered = ImGui::IsItemHovered() && !isSelected;
                const bool isClicked = ImGui::IsItemClicked();
                const ImGuiStyle& style = ImGui::GetStyle();
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                const ImVec4& fill = isSelected ? style.HubCardSelected :
                    (isHovered ? style.HubCardHovered : style.HubCard);
                const ImVec4& border = isSelected ? style.HubCardSelectedBorder : style.HubCardBorder;
                pDrawList->AddRectFilled(
                    cardPos, cardMax, ImGui::ColorConvertFloat4ToU32(fill), style.HubCardRounding);
                if (style.HubCardBorderSize > 0.0f)
                {
                    pDrawList->AddRect(
                        cardPos, cardMax, ImGui::ColorConvertFloat4ToU32(border),
                        style.HubCardRounding, 0, style.HubCardBorderSize);
                }

                const ImVec4 badge = PluginBadgeColor(plugin.Name);
                const ImVec2 badgeMin = ImVec2(cardPos.x + 8, cardPos.y + 10);
                const ImVec2 badgeMax = ImVec2(badgeMin.x + 26, badgeMin.y + 26);

                char aLetter[2] = { static_cast<char>(toupper(static_cast<unsigned char>(plugin.Name[0]))), '\0' };
                const ImVec2 letterSize = ImGui::CalcTextSize(aLetter);

                if (plugin.Icon.id != 0)
                {
                    QcImGuiAddImage(ImGui::GetWindowDrawList(), &plugin.Icon, badgeMin, badgeMax);
                }
                else
                {
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        badgeMin, badgeMax, ImGui::ColorConvertFloat4ToU32(badge), 4.0f);
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(
                            badgeMin.x + (26 - letterSize.x) * 0.5f,
                            badgeMin.y + (26 - letterSize.y) * 0.5f),
                        IM_COL32(255, 255, 255, 230), aLetter);
                }

                if (!plugin.Enabled)
                {
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        ImVec2(badgeMax.x - 2, badgeMin.y + 2), 5.0f, IM_COL32(200, 60, 60, 255));
                }

                ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 44, cardPos.y + 14));
                if (!plugin.Enabled)
                {
                    ImGui::TextDisabled("%s", plugin.Name.c_str());
                }
                else
                {
                    ImGui::Text("%s", plugin.Name.c_str());
                }

                if (isClicked)
                {
                    m_State.SelectedPlugin = index;
                }

                ImGui::SetCursorScreenPos(ImVec2(cardPos.x, cardPos.y + cardHeight + 3));
                ImGui::Dummy(ImVec2(cardWidth, 0));
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##pmgr_detail", ImVec2(-1, -1), false);

            if (m_State.SelectedPlugin < 0 ||
                m_State.SelectedPlugin >= static_cast<int>(m_State.vPlugins.size()))
            {
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const char* pMessage = "Select a plugin to view details.";
                const ImVec2 textSize = ImGui::CalcTextSize(pMessage);
                ImGui::SetCursorPos(ImVec2(
                    (available.x - textSize.x) * 0.5f,
                    (available.y - textSize.y) * 0.5f));
                ImGui::TextDisabled("%s", pMessage);
            }
            else
            {
                SHubPlugin& plugin = m_State.vPlugins[m_State.SelectedPlugin];
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();

                ImVec2 iconPos = ImGui::GetCursorScreenPos();
                iconPos.x += 8;
                iconPos.y += 8;
                const ImVec2 iconMax = ImVec2(iconPos.x + 64, iconPos.y + 64);

                const ImVec4 badge = PluginBadgeColor(plugin.Name);
                char aLetter[2] = { static_cast<char>(toupper(static_cast<unsigned char>(plugin.Name[0]))), '\0' };
                const ImVec2 letterSize = ImGui::CalcTextSize(aLetter);

                if (plugin.Icon.id != 0)
                {
                    QcImGuiAddImage(pDrawList, &plugin.Icon, iconPos, iconMax);
                }
                else
                {
                    pDrawList->AddRectFilled(
                        iconPos, iconMax, ImGui::ColorConvertFloat4ToU32(badge), 8.0f);
                    pDrawList->AddText(
                        nullptr, 28.0f,
                        ImVec2(
                            iconPos.x + (64 - 16) * 0.5f,
                            iconPos.y + (64 - 28) * 0.5f),
                        IM_COL32(255, 255, 255, 230), aLetter);
                }

                ImGui::SetCursorScreenPos(ImVec2(iconMax.x + 14, iconPos.y + 4));
                ImGui::Text("%s", plugin.Name.c_str());

                ImGui::SetCursorScreenPos(ImVec2(iconMax.x + 14, iconPos.y + 26));
                if (plugin.Enabled)
                {
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1.0f), "Enabled");
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "Disabled");
                }

                ImGui::SetCursorScreenPos(ImVec2(iconPos.x - 8, iconMax.y + 18));
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextWrapped("%s", plugin.Description.c_str());
                ImGui::Spacing();
                ImGui::TextDisabled("Path: %s", plugin.Path.c_str());

                const float bottomY = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - 44;
                ImGui::SetCursorScreenPos(ImVec2(iconPos.x - 8, bottomY));
                ImGui::Separator();
                ImGui::Spacing();

                const char* pToggleLabel = plugin.Enabled ? "Disable" : "Enable";
                const ImVec4 toggleColor = plugin.Enabled
                    ? ImVec4(0.70f, 0.30f, 0.30f, 1.0f)
                    : ImVec4(0.20f, 0.60f, 0.30f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, toggleColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
                    toggleColor.x + 0.1f, toggleColor.y + 0.1f, toggleColor.z + 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(
                    toggleColor.x - 0.05f, toggleColor.y - 0.05f, toggleColor.z - 0.05f, 1.0f));

                if (ImGui::Button(pToggleLabel, ImVec2(110, 28)))
                {
                    plugin.Enabled = !plugin.Enabled;
                    PluginSetEnabled(plugin.Path, plugin.Enabled);
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f, 0.10f, 0.10f, 1.0f));

                if (ImGui::Button("Delete", ImVec2(90, 28)))
                {
                    ImGui::OpenPopup("Confirm Delete");
                }
                ImGui::PopStyleColor(3);

                ImGui::SetNextWindowSize(ImVec2(320, 100), ImGuiCond_Always);
                ImGui::SetNextWindowPos(
                    ImVec2(screenWidth * 0.5f, screenHeight * 0.5f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));

                if (ImGui::BeginPopupModal("Confirm Delete", nullptr,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
                {
                    ImGui::Spacing();
                    ImGui::Text("Delete plugin \"%s\"?", plugin.Name.c_str());
                    ImGui::TextDisabled("This removes the file from disk.");
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f, 0.10f, 0.10f, 1.0f));

                    if (ImGui::Button("Delete", ImVec2(90, 26)))
                    {
                        if (plugin.Icon.id != 0)
                        {
                            UnloadTexture(plugin.Icon);
                        }

                        const fs::path binary(plugin.Path);
                        const fs::path parent = binary.parent_path();
                        const fs::path pluginsRoot = fs::canonical("plugins");

                        if (fs::canonical(parent) != pluginsRoot)
                        {
                            fs::remove_all(parent);
                        }
                        else
                        {
                            fs::remove(binary);
                        }

                        const fs::path sentinel = binary.parent_path() / (binary.stem().string() + ".disabled");
                        if (fs::exists(sentinel))
                        {
                            fs::remove(sentinel);
                        }

                        RefreshPlugins();
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();

                    if (ImGui::Button("Cancel", ImVec2(80, 26)))
                    {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Explore"))
        {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const char* pTitle = "Browse online plugins";
            const char* pSubtitle = "Coming soon.";

            const ImVec2 titleSize = ImGui::CalcTextSize(pTitle);
            const ImVec2 subtitleSize = ImGui::CalcTextSize(pSubtitle);
            const float totalHeight = titleSize.y + 6 + subtitleSize.y;

            ImGui::SetCursorPos(ImVec2(
                (available.x - titleSize.x) * 0.5f,
                (available.y - totalHeight) * 0.5f));
            ImGui::Text("%s", pTitle);
            ImGui::SetCursorPosX((available.x - subtitleSize.x) * 0.5f);
            ImGui::TextDisabled("%s", pSubtitle);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

std::string CHubApp::Run(CPreferences& preferences)
{
    m_pPreferences = &preferences;

    fs::create_directories(ProjectsRoot());

    if (!fs::exists(RegistryFile()))
    {
        for (auto& entry : fs::directory_iterator(ProjectsRoot()))
        {
            if (!entry.is_directory() || !CProjectService::IsValid(entry.path().string()))
            {
                continue;
            }

            SHubProject project;
            project.Name = entry.path().filename().string();
            project.Path = fs::absolute(entry.path()).string();
            m_State.vProjects.push_back(project);
        }
    }

    Refresh();
    snprintf(m_State.aCreatePath, sizeof(m_State.aCreatePath), "%s", ProjectsRoot());

    m_PendingResult.clear();
    m_ShouldExit = false;

    while (!WindowShouldClose() && !m_ShouldExit)
    {
        BeginDrawing();
        ClearBackground(ToQuarkColor(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]));
        QcImGuiBegin();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())));
        ImGui::Begin(
            "##hub", nullptr,
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        DrawHeader();
        DrawProjectList();
        ImGui::End();

        DrawCreatePopup();
        DrawRenamePopup();
        DrawDeletePopup();
        DrawVersionWarningPopup();
        DrawPluginManager();

        QcImGuiEnd();
        EndDrawing();
    }

    m_pPreferences = nullptr;
    return m_PendingResult;
}
