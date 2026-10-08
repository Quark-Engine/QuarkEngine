#include "editor/editor_assets.h"

#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "qcImGui.h"
#include "project.h"
#include "tex.h"
#include "language_manager.h"
#include "editor/editor_preferences.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER

    #include <windows.h>
    #include <shellapi.h>

    #undef CloseWindow
    #undef ShowCursor
    #undef Rectangle
#endif

#define lang CLanguageManager::Get()
namespace fs = std::filesystem;

namespace
{

constexpr float kIconSize = 64.0f;

std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool MatchesAssetFilter(const SLocalEntry& entry, int filter, const std::string& search)
{
    if (entry.IsDirectory)
    {
        return filter == 0 &&
            (search.empty() || Lowercase(entry.FileName).find(search) != std::string::npos);
    }

    bool matchesType = false;
    switch (filter)
    {
    case 0:
        matchesType = !entry.IsTextureMeta;
        break;
    case 1:
        matchesType = entry.IsImage || entry.IsModel;
        break;
    case 2:
        matchesType = entry.IsMaterial;
        break;
    case 3:
        matchesType = entry.IsTextureMeta;
        break;
    case 4:
        matchesType = entry.isPrefab;
        break;
    default:
        matchesType = true;
        break;
    }

    if (!matchesType)
    {
        return false;
    }

    return search.empty() || Lowercase(entry.FileName).find(search) != std::string::npos;
}

void OpenInSystemFileExplorer(const fs::path& path, bool bIsDirectory)
{
#ifdef _WIN32
    if (bIsDirectory)
    {
        ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, 1);
    }
    else
    {
        const std::wstring arguments = L"/select,\"" + path.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, 1);
    }
#elif defined(__APPLE__)
    const std::string command = "open \"" + path.string() + "\"";
    std::system(command.c_str());
#elif defined(__linux__)
    const std::string command = "xdg-open \"" + path.string() + "\"";
    std::system(command.c_str());
#endif
}

} // anonymous

static RenderTexture2D CreateModelPreview(const CModelAsset& asset, int previewSize)
{
    RenderTexture2D renderTexture = { 0 };

    Model previewModel;
    if (!CModelService::LoadInstance(asset, previewModel))
    {
        return renderTexture;
    }
    if (!HasValidModelData(previewModel))
    {
        UnloadModel(previewModel);
        return renderTexture;
    }

    renderTexture = LoadRenderTexture(previewSize, previewSize);
    if (renderTexture.id == 0)
    {
        UnloadModel(previewModel);
        return renderTexture;
    }

    Vec3 minBound = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vec3 maxBound = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool hasVertices = false;

    for (int meshIndex = 0; meshIndex < previewModel.meshCount; meshIndex++)
    {
        Mesh& mesh = previewModel.meshes[meshIndex];
        if (!mesh.vertices)
        {
            continue;
        }

        for (int vertexIndex = 0; vertexIndex < mesh.vertexCount; vertexIndex++)
        {
            const float vx = mesh.vertices[vertexIndex * 3 + 0];
            const float vy = mesh.vertices[vertexIndex * 3 + 1];
            const float vz = mesh.vertices[vertexIndex * 3 + 2];

            minBound.x = fminf(minBound.x, vx);
            minBound.y = fminf(minBound.y, vy);
            minBound.z = fminf(minBound.z, vz);
            maxBound.x = fmaxf(maxBound.x, vx);
            maxBound.y = fmaxf(maxBound.y, vy);
            maxBound.z = fmaxf(maxBound.z, vz);
            hasVertices = true;
        }
    }

    Vec3 center = {0, 0, 0};
    float distance = 3.0f;
    if (hasVertices)
    {
        center = {
            (minBound.x + maxBound.x) * 0.5f,
            (minBound.y + maxBound.y) * 0.5f,
            (minBound.z + maxBound.z) * 0.5f
        };

        const Vec3 size = {
            maxBound.x - minBound.x,
            maxBound.y - minBound.y,
            maxBound.z - minBound.z
        };

        float maxSize = fmaxf(fmaxf(size.x, size.y), size.z);
        if (maxSize < 0.1f)
        {
            maxSize = 1.0f;
        }
        distance = maxSize * 2.0f;
    }

    Camera3D camera = {};
    camera.position = { center.x + distance * 0.6f, center.y + distance * 0.5f, center.z + distance * 0.6f };
    camera.target = center;
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(renderTexture);
    ClearBackground({ 32, 32, 40, 255 });
    BeginMode3D(camera);
    DrawModel(previewModel, { 0, 0, 0 }, 1.0f, WHITE);
    EndMode3D();
    EndTextureMode();

    UnloadModel(previewModel);
    return renderTexture;
}

bool ImportPathToResources(const fs::path& src, const fs::path& resourceDir)
{
    std::error_code ec;

    if (!fs::exists(src, ec) || ec)
    {
        TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Dropped path does not exist: %s", src.string().c_str()));
        return false;
    }

    if (fs::is_regular_file(src, ec))
    {
        fs::copy_file(src, resourceDir / src.filename(), fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Failed to import file: %s", src.string().c_str()));
            return false;
        }
        return true;
    }

    if (fs::is_directory(src, ec))
    {
        bool importedAny = false;
        const fs::path dstRoot = resourceDir / src.filename();
        fs::create_directories(dstRoot, ec);
        ec.clear();

        fs::recursive_directory_iterator iterator(src, fs::directory_options::skip_permission_denied, ec);
        if (ec)
        {
            TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Failed to open dropped directory: %s", src.string().c_str()));
            return false;
        }

        for (const auto& entry : iterator)
        {
            if (!entry.is_regular_file(ec) || ec)
            {
                ec.clear();
                continue;
            }

            const fs::path relative = fs::relative(entry.path(), src, ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            const fs::path dst = dstRoot / relative;
            fs::create_directories(dst.parent_path(), ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            importedAny = true;
        }

        return importedAny;
    }

    TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Unsupported dropped path: %s", src.string().c_str()));
    return false;
}

static RenderTexture2D CreateMaterialPreview(CEditor& editor, const std::string& mtlPath)
{
    std::ifstream file(mtlPath);
    if (!file.is_open())
    {
        return {0};
    }

    Model sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));

    Color albedo = WHITE;
    Texture2D tex = {0};
    std::string texPath;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "Kd")
        {
            float r=1,g=1,b=1;
            ss >> r >> g >> b;
            albedo = {
                (unsigned char)(r * 255),
                (unsigned char)(g * 255),
                (unsigned char)(b * 255),
                255
            };
        }
        else if (type == "map_Kd")
        {
            std::getline(ss >> std::ws, texPath);
        }
    }

    Material& mat = sphere.materials[0];

    mat.maps[MATERIAL_MAP_DIFFUSE].color = albedo;

    if (!texPath.empty())
    {
        const fs::path full = fs::path(mtlPath).parent_path() / fs::path(texPath);
        const Texture2D* pTexture = editor.m_Textures.Load(full.string());
        if (pTexture)
        {
            tex = *pTexture;
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
        }
    }

    RenderTexture2D rt = LoadRenderTexture(128, 128);
    if (rt.id == 0)
    {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = {0};
        UnloadModel(sphere);
        return rt;
    }

    Camera3D cam = {};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    cam.target = {0, 0, 0};
    cam.up = {0, 1, 0};
    cam.position = {2, 2, 2};

    BeginTextureMode(rt);
    ClearBackground({40, 40, 45, 255});
    BeginMode3D(cam);

    DrawModel(sphere, {0, 0, 0}, 1.0f, WHITE);
    DrawModelWires(sphere, {0, 0, 0}, 1.0f, DARKGRAY);

    EndMode3D();
    EndTextureMode();

    sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
    UnloadModel(sphere);
    return rt;
}

static Texture GetModelPreview(CEditor& editor, const CModelAsset& asset, const std::string& cacheKey)
{
    if (editor.m_Previews.HasModelPreview(cacheKey))
    {
        return editor.m_Previews.ModelPreview(cacheKey);
    }

    const RenderTexture2D renderTexture = CreateModelPreview(asset, editor.m_Preferences.m_AssetPreviewSize);
    if (renderTexture.id == 0)
    {
        return { 0 };
    }

    editor.m_Previews.StoreModelPreview(cacheKey, renderTexture);
    return renderTexture.texture;
}

static Texture GetMaterialPreview(CEditor& editor, const std::string& mtlPath)
{
    if (editor.m_Previews.HasMaterialPreview(mtlPath))
    {
        return editor.m_Previews.MaterialPreview(mtlPath);
    }

    const RenderTexture2D renderTexture = CreateMaterialPreview(editor, mtlPath);
    if (renderTexture.id == 0)
    {
        return { 0 };
    }

    editor.m_Previews.StoreMaterialPreview(mtlPath, renderTexture);
    return renderTexture.texture;
}

void DrawAssetsUi(CEditor& editor)
{
    ImGui::Begin(lang.Word("assets"), &editor.m_Preferences.m_ShowAssets, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    editor.m_Previews.EnsureIcons();

    std::string popupName = std::string(lang.Word("create_new")) + "##CreateNew";

    SAssetBrowserState& browser = editor.m_Ui.m_AssetBrowser;
    SAssetBrowserMarquee& marquee = browser.Marquee;
    SAssetBrowserDrag& drag = browser.Drag;
    SAssetBrowserCreatePopup& create = browser.Create;


    if (editor.m_CurrentAssetPath.empty() && !editor.m_ProjectPath.empty())
    {
        editor.m_CurrentAssetPath = fs::path(editor.m_ProjectPath) / "resources";
        std::error_code ec;
        fs::create_directories(editor.m_CurrentAssetPath, ec);
    }

    const ImVec2 windowSize = ImGui::GetWindowSize();
    const fs::path projectRoot = fs::path(editor.m_ProjectPath);
    const fs::path relativePath = fs::relative(editor.m_CurrentAssetPath, projectRoot.parent_path());

    std::vector<fs::path> vCrumbs;
    for (const auto& part : relativePath)
    {
        vCrumbs.push_back(part);
    }

    fs::path rebuilt = projectRoot.parent_path();
    for (int index = 0; index < static_cast<int>(vCrumbs.size()); index++)
    {
        rebuilt /= vCrumbs[index];
        const std::string label = vCrumbs[index].string() + "/";

        ImGui::PushID(index);

        const ImVec2 btnMin = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::SmallButton(label.c_str());
        const ImVec2 btnMax = ImVec2(
            btnMin.x + ImGui::GetItemRectSize().x,
            btnMin.y + ImGui::GetItemRectSize().y
        );

        ImGui::PopID();

        if (editor.m_Ui.m_Layout.FileDragging && ImGui::IsMouseHoveringRect(btnMin, btnMax))
        {
            ImGui::GetWindowDrawList()->AddRectFilled(
                btnMin,
                btnMax,
                IM_COL32(100, 200, 100, 120)
            );

            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (editor.m_Ui.m_Layout.FileDragging && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(btnMin, btnMax) && !drag.FilePath.empty())
        {
            fs::path dest = rebuilt / drag.FilePath.filename();

            if (fs::exists(dest))
            {
                browser.DuplicatePopupOpen = true;
                browser.DuplicateName = drag.FilePath.filename().string();
            }
            else
            {
                std::error_code ec;
                fs::rename(drag.FilePath, dest, ec);

                if (!ec)
                {
                    if (CTextureMetadataStore::IsImageFile(drag.FilePath) && fs::exists(drag.FilePath.string() + ".meta"))
                    {
                        fs::rename(drag.FilePath.string() + ".meta", dest.string() + ".meta", ec);
                    }
                    editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                    editor.m_SelectedAssetIndex = -1;
                }
            }

            editor.m_Ui.m_Layout.FileDragging = false;
            drag.FilePath.clear();
            editor.m_Ui.m_Layout.DraggedFileIndex = -1;
        }

        if (clicked)
        {
            editor.m_CurrentAssetPath = rebuilt;
            editor.m_SelectedAssetIndex = -1;
            editor.m_SelectedAssetName.clear();

            editor.m_Previews.InvalidateModelPreviews();
        }
        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    const char* apAssetFilterNames[] = {
        lang.Word("all"), lang.Word("images_models"), lang.Word("materials"),
        lang.Word("texture_metadata"), lang.Word("prefabs")
    };
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("##asset_type_filter_browser", &editor.m_Preferences.m_AssetFilter,
        apAssetFilterNames, IM_ARRAYSIZE(apAssetFilterNames)))
    {
        editor.m_Preferences.Save();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##asset_search", lang.Word("search_assets"), browser.aSearchBuffer,
        IM_ARRAYSIZE(browser.aSearchBuffer));

    std::vector<SLocalEntry> vDirectories;
    std::vector<SLocalEntry> vFiles;
    const std::string search = Lowercase(browser.aSearchBuffer);
    std::error_code dirError;
    for (const auto& path : fs::directory_iterator(editor.m_CurrentAssetPath, dirError))
    {
        SLocalEntry entry = {};
        entry.FileName = path.path().filename().string();
        entry.IsDirectory = path.is_directory();
        entry.IsImage = CTextureMetadataStore::IsImageFile(path.path());
        entry.IsModel = CModelService::IsModelFile(path.path());

        std::string ext = path.path().extension().string();
        if (!ext.empty() && ext[0] == '.')
        {
            ext.erase(ext.begin());
        }
        ext = Lowercase(std::move(ext));
        entry.Extension = ext;
        entry.IsMaterial = entry.Extension == "mtl";
        entry.IsTextureMeta = entry.Extension == "meta" && CTextureMetadataStore::IsImageFile(path.path().stem());
        if (entry.IsTextureMeta && !fs::exists(path.path().parent_path() / path.path().stem()))
        {
            continue;
        }
        entry.isPrefab = entry.Extension == "prefab";
        if (entry.Extension == "meta" && !entry.IsTextureMeta)
        {
            continue;
        }
        if (!MatchesAssetFilter(entry, editor.m_Preferences.m_AssetFilter, search))
        {
            continue;
        }

        if (entry.IsDirectory)
        {
            vDirectories.push_back(entry);
        }
        else
        {
            vFiles.push_back(entry);
        }
    }

    std::vector<SLocalEntry> vEntries;
    vEntries.insert(vEntries.end(), vDirectories.begin(), vDirectories.end());
    vEntries.insert(vEntries.end(), vFiles.begin(), vFiles.end());

    if (!editor.m_SelectedAssetName.empty())
    {
        const auto selectedEntry = std::find_if(vEntries.begin(), vEntries.end(),
            [&editor](const SLocalEntry& entry)
            {
                return entry.FileName == editor.m_SelectedAssetName;
            });
        editor.m_SelectedAssetIndex = selectedEntry == vEntries.end()
            ? -1
            : static_cast<int>(std::distance(vEntries.begin(), selectedEntry));
        std::error_code selectedError;
        if (!fs::exists(editor.m_CurrentAssetPath / editor.m_SelectedAssetName, selectedError) ||
            selectedError)
        {
            editor.m_SelectedAssetName.clear();
            editor.m_SelectedAssetIndex = -1;
        }
    }

    if (vEntries.empty())
    {
        ImGui::BeginChild("AssetScrollEmpty", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 1.0f)
        {
            avail.x = 1.0f;
        }
        if (avail.y < 1.0f)
        {
            avail.y = 1.0f;
        }

        ImGui::InvisibleButton("##empty_drop_zone", avail);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
            {
                int idx = *(const int*)pPayload->Data;
                CEntity e = editor.m_Scene.m_vEntities[idx];

                CEntityFactory::SavePrefab(e, editor.m_CurrentAssetPath);
            }

            ImGui::EndDragDropTarget();
        }

        const char* pText = search.empty() ? lang.Word("empty_folder") : "No matching assets.";
        const ImVec2 ts   = ImGui::CalcTextSize(pText);
        const ImVec2 wp   = ImGui::GetWindowPos();
        const ImVec2 ws   = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(wp.x + (ws.x - ts.x) * 0.5f, wp.y + (ws.y - ts.y) * 0.5f),
            IM_COL32(255, 255, 255, 255),
            pText
        );

        if (ImGui::BeginPopupContextItem("AssetBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem(lang.Word("new_file")))
            {
                create.CreatingFolder = false;
                create.aNewItemName[0] = '\0';
                create.Open = true;
            }

            if (ImGui::MenuItem(lang.Word("new_folder")))
            {
                create.CreatingFolder = true;
                create.aNewItemName[0] = '\0';
                create.Open = true;
            }

            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }

    ImGui::BeginChild("AssetScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (ImGui::IsWindowHovered() && !editor.m_Ui.m_Layout.FileDragging)
    {
        if (ImGui::IsMouseClicked(0))
        {
            marquee.Start = ImGui::GetMousePos();
            marquee.End = marquee.Start;
            marquee.Active = true;
        }
        if (ImGui::IsMouseDown(0) && marquee.Active)
        {
            marquee.End = ImGui::GetMousePos();
        }
        if (ImGui::IsMouseReleased(0))
        {
            marquee.Active = false;
        }
    }

    const float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    editor.m_Ui.m_Layout.DraggedTargetFolderIndex = -1;

    bool navigated = false;
    for (int i = 0; i < static_cast<int>(vEntries.size()); i++)
    {
        auto& entry = vEntries[i];
        ImGui::PushID(i);
        ImGui::BeginGroup();

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(kIconSize, kIconSize + 20.0f);
        ImGui::InvisibleButton("asset_btn", size);

        const bool itemActive = ImGui::IsItemActive();
        const bool itemHovered = ImGui::IsItemHovered();
        if (itemHovered)
        {
            ImGui::SetTooltip("%s", entry.FileName.c_str());
        }
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0))
        {
            editor.m_SelectedAssetIndex = i;
            editor.m_SelectedAssetName = entry.FileName;
            editor.m_Scene.m_Selected = -1;
            editor.m_Scene.m_vSelectedEntities.clear();
        }

        if (entry.IsDirectory && itemHovered && ImGui::IsMouseDoubleClicked(0))
        {
            editor.m_CurrentAssetPath /= entry.FileName;
            editor.m_SelectedAssetIndex = -1;
            editor.m_SelectedAssetName.clear();
            editor.m_Previews.InvalidateModelPreviews();

            ImGui::EndGroup();
            ImGui::PopID();
            navigated = true;
            break;
        }

        if (!entry.IsDirectory && itemHovered && ImGui::IsMouseDoubleClicked(0))
        {
            const fs::path fullPath = editor.m_CurrentAssetPath / entry.FileName;
            if (entry.IsModel)
            {
                CModelAsset* pAsset = editor.m_Assets.FindModelByPath(fullPath, editor.m_ProjectPath);
                if (pAsset)
                {
                    OpenModelViewerForAsset(editor.m_Ui.m_ModelViewer, *pAsset);
                }
            }
            else if (entry.IsMaterial)
            {
                OpenMaterialViewerForPath(editor, editor.m_Ui.m_MaterialViewer, fullPath);
            }
            else
            {
                #ifdef _WIN32
                    ShellExecuteA(nullptr, "open", fullPath.string().c_str(), nullptr, nullptr, 1);
                #elif defined(__APPLE__)
                    std::string cmd = "open \"" + fullPath.string() + "\"";
                    std::system(cmd.c_str());
                #elif defined(__linux__)
                    std::string cmd = "xdg-open \"" + fullPath.string() + "\"";
                    std::system(cmd.c_str());
                #endif
            }
        }

        if (ImGui::BeginPopupContextItem("AssetContext"))
        {
            if (ImGui::MenuItem(lang.Word("open_file_explorer")))
            {
                OpenInSystemFileExplorer(editor.m_CurrentAssetPath / entry.FileName, entry.IsDirectory);
            }

            ImGui::Separator();

            if (ImGui::MenuItem(lang.Word("delete")))
            {
                editor.SaveState();

                const fs::path target = editor.m_CurrentAssetPath / entry.FileName;
                std::error_code ec;
                if (entry.IsDirectory)
                {
                    fs::remove_all(target, ec);
                }
                else
                {
                    fs::remove(target, ec);
                }
                if (entry.IsMaterial)
                {
                    fs::remove(target.string() + ".meta", ec);
                }
                if (entry.IsTextureMeta)
                {
                    fs::remove(CTextureMetadataStore::PathFromMeta(target), ec);
                }

                editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                editor.m_SelectedAssetIndex = -1;
                editor.m_SelectedAssetName.clear();

                ImGui::EndPopup();
                ImGui::EndGroup();
                ImGui::PopID();
                break;
            }

            if (ImGui::MenuItem(lang.Word("rename")))
            {
                browser.RenameTarget = i;
                const size_t copied = entry.FileName.copy(editor.m_Ui.m_MeshEdit.aRenameBuffer, sizeof(editor.m_Ui.m_MeshEdit.aRenameBuffer) - 1);
                editor.m_Ui.m_MeshEdit.aRenameBuffer[copied] = '\0';
                ImGui::OpenPopup(("%s##RenameAsset", lang.Word("rename_asset")));
            }

            ImGui::Separator();

            if (ImGui::MenuItem(lang.Word("new_file")))
            {
                create.CreatingFolder = false;
                create.aNewItemName[0] = '\0';
                create.Open = true;
            }

            if (ImGui::MenuItem(lang.Word("new_folder")))
            {
                create.CreatingFolder = true;
                create.aNewItemName[0] = '\0';
                create.Open = true;
            }

            ImGui::EndPopup();
        }

        bool selected = editor.m_SelectedAssetIndex == i;
        if (marquee.Active && (fabsf(marquee.Start.x - marquee.End.x) > 5.0f || fabsf(marquee.Start.y - marquee.End.y) > 5.0f))
        {
            const ImVec2 min(std::min(marquee.Start.x, marquee.End.x), std::min(marquee.Start.y, marquee.End.y));
            const ImVec2 max(std::max(marquee.Start.x, marquee.End.x), std::max(marquee.Start.y, marquee.End.y));
            if (!(pos.x + size.x < min.x || pos.x > max.x || pos.y + size.y < min.y || pos.y > max.y))
            {
                selected = true;
            }
        }

        if (selected)
        {
            ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(80, 140, 255, 100));
        }

        ImGui::SetCursorScreenPos(pos);
        if (entry.IsDirectory)
        {
            const Texture texture = fs::is_empty(editor.m_CurrentAssetPath / entry.FileName) ? editor.m_Previews.IconFolder() : editor.m_Previews.IconFullFolder();
            if (texture.id != 0)
            {
                QcImGuiImage(&texture, ImVec2(kIconSize, kIconSize));
            }
            else
            {
                ImGui::Button(lang.Word("folder"), ImVec2(kIconSize, kIconSize));
            }
        }
        else if (entry.IsImage)
        {
            const std::string full = (editor.m_CurrentAssetPath / entry.FileName).string();
            const Texture2D* pTexture = nullptr;
            if (ImGui::IsRectVisible(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize)))
            {
                pTexture = editor.m_Textures.Load(full);
            }
            else
            {
                pTexture = editor.m_Textures.Find(full);
            }
            if (pTexture)
            {
                QcImGuiImage(pTexture, ImVec2(kIconSize, kIconSize));
            }
        }
        else if (entry.IsModel)
        {
            const std::string full = (editor.m_CurrentAssetPath / entry.FileName).string();
            Texture previewTexture = {0};
            bool loadFailed = false;

            if (editor.m_Previews.HasModelPreview(full))
            {
                previewTexture = editor.m_Previews.ModelPreview(full);
            }
            else if (ImGui::IsRectVisible(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize)))
            {
                CModelAsset* pAsset = editor.m_Assets.FindModelByPath(editor.m_CurrentAssetPath / entry.FileName, editor.m_ProjectPath);
                if (pAsset)
                {
                    previewTexture = GetModelPreview(editor, *pAsset, full);
                    if (previewTexture.id == 0)
                    {
                        loadFailed = true;
                    }
                }
                else
                {
                    loadFailed = true;
                }
            }

            if (previewTexture.id != 0)
            {
                QcImGuiImage(&previewTexture, ImVec2(kIconSize, kIconSize));
            }
            else
            {
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                pDrawList->AddRectFilled(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize), loadFailed ? IM_COL32(80, 80, 90, 255) : IM_COL32(100, 100, 120, 255));
                if (loadFailed)
                {
                    pDrawList->AddLine(ImVec2(pos.x + 10, pos.y + 10), ImVec2(pos.x + kIconSize - 10, pos.y + kIconSize - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                    pDrawList->AddLine(ImVec2(pos.x + kIconSize - 10, pos.y + 10), ImVec2(pos.x + 10, pos.y + kIconSize - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                }
            }
        }
        else if (entry.IsMaterial)
        {
            const std::string full = (editor.m_CurrentAssetPath / entry.FileName).string();

            Texture preview = {0};

            if (editor.m_Previews.HasMaterialPreview(full))
            {
                preview = editor.m_Previews.MaterialPreview(full);
            }
            else if (ImGui::IsRectVisible(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize)))
            {
                preview = GetMaterialPreview(editor, full);
            }

            if (preview.id != 0)
            {
                QcImGuiImage(&preview, ImVec2(kIconSize, kIconSize));
            }
            else
            {
                ImGui::Button("MAT", ImVec2(kIconSize, kIconSize));
            }
        }
        else
        {
            if (editor.m_Previews.IconFile().id != 0)
            {
                QcImGuiImage(&editor.m_Previews.IconFile(), ImVec2(kIconSize, kIconSize));
            }
            else
            {
                ImGui::Button(entry.FileName.empty() ? "file" : entry.FileName.c_str(), ImVec2(kIconSize, kIconSize));
            }
        }

        if (!entry.IsDirectory && !entry.FileName.empty())
        {
            ImDrawList* pDrawList = ImGui::GetWindowDrawList();
            std::string extDisplay = entry.Extension;
            if (extDisplay.size() > 3)
            {
                extDisplay.resize(3);
            }
            const ImVec2 extTextSize = ImGui::CalcTextSize(extDisplay.c_str());
            const ImVec2 extPos(pos.x + kIconSize - extTextSize.x - 2.0f, pos.y + kIconSize - extTextSize.y - 2.0f);
            pDrawList->AddRectFilled(ImVec2(extPos.x - 2.0f, extPos.y - 1.0f), ImVec2(pos.x + kIconSize, pos.y + kIconSize), IM_COL32(0, 0, 0, 180));
            pDrawList->AddText(extPos, IM_COL32(255, 255, 255, 255), extDisplay.c_str());
        }

        if (itemHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editor.m_Ui.m_Layout.FileDragging)
        {
            drag.FileName = entry.FileName;
            drag.StartPos = ImGui::GetMousePos();

            editor.m_Ui.m_Layout.DraggedFileIndex = i;
            drag.FilePath = editor.m_CurrentAssetPath / entry.FileName;
        }

        if (drag.FileName == entry.FileName && ImGui::IsMouseDown(0))
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 delta = ImVec2(mousePos.x - drag.StartPos.x, mousePos.y - drag.StartPos.y);
            if (fabsf(delta.x) > 5.0f || fabsf(delta.y) > 5.0f)
            {
                editor.m_Ui.m_Layout.FileDragging = true;
                editor.m_Ui.m_Layout.DraggedFileIndex = i;

                if (entry.IsModel)
                {
                    editor.m_Ui.m_Layout.SceneAssetDragging = true;
                    editor.m_Ui.m_Layout.DraggedSceneAssetName = (editor.m_CurrentAssetPath / entry.FileName).string();
                }

                if (entry.Extension == "prefab")
                {
                    editor.m_Ui.m_Layout.SceneAssetDragging = true;
                    editor.m_Ui.m_Layout.DraggedSceneAssetName = (editor.m_CurrentAssetPath / entry.FileName).string();
                }

                if (CModelService::IsModelFile(fs::path(entry.FileName)))
                {
                    const std::string assetName = CAssetLibrary::AssetNameForPath(fs::path(editor.m_ProjectPath), editor.m_CurrentAssetPath / entry.FileName);
                    editor.m_Ui.m_Layout.SceneAssetDragging = true;
                    editor.m_Ui.m_Layout.DraggedSceneAssetName = assetName;
                }
            }
        }

        const bool hasValidDraggedFile = editor.m_Ui.m_Layout.DraggedFileIndex >= 0 &&
            editor.m_Ui.m_Layout.DraggedFileIndex < static_cast<int>(vEntries.size());
        if (entry.IsDirectory && editor.m_Ui.m_Layout.FileDragging && hasValidDraggedFile &&
            vEntries[editor.m_Ui.m_Layout.DraggedFileIndex].FileName != entry.FileName)
            {
            ImVec2 mousePos = ImGui::GetMousePos();
            bool mouseOverFolder = mousePos.x >= pos.x && mousePos.x <= pos.x + size.x &&
                                    mousePos.y >= pos.y && mousePos.y <= pos.y + size.y;

            if (mouseOverFolder)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(100, 200, 100, 150));
                editor.m_Ui.m_Layout.DraggedTargetFolderIndex = i;
            }
        }

        if (!entry.IsDirectory && (CModelService::IsModelFile(fs::path(entry.FileName)) || entry.IsMaterial))
        {
            const std::string assetName = CAssetLibrary::AssetNameForPath(fs::path(editor.m_ProjectPath), editor.m_CurrentAssetPath / entry.FileName);

            if (editor.m_Ui.m_Layout.SceneAssetDragging && editor.m_Ui.m_Layout.DraggedSceneAssetName == assetName)
            {
                if (CModelService::IsModelFile(fs::path(entry.FileName)))
                {
                    ImGui::SetTooltip(lang.Word("spawn"), editor.m_Ui.m_Layout.DraggedSceneAssetName.c_str());
                }
            }
        }

        std::string label = entry.FileName;
        if (label.size() > 10)
        {
            label = label.substr(0, 8) + "..";
        }
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        ImGui::SetCursorScreenPos(ImVec2(pos.x + (kIconSize - labelSize.x) * 0.5f, pos.y + kIconSize + 2.0f));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndGroup();

        const float nextX2 = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + kIconSize;
        if (i + 1 < static_cast<int>(vEntries.size()) && nextX2 < windowVisibleX2)
        {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }

    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImVec2 bgSize = ImGui::GetContentRegionMax();
    if (bgSize.x < 1.f)
    {
        bgSize.x = 1.f;
    }
    if (bgSize.y < 1.f)
    {
        bgSize.y = 1.f;
    }

    ImGui::InvisibleButton("##bg_drop_zone", bgSize);

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
        {
            int idx = *(const int*)pPayload->Data;
            CEntity e = editor.m_Scene.m_vEntities[idx];

            CEntityFactory::SavePrefab(e, editor.m_CurrentAssetPath);
        }
        ImGui::EndDragDropTarget();
    }

    if (marquee.Active && (fabsf(marquee.Start.x - marquee.End.x) > 5.0f || fabsf(marquee.Start.y - marquee.End.y) > 5.0f))
    {
        const ImVec2 min(std::min(marquee.Start.x, marquee.End.x), std::min(marquee.Start.y, marquee.End.y));
        const ImVec2 max(std::max(marquee.Start.x, marquee.End.x), std::max(marquee.Start.y, marquee.End.y));
        ImGui::GetForegroundDrawList()->AddRectFilled(min, max, IM_COL32(80, 140, 255, 40));
    }

    if (editor.m_Ui.m_Layout.FileDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (editor.m_Ui.m_Layout.DraggedTargetFolderIndex >= 0 &&
            editor.m_Ui.m_Layout.DraggedFileIndex >= 0 &&
            editor.m_Ui.m_Layout.DraggedFileIndex < static_cast<int>(vEntries.size()) &&
            editor.m_Ui.m_Layout.DraggedTargetFolderIndex < static_cast<int>(vEntries.size()) &&
            editor.m_Ui.m_Layout.DraggedFileIndex != editor.m_Ui.m_Layout.DraggedTargetFolderIndex &&
            vEntries[editor.m_Ui.m_Layout.DraggedTargetFolderIndex].IsDirectory)
            {

            editor.SaveState();
            const fs::path sourcePath = editor.m_CurrentAssetPath / vEntries[editor.m_Ui.m_Layout.DraggedFileIndex].FileName;
            const fs::path destDir = editor.m_CurrentAssetPath / vEntries[editor.m_Ui.m_Layout.DraggedTargetFolderIndex].FileName;
            const fs::path destPath = destDir / fs::path(sourcePath).filename();

            if (fs::exists(destPath))
            {
                browser.DuplicatePopupOpen = true;
                browser.DuplicateName = sourcePath.filename().string();
            }
            else
            {
                editor.SaveState();

                std::error_code ec;
                fs::rename(sourcePath, destPath, ec);

                if (!ec)
                {
                    if (CTextureMetadataStore::IsImageFile(sourcePath) && fs::exists(sourcePath.string() + ".meta"))
                    {
                        fs::rename(sourcePath.string() + ".meta", destPath.string() + ".meta", ec);
                    }
                    editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                    editor.m_SelectedAssetIndex = -1;
                }
            }
        }
        editor.m_Ui.m_Layout.FileDragging = false;
        editor.m_Ui.m_Layout.DraggedFileIndex = -1;
        editor.m_Ui.m_Layout.DraggedTargetFolderIndex = -1;
        drag.FileName.clear();
        drag.StartPos = ImVec2(0, 0);
    }

    if (browser.RenameTarget >= 0)
    {
        ImGui::OpenPopup(("%s##RenameAsset", lang.Word("rename_asset")));
        browser.RenameTarget = -2;
    }

    if (browser.RenameTarget == -2 && ImGui::BeginPopupModal(("%s##RenameAsset", lang.Word("rename_asset")), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("##rename", editor.m_Ui.m_MeshEdit.aRenameBuffer, IM_ARRAYSIZE(editor.m_Ui.m_MeshEdit.aRenameBuffer));

        if (ImGui::Button(lang.Word("ok")))
        {
            const std::string newFilename = editor.m_Ui.m_MeshEdit.aRenameBuffer;
            if (browser.LastAppliedRename != newFilename)
            {
                editor.SaveState();
                browser.LastAppliedRename = newFilename;

                if (editor.m_SelectedAssetIndex >= 0 && editor.m_SelectedAssetIndex < static_cast<int>(vEntries.size()))
                {
                    const fs::path oldPath = editor.m_CurrentAssetPath / vEntries[editor.m_SelectedAssetIndex].FileName;
                    const fs::path newPath = editor.m_CurrentAssetPath / editor.m_Ui.m_MeshEdit.aRenameBuffer;

                    if (editor.m_Ui.m_MeshEdit.aRenameBuffer[0] != '\0' && oldPath != newPath && fs::exists(oldPath))
                    {
                        try
                        {
                            fs::rename(oldPath, newPath);
                            if (CTextureMetadataStore::IsImageFile(oldPath) && fs::exists(oldPath.string() + ".meta"))
                            {
                                fs::rename(oldPath.string() + ".meta", newPath.string() + ".meta");
                            }
                            editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                            editor.m_SelectedAssetIndex = -1;
                            editor.m_SelectedAssetName.clear();
                        } catch (...)
                        {
                        }
                    }
                }
            }

            browser.RenameTarget = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button(lang.Word("cancel")))
        {
            browser.RenameTarget = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupContextItem("AssetBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem(lang.Word("new_file")))
        {
            create.CreatingFolder = false;
            create.aNewItemName[0] = '\0';
            create.Open = true;
        }

        if (ImGui::MenuItem(lang.Word("new_folder")))
        {
            create.CreatingFolder = true;
            create.aNewItemName[0] = '\0';
            create.Open = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        editor.m_Ui.m_Layout.FileDragging = false;
        editor.m_Ui.m_Layout.SceneAssetDragging = false;
        editor.m_Ui.m_Layout.DraggedFileIndex = -1;
        editor.m_Ui.m_Layout.DraggedTargetFolderIndex = -1;
        drag.FileName.clear();
        drag.FilePath.clear();
        drag.StartPos = ImVec2(0, 0);
    }

    if (browser.DuplicatePopupOpen)
    {
        ImGui::OpenPopup(("%s##DuplicateName", lang.Word("move_error")));
        browser.DuplicatePopupOpen = false;
    }

    if (ImGui::BeginPopupModal(("%s##DuplicateName", lang.Word("move_error")), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", lang.Word("unable_to_move"));
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", browser.DuplicateName.c_str());
        ImGui::Spacing();
        ImGui::Text("%s", lang.Word("path_already_exists"));

        ImGui::Spacing();

        if (ImGui::Button(lang.Word("ok"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();

    if (create.Open && !navigated)
    {
        ImGui::OpenPopup(popupName.c_str());
        create.Open = false;
    }

    if (ImGui::BeginPopupModal(popupName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(create.CreatingFolder ? lang.Word("new_folder") : lang.Word("new_file"));
        ImGui::InputText(lang.Word("name"), create.aNewItemName, IM_ARRAYSIZE(create.aNewItemName));
        ImGui::Spacing();

        if (ImGui::Button(lang.Word("ok"), ImVec2(120, 0)))
        {
            if (create.aNewItemName[0] != '\0')
            {
                fs::path path;

                if (create.CreatingFolder)
                {
                    path = editor.m_CurrentAssetPath / create.aNewItemName;

                    int suffix = 1;
                    while (fs::exists(path))
                    {
                        path = editor.m_CurrentAssetPath / (std::string(create.aNewItemName) + "_" + std::to_string(suffix++));
                    }

                    std::error_code ec;
                    fs::create_directory(path, ec);
                }
                else
                {
                    std::string filename = create.aNewItemName;
                    if (filename.find('.') == std::string::npos)
                    {
                        filename += ".txt";
                    }

                    path = editor.m_CurrentAssetPath / filename;

                    int suffix = 1;
                    while (fs::exists(path))
                    {
                        path = editor.m_CurrentAssetPath / (std::string(create.aNewItemName) + "_" + std::to_string(suffix++) + ".txt");
                    }

                    std::ofstream f(path);
                    f.close();
                }

                editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(lang.Word("cancel"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

static bool PathsReferToSameFile(const fs::path& lhs, const fs::path& rhs)
{
    std::error_code lhsError;
    std::error_code rhsError;
    const fs::path normalizedLhs = fs::weakly_canonical(lhs, lhsError);
    const fs::path normalizedRhs = fs::weakly_canonical(rhs, rhsError);
    if (!lhsError && !rhsError)
    {
        return normalizedLhs == normalizedRhs;
    }
    return lhs.lexically_normal() == rhs.lexically_normal();
}

static void DrawAssetDependencies(CEditor& editor, const fs::path& selected)
{
    const fs::path resourceDir = fs::path(editor.m_ProjectPath) / "resources";
    std::error_code relativeError;
    const std::string selectedRelative = fs::relative(selected, resourceDir, relativeError).generic_string();
    std::vector<std::pair<int, std::string>> vEntityReferences;
    SAssetBrowserState& browser = editor.m_Ui.m_AssetBrowser;
    if (browser.dependencyCachePath != selected.string())
    {
        browser.dependencyCachePath = selected.string();
        browser.vCachedFileDependencies.clear();
        browser.dependencyCacheReady = false;
    }

    const bool isMaterial = Lowercase(selected.extension().string()) == ".mtl";
    const bool isObjModel = Lowercase(selected.extension().string()) == ".obj";
    const bool isTexture = CTextureMetadataStore::IsImageFile(selected);
    unsigned int textureId = 0;
    if (isTexture && !relativeError)
    {
        for (const STextureOption& option : editor.m_Assets.Textures())
        {
            if (option.Name == selectedRelative)
            {
                textureId = option.Texture.id;
                break;
            }
        }
    }

    for (int entityIndex = 0; entityIndex < static_cast<int>(editor.m_Scene.m_vEntities.size()); ++entityIndex)
    {
        const CEntity& entity = editor.m_Scene.m_vEntities[entityIndex];
        const CMeshComponent* pMesh = entity.GetMeshComponent();
        const CMaterialComponent* pMaterial = entity.GetMaterialComponent();

        if (pMesh && !selectedRelative.empty() &&
            fs::path(pMesh->m_AssetName).lexically_normal().generic_string() ==
                fs::path(selectedRelative).lexically_normal().generic_string())
        {
            vEntityReferences.emplace_back(entityIndex, "Model used by: " + entity.m_Name);
        }

        if (!pMaterial)
        {
            continue;
        }

        if (isMaterial && !pMaterial->m_TextureName.empty() &&
            PathsReferToSameFile(pMaterial->m_TextureName, selected))
        {
            vEntityReferences.emplace_back(entityIndex, "Material used by: " + entity.m_Name);
        }

        if (isTexture && textureId != 0 && pMaterial->m_Texture.id == textureId)
        {
            vEntityReferences.emplace_back(entityIndex, "Texture used by: " + entity.m_Name);
        }
    }

    if (!browser.dependencyCacheReady && isObjModel)
    {
        std::ifstream modelFile(selected);
        std::string line;
        while (std::getline(modelFile, line))
        {
            std::istringstream stream(line);
            std::string type;
            stream >> type;
            if (type != "mtllib")
            {
                continue;
            }

            std::string materialName;
            while (stream >> materialName)
            {
                browser.vCachedFileDependencies.push_back("Uses material: " +
                    (selected.parent_path() / materialName).lexically_normal().string());
            }
        }
    }
    else if (!browser.dependencyCacheReady && isMaterial)
    {
        std::ifstream materialFile(selected);
        std::string line;
        while (std::getline(materialFile, line))
        {
            std::istringstream stream(line);
            std::string type;
            stream >> type;
            if (type != "map_Kd")
            {
                continue;
            }

            std::string textureName;
            std::getline(stream >> std::ws, textureName);
            if (!textureName.empty())
            {
                browser.vCachedFileDependencies.push_back("Uses texture: " +
                    (selected.parent_path() / fs::path(textureName)).lexically_normal().string());
            }
        }
    }
    else if (!browser.dependencyCacheReady && isTexture)
    {
        std::error_code scanError;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(
            resourceDir, fs::directory_options::skip_permission_denied, scanError))
        {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError ||
                Lowercase(entry.path().extension().string()) != ".mtl")
            {
                continue;
            }

            std::ifstream materialFile(entry.path());
            std::string line;
            while (std::getline(materialFile, line))
            {
                std::istringstream stream(line);
                std::string type;
                stream >> type;
                if (type != "map_Kd")
                {
                    continue;
                }

                std::string textureName;
                std::getline(stream >> std::ws, textureName);
                if (!textureName.empty() &&
                    PathsReferToSameFile(entry.path().parent_path() / fs::path(textureName), selected))
                {
                    browser.vCachedFileDependencies.push_back("Used by material: " + entry.path().string());
                }
            }
        }
    }
    browser.dependencyCacheReady = true;

    ImGui::Separator();
    ImGui::TextUnformatted(lang.Word("dependencies"));
    for (const std::string& reference : browser.vCachedFileDependencies)
    {
        ImGui::TextWrapped("%s", reference.c_str());
    }
    for (const auto& [entityIndex, reference] : vEntityReferences)
    {
        ImGui::PushID(entityIndex);
        if (ImGui::Selectable(reference.c_str()))
        {
            editor.m_Scene.SelectEntity(entityIndex, false);
        }
        ImGui::PopID();
    }
    if (browser.vCachedFileDependencies.empty() && vEntityReferences.empty())
    {
        ImGui::TextDisabled("%s", lang.Word("no_references_found"));
    }
}

void DrawSelectedTextureInspector(CEditor& editor)
{
    if (editor.m_SelectedAssetName.empty())
    {
        return;
    }

    const fs::path selected = editor.m_CurrentAssetPath / editor.m_SelectedAssetName;
    std::error_code selectedError;
    if (fs::is_regular_file(selected, selectedError) && !selectedError)
    {
        const fs::path dependencyPath = selected.extension() == ".meta"
            ? CTextureMetadataStore::PathFromMeta(selected)
            : selected;
        DrawAssetDependencies(editor, dependencyPath);
    }

    const fs::path texturePath = CTextureMetadataStore::PathFromMeta(selected);
    if (!CTextureMetadataStore::IsImageFile(texturePath) || !fs::exists(texturePath))
    {
        return;
    }

    STextureMeta meta;
    if (!CTextureMetadataStore::Load(texturePath, meta))
    {
        CTextureMetadataStore::Ensure(texturePath);
        CTextureMetadataStore::Load(texturePath, meta);
    }

    ImGui::Separator();
    ImGui::TextUnformatted(lang.Word("texture_importer"));
    ImGui::TextWrapped("%s", texturePath.filename().string().c_str());
    ImGui::TextDisabled("GUID: %s", meta.Guid.c_str());

    bool changed = false;
    changed |= ImGui::Checkbox(lang.Word("enable_mip_maps"), &meta.EnableMipMap);
    changed |= ImGui::Checkbox(lang.Word("srgb_texture"), &meta.SrgbTexture);
    changed |= ImGui::Checkbox(lang.Word("readable"), &meta.IsReadable);
    changed |= ImGui::Checkbox(lang.Word("alpha_transparency"), &meta.AlphaIsTransparency);
    const char* apFilterModes[] = { lang.Word("nearest"), lang.Word("linear") };
    const char* apWrapModes[] = { lang.Word("repeat"), lang.Word("clamp") };
    const char* apSpriteModes[] = { lang.Word("none"), lang.Word("single"), lang.Word("multiple") };
    const char* apTextureTypes[] = {
        lang.Word("default"), lang.Word("normal"), lang.Word("sprite"), lang.Word("cursor"),
        lang.Word("cookie"), lang.Word("lightmap"), lang.Word("shadowmask"), lang.Word("directional"),
        lang.Word("single_channel")
    };
    changed |= ImGui::Combo(lang.Word("filter_mode"), &meta.FilterMode, apFilterModes, IM_ARRAYSIZE(apFilterModes));
    changed |= ImGui::Combo(lang.Word("wrap_u"), &meta.WrapU, apWrapModes, IM_ARRAYSIZE(apWrapModes));
    changed |= ImGui::Combo(lang.Word("wrap_v"), &meta.WrapV, apWrapModes, IM_ARRAYSIZE(apWrapModes));
    changed |= ImGui::SliderInt(lang.Word("max_texture_size"), &meta.MaxTextureSize, 32, 8192);
    changed |= ImGui::SliderInt(lang.Word("compression_quality"), &meta.CompressionQuality, 0, 100);
    changed |= ImGui::Combo(lang.Word("sprite_mode"), &meta.SpriteMode, apSpriteModes, IM_ARRAYSIZE(apSpriteModes));
    changed |= ImGui::Combo(lang.Word("texture_type"), &meta.TextureType, apTextureTypes, IM_ARRAYSIZE(apTextureTypes));

    if (changed)
    {
        CTextureMetadataStore::Save(texturePath, meta);
        const std::string relativeName = fs::relative(texturePath, fs::path(editor.m_ProjectPath) / "resources").generic_string();
        for (auto& option : editor.m_Assets.Textures())
        {
            if (option.Name == relativeName)
            {
                CTextureMetadataStore::ApplyToTexture(option.Texture, meta);
            }
        }
    }
}

void CEditor::DrawAssetsUi()
{
    ::DrawAssetsUi(*this);
}

void CleanupAssetsUi(CEditor& editor)
{
    editor.m_Previews.Unload();
}
