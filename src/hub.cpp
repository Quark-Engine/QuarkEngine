#define NOMINMAX

#include "raylib.h"

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
#endif

#include "hub.h"
#include "headers/version.h"
#include "headers/language_manager.h"
#include "project.h"
#include "rlImGui.h"
#include "imgui.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>

#define lang LanguageManager::get()

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char* HUB_PROJECTS_ROOT = "projects";
static const char* HUB_REGISTRY_FILE = "config.json";

struct HubProject {
    std::string name;
    std::string path;
};

static std::vector<HubProject> hub_projects;
static int  hub_selected         = -1;
static int  hub_rename_index     = -1;
static bool hub_show_create      = false;
static bool hub_show_rename      = false;
static bool hub_show_delete      = false;
static char hub_create_name[256] = "";
static char hub_create_path[512] = "";
static char hub_rename_buf[256]  = "";

static bool        hub_show_version_warning = false;
static std::string hub_pending_open_path    = "";
static std::string hub_saved_version        = "";

// Plugin Management
static bool hub_show_plugin_manager = false;

struct HubPluginInfo {
    std::string name;
    std::string path;
    std::string description;
    bool enabled;
};

static std::vector<HubPluginInfo> hub_plugin_list;
static int hub_plugin_selected = -1;

static fs::path hub_plugin_disabled_sentinel(const std::string& plugin_path) {
    fs::path p(plugin_path);
    return p.parent_path() / (p.stem().string() + ".disabled");
}

static bool hub_plugin_is_enabled(const std::string& plugin_path) {
    return !fs::exists(hub_plugin_disabled_sentinel(plugin_path));
}

static void hub_plugin_set_enabled(const std::string& plugin_path, bool enabled) {
    fs::path sentinel = hub_plugin_disabled_sentinel(plugin_path);

    if (enabled) {
        if (fs::exists(sentinel)) fs::remove(sentinel);
    }

    else    
        std::ofstream f(sentinel);
}

static std::string hub_plugin_read_meta_description(const fs::path& plugin_path) {
    fs::path meta = plugin_path.parent_path() / (plugin_path.stem().string() + ".meta");
    if (!fs::exists(meta)) return "";

    std::ifstream f(meta);
    std::string line;

    while (std::getline(f, line)) {
        if (line.rfind("description=", 0) == 0)
            return line.substr(12);
    }

    return "";
}

static void hub_refresh_plugins() {
    hub_plugin_list.clear();
    hub_plugin_selected = -1;

    const std::string plugins_dir = "plugins";
    if (!fs::exists(plugins_dir)) return;

    for (auto& entry : fs::directory_iterator(plugins_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();

        #ifdef _WIN32
                if (ext != ".dll") continue;
        #else
                if (ext != ".so") continue;
        #endif

        HubPluginInfo info;
        info.path    = entry.path().string();
        info.name    = entry.path().stem().string();
        info.enabled = hub_plugin_is_enabled(info.path);
        info.description = hub_plugin_read_meta_description(entry.path());

        if (info.description.empty())
            info.description = "No description provided.";

        hub_plugin_list.push_back(info);
    }

    std::sort(hub_plugin_list.begin(), hub_plugin_list.end(), [](const HubPluginInfo& a, const HubPluginInfo& b){ return a.name < b.name; });
}

static ImVec4 hub_plugin_badge_color(const std::string& name) {
    static const ImVec4 palette[] = {
        {0.20f, 0.55f, 0.95f, 1.f},
        {0.18f, 0.72f, 0.56f, 1.f},
        {0.85f, 0.45f, 0.20f, 1.f},
        {0.65f, 0.35f, 0.90f, 1.f},
        {0.90f, 0.70f, 0.10f, 1.f},
        {0.85f, 0.25f, 0.35f, 1.f},
    };

    size_t h = 0;
    for (char c : name) h = h * 31 + (unsigned char)c;
    return palette[h % 6];
}

static void hub_draw_plugin_manager() {
    if (!hub_show_plugin_manager) return;

    ImGuiIO& io = ImGui::GetIO();
    float W = io.DisplaySize.x;
    float H = io.DisplaySize.y;

    const float WND_W = 720.f, WND_H = 480.f;
    ImGui::SetNextWindowSize(ImVec2(WND_W, WND_H), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(W * 0.5f, H * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    bool open = true;
    ImGui::Begin("Plugin Manager##pmgr", &open);

    if (!open) {
        hub_show_plugin_manager = false;
        hub_plugin_selected = -1;
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##pmgr_tabs")) {
        if (ImGui::BeginTabItem("Installed")) {
            ImGui::BeginChild("##pmgr_list", ImVec2(220, -1), true);

            if (hub_plugin_list.empty()) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                const char* msg = "No plugins installed.";
                ImVec2 ts = ImGui::CalcTextSize(msg);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", msg);
            }

            for (int i = 0; i < (int)hub_plugin_list.size(); i++) {
                auto& pi = hub_plugin_list[i];
                ImGui::PushID(i);

                bool is_sel = (hub_plugin_selected == i);

                ImVec2 card_pos = ImGui::GetCursorScreenPos();
                float card_w    = ImGui::GetContentRegionAvail().x;
                float card_h    = 46.f;

                ImU32 bg_col = is_sel ? IM_COL32(30, 80, 140, 255) : IM_COL32(26, 28, 31, 255);

                ImGui::GetWindowDrawList()->AddRectFilled( card_pos, ImVec2(card_pos.x + card_w, card_pos.y + card_h), bg_col);
                ImGui::GetWindowDrawList()->AddRect(
                    card_pos, ImVec2(card_pos.x + card_w, card_pos.y + card_h),
                    is_sel ? IM_COL32(50,130,220,255) : IM_COL32(50,52,56,255)
                );

                ImVec4 badge = hub_plugin_badge_color(pi.name);
                ImVec2 badge_min = ImVec2(card_pos.x + 8, card_pos.y + 10);
                ImVec2 badge_max = ImVec2(badge_min.x + 26, badge_min.y + 26);
                ImGui::GetWindowDrawList()->AddRectFilled(badge_min, badge_max, ImGui::ColorConvertFloat4ToU32(badge), 4.f);

                char letter[2] = { (char)toupper((unsigned char)pi.name[0]), '\0' };
                ImVec2 letter_sz = ImGui::CalcTextSize(letter);

                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(badge_min.x + (26 - letter_sz.x) * 0.5f, badge_min.y + (26 - letter_sz.y) * 0.5f), 
                    IM_COL32(255,255,255,230), letter
                );

                if (!pi.enabled) {
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(badge_max.x - 2, badge_min.y + 2), 5.f, IM_COL32(200, 60, 60, 255));
                }

                ImGui::SetCursorScreenPos(ImVec2(card_pos.x + 44, card_pos.y + 14));
                if (!pi.enabled) ImGui::TextDisabled("%s", pi.name.c_str());
                else ImGui::Text("%s", pi.name.c_str());

                ImGui::SetCursorScreenPos(card_pos);
                ImGui::InvisibleButton("##card", ImVec2(card_w, card_h));
                if (ImGui::IsItemClicked()) hub_plugin_selected = i;

                ImGui::SetCursorScreenPos(ImVec2(card_pos.x, card_pos.y + card_h + 3));
                ImGui::Dummy(ImVec2(card_w, 0));
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##pmgr_detail", ImVec2(-1, -1), false);

            if (hub_plugin_selected >= 0 && hub_plugin_selected < (int)hub_plugin_list.size()) {
                HubPluginInfo& pi = hub_plugin_list[hub_plugin_selected];

                ImVec4 badge = hub_plugin_badge_color(pi.name);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 icon_pos = ImGui::GetCursorScreenPos();

                icon_pos.x += 8; icon_pos.y += 8;
                ImVec2 icon_max = ImVec2(icon_pos.x + 64, icon_pos.y + 64);
                dl->AddRectFilled(icon_pos, icon_max, ImGui::ColorConvertFloat4ToU32(badge), 8.f);

                char letter[2] = { (char)toupper((unsigned char)pi.name[0]), '\0' };
                ImVec2 lsz = ImGui::CalcTextSize(letter);
                dl->AddText(nullptr, 28.f, ImVec2(icon_pos.x + (64 - 16) * 0.5f, icon_pos.y + (64 - 28) * 0.5f), IM_COL32(255,255,255,230), letter);

                ImGui::SetCursorScreenPos(ImVec2(icon_max.x + 14, icon_pos.y + 4));
                ImGui::Text("%s", pi.name.c_str());

                ImGui::SetCursorScreenPos(ImVec2(icon_max.x + 14, icon_pos.y + 26));

                if (pi.enabled) ImGui::TextColored(ImVec4(0.3f,0.8f,0.4f,1.f), "Enabled");
                else ImGui::TextColored(ImVec4(0.7f,0.3f,0.3f,1.f), "Disabled");

                ImGui::SetCursorScreenPos(ImVec2(icon_pos.x - 8, icon_max.y + 18));

                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextWrapped("%s", pi.description.c_str());
                ImGui::Spacing();
                ImGui::TextDisabled("Path: %s", pi.path.c_str());

                float bottom_y = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - 44;
                ImGui::SetCursorScreenPos(ImVec2(icon_pos.x - 8, bottom_y));
                ImGui::Separator();
                ImGui::Spacing();

                const char* toggle_lbl = pi.enabled ? "Disable" : "Enable";
                ImVec4 tog_col = pi.enabled ? ImVec4(0.70f, 0.30f, 0.30f, 1.f) : ImVec4(0.20f, 0.60f, 0.30f, 1.f);

                ImGui::PushStyleColor(ImGuiCol_Button, tog_col);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(tog_col.x+0.1f, tog_col.y+0.1f, tog_col.z+0.1f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(tog_col.x-0.05f, tog_col.y-0.05f, tog_col.z-0.05f, 1.f));

                if (ImGui::Button(toggle_lbl, ImVec2(110, 28))) {
                    pi.enabled = !pi.enabled;
                    hub_plugin_set_enabled(pi.path, pi.enabled);
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f,0.15f,0.15f,1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f,0.20f,0.20f,1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f,0.10f,0.10f,1.f));

                if (ImGui::Button("Delete", ImVec2(90, 28))) {
                    ImGui::OpenPopup("Confirm Delete");
                }
                ImGui::PopStyleColor(3);

                ImGui::SetNextWindowSize(ImVec2(320, 100), ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(W * 0.5f, H * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));

                if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

                    ImGui::Spacing();
                    ImGui::Text("Delete plugin \"%s\"?", pi.name.c_str());
                    ImGui::TextDisabled("This removes the file from disk.");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f,0.15f,0.15f,1.f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f,0.20f,0.20f,1.f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f,0.10f,0.10f,1.f));

                    if (ImGui::Button("Delete", ImVec2(90, 26))) {
                        fs::remove(pi.path);
                        fs::path sentinel = hub_plugin_disabled_sentinel(pi.path);
                        if (fs::exists(sentinel)) fs::remove(sentinel);

                        hub_refresh_plugins();
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();

                    if (ImGui::Button("Cancel", ImVec2(80, 26)))
                        ImGui::CloseCurrentPopup();

                    ImGui::EndPopup();
                }

            } 
            
            else {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                const char* msg = "Select a plugin to view details.";
                ImVec2 ts = ImGui::CalcTextSize(msg);

                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", msg);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Explore")) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* line1 = "Browse online plugins";
            const char* line2 = "Coming soon.";

            ImVec2 s1 = ImGui::CalcTextSize(line1);
            ImVec2 s2 = ImGui::CalcTextSize(line2);
            float total_h = s1.y + 6 + s2.y;

            ImGui::SetCursorPos(ImVec2((avail.x - s1.x) * 0.5f, (avail.y - total_h) * 0.5f));
            ImGui::Text("%s", line1);
            ImGui::SetCursorPosX((avail.x - s2.x) * 0.5f);
            ImGui::TextDisabled("%s", line2);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// Hub
static void hub_save_registry() {
    json j;
    
    if (fs::exists(HUB_REGISTRY_FILE)) {
        std::ifstream f(HUB_REGISTRY_FILE);
        try {
            f >> j;
        } catch (...) {}
    }
    
    if (!j.contains("language")) {
        j["language"] = "en_us";
    }

    json projects = json::array();
    for (auto& p : hub_projects)
        projects.push_back( { {"name", p.name}, {"path", p.path} } );
    
    j["projects"] = projects;
    
    std::ofstream f(HUB_REGISTRY_FILE);
    f << j.dump(4);
}

static void hub_refresh() {
    hub_projects.clear();

    if (fs::exists(HUB_REGISTRY_FILE)) {
        std::ifstream f(HUB_REGISTRY_FILE);
        json j;
        try {
            f >> j;
            json projects = j.contains("projects") ? j["projects"] : json::array();
            if (projects.is_array()) {
                for (auto& entry : projects) {
                    std::string path = entry["path"];
                    if (project_is_valid(path)) {
                        HubProject p;
                        p.name = entry["name"];
                        p.path = project_resolve_root(path);
                        hub_projects.push_back(p);
                    }
                }
            }
        } catch (...) {}
    }

    std::sort(hub_projects.begin(), hub_projects.end(),
        [](const HubProject& a, const HubProject& b) { return a.name < b.name; });
}

static void hub_create_project(const std::string& name, const std::string& base) {
    fs::path proj = fs::path(base) / name;
    Scene empty_scene;
    project_new(proj.string(), empty_scene);

    HubProject p;
    p.name = name;
    p.path = fs::absolute(proj).string();
    hub_projects.push_back(p);
    hub_save_registry();
}

static void hub_delete_project(const std::string& path) {
    fs::remove_all(path);
    hub_projects.erase(std::remove_if(hub_projects.begin(), hub_projects.end(),
        [&](const HubProject& p) { return p.path == path; }), hub_projects.end());
    hub_save_registry();
}

static void hub_rename_project(const std::string& old_path, const std::string& new_name) {
    fs::path p(old_path);
    fs::path new_path = p.parent_path() / new_name;
    fs::rename(p, new_path);

    for (auto& proj : hub_projects) {
        if (proj.path == old_path) {
            proj.name = new_name;
            proj.path = fs::absolute(new_path).string();
            break;
        }
    }
    hub_save_registry();
}

static std::string hub_browse_folder() {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    BROWSEINFOA bi = {};
    bi.lpszTitle = lang.word("select_project_loc");
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        SHGetPathFromIDListA(pidl, path);
        CoTaskMemFree(pidl);
    }
    return path;

#elif __linux__
    FILE* pipe = popen("zenity --file-selection --directory 2>/dev/null", "r");
    if (!pipe) return "";
    char result[512] = {};
    if (fgets(result, sizeof(result), pipe)) {
        size_t len = strlen(result);
        if (len > 0 && result[len - 1] == '\n') result[len - 1] = '\0';
    }
    pclose(pipe);
    return result;
#endif
}

static std::string hub_browse_project_file() {
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
    if (GetOpenFileNameA(&ofn)) return path;
    return "";

#elif __linux__
    FILE* pipe = popen("zenity --file-selection --file-filter='*.quarkproj' 2>/dev/null", "r");
    if (!pipe) return "";
    char result[512] = {};
    if (fgets(result, sizeof(result), pipe)) {
        size_t len = strlen(result);
        if (len > 0 && result[len - 1] == '\n') result[len - 1] = '\0';
    }
    pclose(pipe);
    return result;
#else
    return "";
#endif
}

static void hub_import_project(const std::string& manifest_or_path) {
    if (!project_is_valid(manifest_or_path)) return;

    const std::string root_path = project_resolve_root(manifest_or_path);
    const fs::path root(root_path);
    const std::string name = root.filename().string().empty() ? root.stem().string() : root.filename().string();

    for (auto& existing : hub_projects) {
        if (existing.path == root_path) return;
    }

    HubProject project;
    project.name = name;
    project.path = root_path;
    hub_projects.push_back(project);
    hub_save_registry();
}

std::string run_hub() {
    fs::create_directories(HUB_PROJECTS_ROOT);
    if (!fs::exists(HUB_REGISTRY_FILE)) {
        for (auto& entry : fs::directory_iterator(HUB_PROJECTS_ROOT)) {
            if (!entry.is_directory()) continue;
            if (!project_is_valid(entry.path().string())) continue;

            HubProject p;
            p.name = entry.path().filename().string();
            p.path = fs::absolute(entry.path()).string();
            hub_projects.push_back(p);
        }
    }
    hub_refresh();
    strncpy(hub_create_path, HUB_PROJECTS_ROOT, sizeof(hub_create_path) - 1);

    std::string result_path = "";
    bool should_exit = false;

    while (!WindowShouldClose() && !should_exit) {
        BeginDrawing();
        ClearBackground({ 33, 35, 38, 255 });
        rlImGuiBegin();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)GetScreenWidth(), (float)GetScreenHeight()));
        ImGui::Begin(
            "##hub", nullptr,
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoBringToFrontOnFocus
        );

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::Text("QUARK HUB");
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", lang.word("project_manager"));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 354);

        if (ImGui::Button(lang.word("import_project"), ImVec2(120, 28))) {
            std::string picked = hub_browse_project_file();
            if (!picked.empty()) {
                hub_import_project(picked);
                hub_refresh();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button(("+ %s", lang.word("create_project")), ImVec2(134, 28))) {
            memset(hub_create_name, 0, sizeof(hub_create_name));
            strncpy(hub_create_path, HUB_PROJECTS_ROOT, sizeof(hub_create_path) - 1);
            hub_show_create = true;
        }

        ImGui::SameLine();
        
        if (ImGui::Button("Plugins", ImVec2(80, 28))) {
            hub_refresh_plugins();
            hub_show_plugin_manager = true;
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("##list", ImVec2(0, (float)GetScreenHeight() - 90), false);

        if (hub_projects.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* msg = lang.word("no_projects");
            ImVec2 ts = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
            ImGui::TextDisabled("%s", msg);
        }

        for (int i = 0; i < (int)hub_projects.size(); i++) {
            ImGui::PushID(i);

            bool is_sel = (hub_selected == i);
            ImVec2 card_pos = ImGui::GetCursorScreenPos();
            float  card_w   = ImGui::GetContentRegionAvail().x;
            float  card_h   = 62.0f;

            ImGui::GetWindowDrawList()->AddRectFilled(
                card_pos,
                ImVec2(card_pos.x + card_w, card_pos.y + card_h),
                is_sel ? IM_COL32(30, 80, 140, 255) : IM_COL32(26, 28, 31, 255)
            );

            ImGui::GetWindowDrawList()->AddRect(
                card_pos,
                ImVec2(card_pos.x + card_w, card_pos.y + card_h),
                is_sel ? IM_COL32(50, 130, 220, 255) : IM_COL32(50, 52, 56, 255)
            );

            ImGui::InvisibleButton("##card", ImVec2(card_w, card_h));

            if (ImGui::IsItemHovered() && !is_sel) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    card_pos,
                    ImVec2(card_pos.x + card_w, card_pos.y + card_h),
                    IM_COL32(40, 42, 46, 180)
                );
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                hub_selected = i;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                std::string saved_ver = get_project_version(hub_projects[i].path);
                if (!saved_ver.empty() && saved_ver != QUARK_ENGINE_VERSION) {
                    hub_pending_open_path = hub_projects[i].path;
                    hub_saved_version = saved_ver;
                    hub_show_version_warning = true;
                } 
                
                else {
                    result_path = hub_projects[i].path;
                    should_exit = true;
                }
            }

            if (ImGui::BeginPopupContextItem("##ctx")) {
                if (ImGui::MenuItem(lang.word("open"))) {
                    result_path = hub_projects[i].path;
                    should_exit = true;
                }

                ImGui::Separator();
                if (ImGui::MenuItem(lang.word("rename"))) {
                    hub_rename_index = i;
                    strncpy(hub_rename_buf, hub_projects[i].name.c_str(), sizeof(hub_rename_buf) - 1);
                    hub_show_rename = true;
                }

                if (ImGui::MenuItem(lang.word("delete"))) {
                    hub_selected    = i;
                    hub_show_delete = true;
                }
                ImGui::EndPopup();
            }

            ImGui::SetCursorScreenPos(ImVec2(card_pos.x + 14, card_pos.y + 11));
            ImGui::Text("%s", hub_projects[i].name.c_str());

            ImGui::SetCursorScreenPos(ImVec2(card_pos.x + 14, card_pos.y + 36));
            ImGui::TextDisabled("%s", hub_projects[i].path.c_str());

            ImGui::SetCursorScreenPos(ImVec2(card_pos.x, card_pos.y + card_h + 4));
            ImGui::Dummy(ImVec2(card_w, 0));

            ImGui::PopID();
        }

        ImGui::EndChild();

        if (hub_selected >= 0 && hub_selected < (int)hub_projects.size()) {
            if (ImGui::Button(lang.word("open_selected"), ImVec2(140, 30))) {
                result_path = hub_projects[hub_selected].path;
                should_exit = true;
            }
        }

        ImGui::End();

        if (hub_show_create) { ImGui::OpenPopup(lang.word("create_project")); hub_show_create = false; }

        ImGui::SetNextWindowSize(ImVec2(460, 182), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(lang.word("create_project"), nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            ImGui::Text(lang.word("project_name"));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##cname", hub_create_name, sizeof(hub_create_name));

            ImGui::Spacing();
            ImGui::Text(lang.word("location"));
            ImGui::SetNextItemWidth(-64);
            ImGui::InputText("##cpath", hub_create_path, sizeof(hub_create_path));
            ImGui::SameLine();

            if (ImGui::Button("Browse", ImVec2(56, 0))) {
                std::string picked = hub_browse_folder();
                if (!picked.empty())
                    strncpy(hub_create_path, picked.c_str(), sizeof(hub_create_path) - 1);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool can_create = hub_create_name[0] != '\0' && hub_create_path[0] != '\0';
            if (!can_create) ImGui::BeginDisabled();

            if (ImGui::Button(lang.word("create"), ImVec2(110, 30))) {
                hub_create_project(hub_create_name, hub_create_path);
                hub_refresh();
                ImGui::CloseCurrentPopup();
            }

            if (!can_create) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(lang.word("cancel"), ImVec2(110, 30))) ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (hub_show_rename) { ImGui::OpenPopup(lang.word("rename_project")); hub_show_rename = false; }

        ImGui::SetNextWindowSize(ImVec2(380, 130), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(lang.word("rename_project"), nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            ImGui::Text(lang.word("new_name"));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rname", hub_rename_buf, sizeof(hub_rename_buf));
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(lang.word("rename"), ImVec2(110, 28))) {
                if (hub_rename_index >= 0 && hub_rename_buf[0] != '\0') {
                    hub_rename_project(hub_projects[hub_rename_index].path, hub_rename_buf);
                    hub_refresh();
                    hub_selected = -1;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(lang.word("cancel"), ImVec2(110, 28))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (hub_show_delete) { ImGui::OpenPopup(lang.word("delete_project")); hub_show_delete = false; }

        ImGui::SetNextWindowSize(ImVec2(380, 105), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(lang.word("delete_project"), nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            if (hub_selected >= 0 && hub_selected < (int)hub_projects.size())
                ImGui::Text(lang.word("delete_project_ask"),
                    hub_projects[hub_selected].name.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(lang.word("delete"), ImVec2(110, 28))) {
                if (hub_selected >= 0) {
                    hub_delete_project(hub_projects[hub_selected].path);
                    hub_refresh();
                    hub_selected = -1;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(lang.word("cancel"), ImVec2(110, 28))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (hub_show_version_warning) { ImGui::OpenPopup(lang.word("version_mismatch")); hub_show_version_warning = false; }

        ImGui::SetNextWindowSize(ImVec2(480, 155), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(lang.word("version_mismatch"), nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            ImGui::TextWrapped(lang.word("version_mismatch_msg"), hub_saved_version.c_str(), QUARK_ENGINE_VERSION.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(lang.word("open_anyway"), ImVec2(130, 28))) {
                result_path = hub_pending_open_path;
                should_exit = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(lang.word("cancel"), ImVec2(110, 28))) {
                hub_pending_open_path.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        hub_draw_plugin_manager();
        rlImGuiEnd();
        EndDrawing();
    }

    return result_path;
}
