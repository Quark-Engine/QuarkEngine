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
#include "editor/editor_theme.h"
#include "editor/editor_preferences.h"
#include "project.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;

using Json = nlohmann::json;

#define lang CLanguageManager::Get()

namespace
{

    const char* const HUB_PROJECTS_ROOT = "projects";
    const char* const HUB_REGISTRY_FILE = "config.json";

    bool ThemedButton(const char* pLabel, const ImVec2& size, const std::string& variant,
                      bool disabled = false)
    {
        if (!CThemeManager::PushVariant("button", variant, disabled))
        {
            return ImGui::Button(pLabel, size);
        }

        const bool clicked = ImGui::Button(pLabel, size);
        CThemeManager::PopVariant();
        return clicked;
    }

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
    for (auto& plugin : m_State.m_vPlugins)
    {
        if (plugin.m_Icon.id != 0)
        {
            UnloadTexture(plugin.m_Icon);
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
    char aPath[MAX_PATH] = {};
    BROWSEINFOA info = {};
    info.lpszTitle = lang.Word("select_project_loc");
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pPidl = SHBrowseForFolderA(&info);
    if (!pPidl)
    {
        return "";
    }

    SHGetPathFromIDListA(pPidl, aPath);
    CoTaskMemFree(pPidl);
    return aPath;

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
    char aPath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Quark Project (*.quarkproj)\0*.quarkproj\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = aPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "quarkproj";
    if (GetOpenFileNameA(&ofn))
    {
        return aPath;
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
    Json root;

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

    Json projects = Json::array();
    for (auto& project : m_State.m_vProjects)
    {
        projects.push_back({ { "name", project.m_Name }, { "path", project.m_Path } });
    }
    root["projects"] = projects;

    std::ofstream file(RegistryFile());
    file << root.dump(4);
}

void CHubApp::Refresh()
{
    m_State.m_vProjects.clear();

    if (fs::exists(RegistryFile()))
    {
        std::ifstream file(RegistryFile());
        Json root;
        try
        {
            file >> root;
            Json projects = root.contains("projects") ? root["projects"] : Json::array();
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
                    project.m_Name = entry["name"];
                    project.m_Path = CProjectService::ResolveRoot(path);
                    m_State.m_vProjects.push_back(project);
                }
            }
        }
        catch (...)
        {
        }
    }

    std::sort(m_State.m_vProjects.begin(), m_State.m_vProjects.end(),
              [](const SHubProject& a, const SHubProject& b)
              {
                  return a.m_Name < b.m_Name;
              });
}

void CHubApp::RefreshPlugins()
{
    for (auto& plugin : m_State.m_vPlugins)
    {
        if (plugin.m_Icon.id != 0)
        {
            UnloadTexture(plugin.m_Icon);
        }
    }

    m_State.m_vPlugins.clear();
    m_State.m_SelectedPlugin = -1;

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

            plugin.m_Path = binary.string();
            plugin.m_Name = entry.path().filename().string();

            const fs::path iconPath = entry.path() / "icon.png";
            const fs::path metaPath = entry.path() / "meta.txt";
            plugin.m_Icon = fs::exists(iconPath) ? LoadTexture(iconPath.string().c_str()) : Texture2D{ 0 };
            plugin.m_Description = ReadMetaLine(metaPath, "description");
        }
        else if (entry.is_regular_file())
        {
            if (!IsSupportedPluginExtension(entry.path().extension().string()))
            {
                continue;
            }

            plugin.m_Path = entry.path().string();
            plugin.m_Name = entry.path().stem().string();

            const fs::path iconPath = entry.path().parent_path() / (plugin.m_Name + ".png");
            const fs::path metaPath = entry.path().parent_path() / (plugin.m_Name + ".meta");
            plugin.m_Icon = fs::exists(iconPath) ? LoadTexture(iconPath.string().c_str()) : Texture2D{ 0 };
            plugin.m_Description = ReadMetaLine(metaPath, "description");
        }
        else
        {
            continue;
        }

        plugin.m_Enabled = PluginIsEnabled(plugin.m_Path);
        if (plugin.m_Description.empty())
        {
            plugin.m_Description = "No description provided.";
        }
        m_State.m_vPlugins.push_back(plugin);
    }

    std::sort(m_State.m_vPlugins.begin(), m_State.m_vPlugins.end(),
              [](const SHubPlugin& a, const SHubPlugin& b)
              {
                  return a.m_Name < b.m_Name;
              });
}

void CHubApp::CreateProject(const std::string& name, const std::string& base)
{
    const fs::path project = fs::path(base) / name;
    CScene emptyScene;
    CProjectService::CreateNew(project.string(), emptyScene);

    SHubProject entry;
    entry.m_Name = name;
    entry.m_Path = fs::absolute(project).string();
    m_State.m_vProjects.push_back(entry);
    SaveRegistry();
}

void CHubApp::DeleteProject(const std::string& path)
{
    fs::remove_all(path);
    m_State.m_vProjects.erase(
        std::remove_if(m_State.m_vProjects.begin(), m_State.m_vProjects.end(),
                       [&](const SHubProject& project)
                       {
                           return project.m_Path == path;
                       }),
        m_State.m_vProjects.end());
    SaveRegistry();
}

void CHubApp::RenameProject(const std::string& oldPath, const std::string& newName)
{
    const fs::path oldProject(oldPath);
    const fs::path newPath = oldProject.parent_path() / newName;
    fs::rename(oldProject, newPath);

    for (auto& project : m_State.m_vProjects)
    {
        if (project.m_Path != oldPath)
        {
            continue;
        }
        project.m_Name = newName;
        project.m_Path = fs::absolute(newPath).string();
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

    for (auto& existing : m_State.m_vProjects)
    {
        if (existing.m_Path == rootPath)
        {
            return;
        }
    }

    SHubProject project;
    project.m_Name = name;
    project.m_Path = rootPath;
    m_State.m_vProjects.push_back(project);
    SaveRegistry();
}

void CHubApp::DrawProjectCard(int index)
{
    ImGui::PushID(index);

    const bool isSelected = m_State.m_SelectedProject == index;
    const ImVec2 cardPos = ImGui::GetCursorScreenPos();
    const float cardWidth = ImGui::GetContentRegionAvail().x;
    const float cardHeight = 62.0f;
    const ImVec2 cardMax = ImVec2(cardPos.x + cardWidth, cardPos.y + cardHeight);

    ImGui::InvisibleButton("##card", ImVec2(cardWidth, cardHeight));
    const bool isHovered = ImGui::IsItemHovered() && !isSelected;
    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
    const ImVec4& fill = isSelected ? style.HubCardSelected : (isHovered ? style.HubCardHovered : style.HubCard);
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
        m_State.m_SelectedProject = index;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        const std::string savedVersion = CProjectService::GetVersion(m_State.m_vProjects[index].m_Path);
        if (!savedVersion.empty() && savedVersion != QUARK_ENGINE_VERSION)
        {
            m_State.m_PendingOpenPath = m_State.m_vProjects[index].m_Path;
            m_State.m_SavedVersion = savedVersion;
            m_State.m_ShowVersionWarning = true;
        }
        else
        {
            m_PendingResult = m_State.m_vProjects[index].m_Path;
            m_ShouldExit = true;
        }
    }

    if (ImGui::BeginPopupContextItem("##ctx"))
    {
        if (ImGui::MenuItem(lang.Word("open")))
        {
            m_PendingResult = m_State.m_vProjects[index].m_Path;
            m_ShouldExit = true;
        }

        ImGui::Separator();
        if (ImGui::MenuItem(lang.Word("rename")))
        {
            m_State.m_RenameProjectIndex = index;
            snprintf(m_State.m_aRenameBuffer, sizeof(m_State.m_aRenameBuffer), "%s",
                     m_State.m_vProjects[index].m_Name.c_str());
            m_State.m_ShowRename = true;
        }

        if (ImGui::MenuItem(lang.Word("delete")))
        {
            m_State.m_SelectedProject = index;
            m_State.m_ShowDelete = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 14, cardPos.y + 11));
    ImGui::Text("%s", m_State.m_vProjects[index].m_Name.c_str());

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 14, cardPos.y + 36));
    ImGui::TextDisabled("%s", m_State.m_vProjects[index].m_Path.c_str());

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x, cardPos.y + cardHeight + 4));
    ImGui::Dummy(ImVec2(cardWidth, 0));

    ImGui::PopID();
}

void CHubApp::DrawProjectList()
{
    ImGui::BeginChild("##list", ImVec2(0, static_cast<float>(GetScreenHeight()) - 90), false);

    if (m_State.m_vProjects.empty())
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const char* pMessage = lang.Word("no_projects");
        const ImVec2 textSize = ImGui::CalcTextSize(pMessage);
        ImGui::SetCursorPos(ImVec2(
            (available.x - textSize.x) * 0.5f,
            (available.y - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", pMessage);
    }

    for (int index = 0; index < static_cast<int>(m_State.m_vProjects.size()); index++)
    {
        DrawProjectCard(index);
    }

    ImGui::EndChild();

    if (m_State.m_SelectedProject < 0 ||
        m_State.m_SelectedProject >= static_cast<int>(m_State.m_vProjects.size()))
    {
        return;
    }

    if (ThemedButton(lang.Word("open_selected"), ImVec2(140, 30), "secondary"))
    {
        m_PendingResult = m_State.m_vProjects[m_State.m_SelectedProject].m_Path;
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

    if (ThemedButton(("+ %s", lang.Word("create_project")), ImVec2(134, 28), "secondary"))
    {
        memset(m_State.m_aCreateName, 0, sizeof(m_State.m_aCreateName));
        snprintf(m_State.m_aCreatePath, sizeof(m_State.m_aCreatePath), "%s", ProjectsRoot());
        m_State.m_ShowCreate = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Plugins", ImVec2(80, 28)))
    {
        RefreshPlugins();
        m_State.m_ShowPluginManager = true;
    }

    ImGui::Separator();
    ImGui::Spacing();
}

void CHubApp::DrawCreatePopup()
{
    if (m_State.m_ShowCreate)
    {
        ImGui::OpenPopup(lang.Word("create_project"));
        m_State.m_ShowCreate = false;
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
    ImGui::InputText("##cname", m_State.m_aCreateName, sizeof(m_State.m_aCreateName));

    ImGui::Spacing();
    ImGui::Text("%s", lang.Word("location"));
    ImGui::SetNextItemWidth(-64);
    ImGui::InputText("##cpath", m_State.m_aCreatePath, sizeof(m_State.m_aCreatePath));
    ImGui::SameLine();

    if (ImGui::Button("Browse", ImVec2(56, 0)))
    {
        const std::string picked = BrowseFolder();
        if (!picked.empty())
        {
            snprintf(m_State.m_aCreatePath, sizeof(m_State.m_aCreatePath), "%s", picked.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool canCreate = m_State.m_aCreateName[0] != '\0' && m_State.m_aCreatePath[0] != '\0';
    if (!canCreate)
    {
        ImGui::BeginDisabled();
    }

    if (ThemedButton(lang.Word("create"), ImVec2(110, 30), "primary", !canCreate))
    {
        CreateProject(m_State.m_aCreateName, m_State.m_aCreatePath);
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
    if (m_State.m_ShowRename)
    {
        ImGui::OpenPopup(lang.Word("rename_project"));
        m_State.m_ShowRename = false;
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
    ImGui::InputText("##rname", m_State.m_aRenameBuffer, sizeof(m_State.m_aRenameBuffer));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(lang.Word("rename"), ImVec2(110, 28)))
    {
        if (m_State.m_RenameProjectIndex >= 0 && m_State.m_aRenameBuffer[0] != '\0')
        {
            RenameProject(
                m_State.m_vProjects[m_State.m_RenameProjectIndex].m_Path,
                m_State.m_aRenameBuffer);
            Refresh();
            m_State.m_SelectedProject = -1;
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
    if (m_State.m_ShowDelete)
    {
        ImGui::OpenPopup(lang.Word("delete_project"));
        m_State.m_ShowDelete = false;
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
    if (m_State.m_SelectedProject >= 0 &&
        m_State.m_SelectedProject < static_cast<int>(m_State.m_vProjects.size()))
    {
        ImGui::Text(lang.Word("delete_project_ask"),
                    m_State.m_vProjects[m_State.m_SelectedProject].m_Name.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ThemedButton(lang.Word("delete"), ImVec2(110, 28), "danger"))
    {
        if (m_State.m_SelectedProject >= 0)
        {
            DeleteProject(m_State.m_vProjects[m_State.m_SelectedProject].m_Path);
            Refresh();
            m_State.m_SelectedProject = -1;
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
    if (m_State.m_ShowVersionWarning)
    {
        ImGui::OpenPopup(lang.Word("version_mismatch"));
        m_State.m_ShowVersionWarning = false;
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
                       m_State.m_SavedVersion.c_str(), QUARK_ENGINE_VERSION);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(lang.Word("open_anyway"), ImVec2(130, 28)))
    {
        m_PendingResult = m_State.m_PendingOpenPath;
        m_ShouldExit = true;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(lang.Word("cancel"), ImVec2(110, 28)))
    {
        m_State.m_PendingOpenPath.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void CHubApp::DrawPluginManager()
{
    if (!m_State.m_ShowPluginManager)
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
        m_State.m_ShowPluginManager = false;
        m_State.m_SelectedPlugin = -1;
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##pmgr_tabs"))
    {
        if (ImGui::BeginTabItem("Installed"))
        {
            ImGui::BeginChild("##pmgr_list", ImVec2(220, -1), true);

            if (m_State.m_vPlugins.empty())
            {
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const char* pMessage = "No plugins installed.";
                const ImVec2 textSize = ImGui::CalcTextSize(pMessage);
                ImGui::SetCursorPos(ImVec2(
                    (available.x - textSize.x) * 0.5f,
                    (available.y - textSize.y) * 0.5f));
                ImGui::TextDisabled("%s", pMessage);
            }

            for (int index = 0; index < static_cast<int>(m_State.m_vPlugins.size()); index++)
            {
                SHubPlugin& plugin = m_State.m_vPlugins[index];
                ImGui::PushID(index);

                const bool isSelected = m_State.m_SelectedPlugin == index;
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
                const ImVec4& fill = isSelected ? style.HubCardSelected : (isHovered ? style.HubCardHovered : style.HubCard);
                const ImVec4& border = isSelected ? style.HubCardSelectedBorder : style.HubCardBorder;
                pDrawList->AddRectFilled(
                    cardPos, cardMax, ImGui::ColorConvertFloat4ToU32(fill), style.HubCardRounding);
                if (style.HubCardBorderSize > 0.0f)
                {
                    pDrawList->AddRect(
                        cardPos, cardMax, ImGui::ColorConvertFloat4ToU32(border),
                        style.HubCardRounding, 0, style.HubCardBorderSize);
                }

                const ImVec4 badge = PluginBadgeColor(plugin.m_Name);
                const ImVec2 badgeMin = ImVec2(cardPos.x + 8, cardPos.y + 10);
                const ImVec2 badgeMax = ImVec2(badgeMin.x + 26, badgeMin.y + 26);

                char aLetter[2] = { static_cast<char>(toupper(static_cast<unsigned char>(plugin.m_Name[0]))), '\0' };
                const ImVec2 letterSize = ImGui::CalcTextSize(aLetter);

                if (plugin.m_Icon.id != 0)
                {
                    QcImGuiAddImage(ImGui::GetWindowDrawList(), &plugin.m_Icon, badgeMin, badgeMax);
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

                if (!plugin.m_Enabled)
                {
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        ImVec2(badgeMax.x - 2, badgeMin.y + 2), 5.0f, IM_COL32(200, 60, 60, 255));
                }

                ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 44, cardPos.y + 14));
                if (!plugin.m_Enabled)
                {
                    ImGui::TextDisabled("%s", plugin.m_Name.c_str());
                }
                else
                {
                    ImGui::Text("%s", plugin.m_Name.c_str());
                }

                if (isClicked)
                {
                    m_State.m_SelectedPlugin = index;
                }

                ImGui::SetCursorScreenPos(ImVec2(cardPos.x, cardPos.y + cardHeight + 3));
                ImGui::Dummy(ImVec2(cardWidth, 0));
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##pmgr_detail", ImVec2(-1, -1), false);

            if (m_State.m_SelectedPlugin < 0 ||
                m_State.m_SelectedPlugin >= static_cast<int>(m_State.m_vPlugins.size()))
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
                SHubPlugin& plugin = m_State.m_vPlugins[m_State.m_SelectedPlugin];
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();

                ImVec2 iconPos = ImGui::GetCursorScreenPos();
                iconPos.x += 8;
                iconPos.y += 8;
                const ImVec2 iconMax = ImVec2(iconPos.x + 64, iconPos.y + 64);

                const ImVec4 badge = PluginBadgeColor(plugin.m_Name);
                char aLetter[2] = { static_cast<char>(toupper(static_cast<unsigned char>(plugin.m_Name[0]))), '\0' };
                const ImVec2 letterSize = ImGui::CalcTextSize(aLetter);

                if (plugin.m_Icon.id != 0)
                {
                    QcImGuiAddImage(pDrawList, &plugin.m_Icon, iconPos, iconMax);
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
                ImGui::Text("%s", plugin.m_Name.c_str());

                ImGui::SetCursorScreenPos(ImVec2(iconMax.x + 14, iconPos.y + 26));
                if (plugin.m_Enabled)
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
                ImGui::TextWrapped("%s", plugin.m_Description.c_str());
                ImGui::Spacing();
                ImGui::TextDisabled("Path: %s", plugin.m_Path.c_str());

                const float bottomY = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - 44;
                ImGui::SetCursorScreenPos(ImVec2(iconPos.x - 8, bottomY));
                ImGui::Separator();
                ImGui::Spacing();

                const char* pToggleLabel = plugin.m_Enabled ? "Disable" : "Enable";
                const ImVec4 toggleColor = plugin.m_Enabled
                    ? ImVec4(0.70f, 0.30f, 0.30f, 1.0f)
                    : ImVec4(0.20f, 0.60f, 0.30f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, toggleColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(toggleColor.x + 0.1f, toggleColor.y + 0.1f, toggleColor.z + 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(toggleColor.x - 0.05f, toggleColor.y - 0.05f, toggleColor.z - 0.05f, 1.0f));

                if (ImGui::Button(pToggleLabel, ImVec2(110, 28)))
                {
                    plugin.m_Enabled = !plugin.m_Enabled;
                    PluginSetEnabled(plugin.m_Path, plugin.m_Enabled);
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ThemedButton("Delete", ImVec2(90, 28), "danger"))
                {
                    ImGui::OpenPopup("Confirm Delete");
                }

                ImGui::SetNextWindowSize(ImVec2(320, 100), ImGuiCond_Always);
                ImGui::SetNextWindowPos(
                    ImVec2(screenWidth * 0.5f, screenHeight * 0.5f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));

                if (ImGui::BeginPopupModal("Confirm Delete", nullptr,
                                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
                {
                    ImGui::Spacing();
                    ImGui::Text("Delete plugin \"%s\"?", plugin.m_Name.c_str());
                    ImGui::TextDisabled("This removes the file from disk.");
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ThemedButton("Delete", ImVec2(90, 26), "danger"))
                    {
                        if (plugin.m_Icon.id != 0)
                        {
                            UnloadTexture(plugin.m_Icon);
                        }

                        const fs::path binary(plugin.m_Path);
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
            project.m_Name = entry.path().filename().string();
            project.m_Path = fs::absolute(entry.path()).string();
            m_State.m_vProjects.push_back(project);
        }
    }

    Refresh();
    snprintf(m_State.m_aCreatePath, sizeof(m_State.m_aCreatePath), "%s", ProjectsRoot());

    m_PendingResult.clear();
    m_ShouldExit = false;

    while (!WindowShouldClose() && !m_ShouldExit)
    {
        BeginDrawing();
        ClearBackground(ToQuarkColor(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]));
        QcImGuiBegin();

        const bool rtl = CLanguageManager::Get().m_Current == "arabic";
        if (rtl)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(1.0f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(1.0f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.0f, 0.5f));
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())));
        ImGui::Begin(
            "##hub", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBringToFrontOnFocus);

        DrawHeader();
        DrawProjectList();
        ImGui::End();

        DrawCreatePopup();
        DrawRenamePopup();
        DrawDeletePopup();
        DrawVersionWarningPopup();
        DrawPluginManager();

        if (rtl)
        {
            ImGui::PopStyleVar(3);
        }

        QcImGuiEnd();
        EndDrawing();
    }

    m_pPreferences = nullptr;
    return m_PendingResult;
}
