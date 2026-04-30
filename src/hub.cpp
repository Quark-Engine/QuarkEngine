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
#include "project.h"
#include "rlImGui.h"
#include "imgui.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char* HUB_PROJECTS_ROOT = "projects";
static const char* HUB_REGISTRY_FILE = "projects.json";

struct HubProject {
    std::string name;
    std::string path;
};

static std::vector<HubProject> hub_projects;
static int  hub_selected       = -1;
static int  hub_rename_index   = -1;
static bool hub_show_create    = false;
static bool hub_show_rename    = false;
static bool hub_show_delete    = false;
static char hub_create_name[256] = "";
static char hub_create_path[512] = "";
static char hub_rename_buf[256]  = "";

static void hub_save_registry() {
    json j = json::array();

    for (auto& p : hub_projects)
        j.push_back( { {"name", p.name}, {"path", p.path} } );
    
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
            for (auto& entry : j) {
                std::string path = entry["path"];
                if (project_is_valid(path)) {
                    HubProject p;
                    p.name = entry["name"];
                    p.path = project_resolve_root(path);
                    hub_projects.push_back(p);
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
    bi.lpszTitle = "Select Project Location";
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
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
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
        ImGui::Begin("##hub", nullptr,
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::Text("QUARK HUB");
        ImGui::SameLine();
        ImGui::TextDisabled("  Project Manager");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 262);

        if (ImGui::Button("Import Project", ImVec2(120, 28))) {
            std::string picked = hub_browse_project_file();
            if (!picked.empty()) {
                hub_import_project(picked);
                hub_refresh();
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("+ Create Project", ImVec2(134, 28))) {
            memset(hub_create_name, 0, sizeof(hub_create_name));
            strncpy(hub_create_path, HUB_PROJECTS_ROOT, sizeof(hub_create_path) - 1);
            hub_show_create = true;
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("##list", ImVec2(0, (float)GetScreenHeight() - 90), false);

        if (hub_projects.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* msg = "No projects yet. Click \"+ Create Project\" to get started.";
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
                result_path = hub_projects[i].path;
                should_exit = true;
            }

            if (ImGui::BeginPopupContextItem("##ctx")) {
                if (ImGui::MenuItem("Open")) {
                    result_path = hub_projects[i].path;
                    should_exit = true;
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Rename")) {
                    hub_rename_index = i;
                    strncpy(hub_rename_buf, hub_projects[i].name.c_str(), sizeof(hub_rename_buf) - 1);
                    hub_show_rename = true;
                }

                if (ImGui::MenuItem("Delete")) {
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
            if (ImGui::Button("Open Selected", ImVec2(140, 30))) {
                result_path = hub_projects[hub_selected].path;
                should_exit = true;
            }
        }

        ImGui::End();

        if (hub_show_create) { ImGui::OpenPopup("Create Project"); hub_show_create = false; }

        ImGui::SetNextWindowSize(ImVec2(460, 182), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Create Project", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            ImGui::Text("Project Name");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##cname", hub_create_name, sizeof(hub_create_name));

            ImGui::Spacing();
            ImGui::Text("Location");
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

            if (ImGui::Button("Create", ImVec2(110, 30))) {
                hub_create_project(hub_create_name, hub_create_path);
                hub_refresh();
                ImGui::CloseCurrentPopup();
            }

            if (!can_create) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110, 30))) ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (hub_show_rename) { ImGui::OpenPopup("Rename Project"); hub_show_rename = false; }

        ImGui::SetNextWindowSize(ImVec2(380, 130), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Rename Project", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            ImGui::Text("New Name");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rname", hub_rename_buf, sizeof(hub_rename_buf));
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Rename", ImVec2(110, 28))) {
                if (hub_rename_index >= 0 && hub_rename_buf[0] != '\0') {
                    hub_rename_project(hub_projects[hub_rename_index].path, hub_rename_buf);
                    hub_refresh();
                    hub_selected = -1;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110, 28))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (hub_show_delete) { ImGui::OpenPopup("Delete Project"); hub_show_delete = false; }

        ImGui::SetNextWindowSize(ImVec2(380, 105), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Delete Project", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::Spacing();
            if (hub_selected >= 0 && hub_selected < (int)hub_projects.size())
                ImGui::Text("Delete \"%s\"? This cannot be undone.",
                    hub_projects[hub_selected].name.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Delete", ImVec2(110, 28))) {
                if (hub_selected >= 0) {
                    hub_delete_project(hub_projects[hub_selected].path);
                    hub_refresh();
                    hub_selected = -1;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110, 28))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        rlImGuiEnd();
        EndDrawing();
    }

    return result_path;
}
