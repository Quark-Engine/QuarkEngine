#include "editor/editor_assets.h"

#include "application_plugin_bridge.h"
#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "engine/scene_document.h"
#include "qcImGui.h"
#include "project.h"
#include "tex.h"
#include "language_manager.h"
#include "editor/editor_preferences.h"
#include "imgui.h"
#include "imgui_internal.h"
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

    constexpr float ICON_SIZE = 64.0f;

    std::string Lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
                       {
                           return static_cast<char>(std::tolower(character));
                       });
        return value;
    }

    bool SaveEntityBranchToDirectory(CEditor& editor, int entityIndex, const fs::path& targetDir)
    {
        if (entityIndex < 0 || entityIndex >= static_cast<int>(editor.m_Scene.m_vEntities.size()))
        {
            return false;
        }

        fs::path savedPath;
        if (!CEntityFactory::SavePrefab(editor.m_Scene, entityIndex, targetDir, &savedPath))
        {
            return false;
        }

        const int syncedCount = CEntityFactory::SyncPrefabInstances(
            editor.m_Scene, editor.m_Assets, savedPath, editor.m_ComponentFactories, entityIndex);
        editor.m_Ui.m_AssetBrowser.m_AssetListCacheDirty = true;
        editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
        if (syncedCount > 0)
        {
            editor.SetStatusMessage(
                TextFormat("Saved prefab: %s (synced %d instance(s))",
                           savedPath.filename().string().c_str(), syncedCount));
        }
        else
        {
            editor.SetStatusMessage(TextFormat("Saved prefab: %s", savedPath.filename().string().c_str()));
        }
        return true;
    }

    bool SpawnPrefabInScene(CEditor& editor, const fs::path& prefabPath)
    {
        std::ifstream file(prefabPath);
        if (!file.is_open())
        {
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        if (!quark::CSceneDocument::ParsePrefab(buffer.str()).m_IsValid)
        {
            return false;
        }

        editor.SaveState();
        const std::vector<int> vCreatedIndices = CEntityFactory::InstantiatePrefab(
            editor.m_Scene, editor.m_Assets, prefabPath, editor.m_ComponentFactories, -1);
        if (vCreatedIndices.empty())
        {
            return false;
        }

        editor.m_Scene.SelectEntity(vCreatedIndices.front(), false);
        for (int createdIndex : vCreatedIndices)
        {
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, createdIndex);
        }
        editor.m_SceneDirty = true;
        editor.RequestSceneRedraw();
        return true;
    }

    void CreateScriptFiles(const fs::path& directory, const std::string& scriptName)
    {
        std::string safeName = scriptName;
        while (!safeName.empty() && (safeName.front() == ' ' || safeName.front() == '\t'))
        {
            safeName.erase(safeName.begin());
        }
        while (!safeName.empty() && (safeName.back() == ' ' || safeName.back() == '\t'))
        {
            safeName.pop_back();
        }
        if (safeName.empty())
        {
            return;
        }

        std::string upperName = safeName;
        for (char& c : upperName)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        const fs::path hppPath = directory / (safeName + ".hpp");
        const fs::path cppPath = directory / (safeName + ".cpp");

        std::ofstream hppOut(hppPath);
        hppOut << "#ifndef __" << upperName << "_HPP__\n";
        hppOut << "#define __" << upperName << "_HPP__\n\n";
        hppOut << "#include \"QuarkEngine/SDK.h\"\n\n";
        hppOut << "class " << safeName << " : public IScript\n";
        hppOut << "{\n";
        hppOut << "public:\n";
        hppOut << "    void OnCreate() override;\n";
        hppOut << "    void OnStart() override;\n";
        hppOut << "    void OnUpdate(float deltaTime) override;\n";
        hppOut << "    void OnFixedUpdate(float fixedDeltaTime) override;\n";
        hppOut << "    void OnLateUpdate(float deltaTime) override;\n";
        hppOut << "    void OnDestroy() override;\n";
        hppOut << "};\n\n";
        hppOut << "#endif // __" << upperName << "_HPP__\n";
        hppOut.close();

        std::ofstream cppOut(cppPath);
        cppOut << "#include \"" << safeName << ".hpp\"\n\n";
        cppOut << "void " << safeName << "::OnCreate()\n";
        cppOut << "{\n";
        cppOut << "    // Script created\n";
        cppOut << "}\n\n";
        cppOut << "void " << safeName << "::OnStart()\n";
        cppOut << "{\n";
        cppOut << "    // Script initialized\n";
        cppOut << "}\n\n";
        cppOut << "void " << safeName << "::OnUpdate(float deltaTime)\n";
        cppOut << "{\n";
        cppOut << "    // Script updating...\n";
        cppOut << "}\n\n";
        cppOut << "void " << safeName << "::OnFixedUpdate(float fixedDeltaTime)\n";
        cppOut << "{\n";
        cppOut << "    // Physics / fixed step update...\n";
        cppOut << "}\n\n";
        cppOut << "void " << safeName << "::OnLateUpdate(float deltaTime)\n";
        cppOut << "{\n";
        cppOut << "    // Camera / post-update...\n";
        cppOut << "}\n\n";
        cppOut << "void " << safeName << "::OnDestroy()\n";
        cppOut << "{\n";
        cppOut << "    // Script destroying\n";
        cppOut << "}\n\n";
        cppOut << "REGISTER_SCRIPT(" << safeName << ");\n";
        cppOut.close();
    }

    bool MatchesAssetFilter(const SLocalEntry& entry, int filter, const std::string& search)
    {
        if (entry.m_IsDirectory)
        {
            return filter == 0 &&
                (search.empty() || Lowercase(entry.m_FileName).find(search) != std::string::npos);
        }

        bool matchesType = false;
        switch (filter)
        {
            case 0:
                matchesType = !entry.m_IsTextureMeta;
                break;
            case 1:
                matchesType = entry.m_IsImage || entry.m_IsModel;
                break;
            case 2:
                matchesType = entry.m_IsMaterial;
                break;
            case 3:
                matchesType = entry.m_IsTextureMeta;
                break;
            case 4:
                matchesType = entry.m_IsPrefab;
                break;
            default:
                matchesType = true;
                break;
        }

        if (!matchesType)
        {
            return false;
        }

        return search.empty() || Lowercase(entry.m_FileName).find(search) != std::string::npos;
    }

    bool MatchesFavoriteFilter(const SLocalEntry& entry, int favoriteFilter, const std::string& search)
    {
        if (entry.m_IsDirectory || entry.m_IsTextureMeta)
        {
            return false;
        }
        bool matches = false;
        switch (favoriteFilter)
        {
            case 1: // All GUI / Textures
                matches = entry.m_IsImage;
                break;
            case 2: // All Materials
                matches = entry.m_IsMaterial;
                break;
            case 3: // All Models
                matches = entry.m_IsModel;
                break;
            case 4: // All Prefabs
                matches = entry.m_IsPrefab;
                break;
            case 5: // All Scripts
                matches = entry.m_IsScript;
                break;
            case 6: // All Scenes
                matches = entry.m_IsScene;
                break;
            default:
                matches = true;
                break;
        }
        if (!matches)
        {
            return false;
        }
        return search.empty() || Lowercase(entry.m_FileName).find(search) != std::string::npos;
    }

    void DrawStarIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const ImVec2 center(minPt.x + size * 0.5f, minPt.y + size * 0.5f);
        const float outerR = size * 0.48f;
        const float innerR = size * 0.22f;
        ImVec2 aPts[10];
        for (int i = 0; i < 10; ++i)
        {
            const float angle = -1.5707963f + static_cast<float>(i) * 0.6283185f;
            const float r = (i % 2 == 0) ? outerR : innerR;
            aPts[i] = ImVec2(center.x + std::cos(angle) * r, center.y + std::sin(angle) * r);
        }
        for (int i = 0; i < 10; ++i)
        {
            pDraw->AddTriangleFilled(center, aPts[i], aPts[(i + 1) % 10], IM_COL32(245, 205, 45, 255));
        }
    }

    void DrawSearchLensIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const ImVec2 center(minPt.x + size * 0.42f, minPt.y + size * 0.42f);
        const float radius = size * 0.28f;
        pDraw->AddCircle(center, radius, IM_COL32(240, 200, 50, 255), 16, 2.0f);
        pDraw->AddLine(
            ImVec2(center.x + radius * 0.7f, center.y + radius * 0.7f),
            ImVec2(minPt.x + size * 0.88f, minPt.y + size * 0.88f),
            IM_COL32(240, 200, 50, 255), 2.2f);
    }

    void DrawFolderSmallIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const ImU32 tabCol = IM_COL32(130, 145, 165, 255);
        const ImU32 bodyCol = IM_COL32(150, 168, 190, 255);
        const ImU32 borderCol = IM_COL32(60, 70, 82, 255);
        pDraw->AddRectFilled(
            ImVec2(minPt.x + size * 0.08f, minPt.y + size * 0.16f),
            ImVec2(minPt.x + size * 0.46f, minPt.y + size * 0.34f),
            tabCol, 2.0f);
        pDraw->AddRectFilled(
            ImVec2(minPt.x + size * 0.08f, minPt.y + size * 0.28f),
            ImVec2(minPt.x + size * 0.92f, minPt.y + size * 0.86f),
            bodyCol, 2.0f);
        pDraw->AddRect(
            ImVec2(minPt.x + size * 0.08f, minPt.y + size * 0.28f),
            ImVec2(minPt.x + size * 0.92f, minPt.y + size * 0.86f),
            borderCol, 2.0f, 0, 1.0f);
    }

    void DrawPrefabCubeIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const float cx = minPt.x + size * 0.5f;
        const float topY = minPt.y + size * 0.08f;
        const float midY = minPt.y + size * 0.32f;
        const float botMidY = minPt.y + size * 0.68f;
        const float botY = minPt.y + size * 0.92f;
        const float leftX = minPt.x + size * 0.12f;
        const float rightX = minPt.x + size * 0.88f;

        const ImVec2 pTop(cx, topY);
        const ImVec2 pRightTop(rightX, midY);
        const ImVec2 pCenter(cx, minPt.y + size * 0.50f);
        const ImVec2 pLeftTop(leftX, midY);
        const ImVec2 pLeftBot(leftX, botMidY);
        const ImVec2 pBot(cx, botY);
        const ImVec2 pRightBot(rightX, botMidY);

        pDraw->AddQuadFilled(pTop, pRightTop, pCenter, pLeftTop, IM_COL32(145, 205, 250, 255));
        pDraw->AddQuadFilled(pLeftTop, pCenter, pBot, pLeftBot, IM_COL32(95, 168, 230, 255));
        pDraw->AddQuadFilled(pCenter, pRightTop, pRightBot, pBot, IM_COL32(65, 130, 195, 255));
        pDraw->AddLine(pTop, pRightTop, IM_COL32(35, 70, 105, 220), 1.0f);
        pDraw->AddLine(pRightTop, pRightBot, IM_COL32(35, 70, 105, 220), 1.0f);
        pDraw->AddLine(pRightBot, pBot, IM_COL32(35, 70, 105, 220), 1.0f);
        pDraw->AddLine(pBot, pLeftBot, IM_COL32(35, 70, 105, 220), 1.0f);
        pDraw->AddLine(pLeftBot, pLeftTop, IM_COL32(35, 70, 105, 220), 1.0f);
        pDraw->AddLine(pLeftTop, pTop, IM_COL32(35, 70, 105, 220), 1.0f);
    }

    void DrawMaterialSphereIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const ImVec2 center(minPt.x + size * 0.5f, minPt.y + size * 0.5f);
        const float r = size * 0.42f;
        pDraw->AddCircleFilled(center, r, IM_COL32(85, 135, 185, 255), 20);
        pDraw->AddCircleFilled(
            ImVec2(center.x - r * 0.25f, center.y - r * 0.25f),
            r * 0.55f, IM_COL32(135, 185, 230, 200), 16);
        pDraw->AddCircleFilled(
            ImVec2(center.x - r * 0.35f, center.y - r * 0.35f),
            r * 0.22f, IM_COL32(235, 248, 255, 230), 12);
        pDraw->AddCircle(center, r, IM_COL32(40, 65, 95, 255), 20, 1.0f);
    }

    void DrawScriptDocIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const ImVec2 docMin(minPt.x + size * 0.16f, minPt.y + size * 0.08f);
        const ImVec2 docMax(minPt.x + size * 0.84f, minPt.y + size * 0.92f);
        pDraw->AddRectFilled(docMin, docMax, IM_COL32(230, 236, 242, 255), 1.5f);
        pDraw->AddRect(docMin, docMax, IM_COL32(90, 100, 115, 255), 1.5f, 0, 1.0f);
        pDraw->AddRectFilled(
            ImVec2(docMin.x, docMin.y + size * 0.35f),
            ImVec2(docMax.x, docMax.y),
            IM_COL32(65, 145, 210, 240), 1.0f);
        if (size >= 16.0f)
        {
            pDraw->AddText(
                ImGui::GetFont(), std::min(11.0f, size * 0.50f),
                ImVec2(docMin.x + 1.5f, docMin.y + size * 0.38f),
                IM_COL32(255, 255, 255, 255), "C+");
        }
    }

    void DrawSceneIcon(ImDrawList* pDraw, ImVec2 minPt, float size)
    {
        const ImVec2 center(minPt.x + size * 0.5f, minPt.y + size * 0.5f);
        const float r = size * 0.40f;
        pDraw->AddCircleFilled(center, r, IM_COL32(45, 50, 60, 255), 16);
        pDraw->AddCircle(center, r, IM_COL32(215, 225, 240, 255), 16, 1.6f);
        pDraw->AddTriangleFilled(
            ImVec2(center.x, center.y - r * 0.65f),
            ImVec2(center.x + r * 0.60f, center.y + r * 0.45f),
            ImVec2(center.x - r * 0.60f, center.y + r * 0.45f),
            IM_COL32(235, 240, 250, 255));
    }

    void BuildFolderTreeNodeRecursive(const fs::path& folderPath, const std::string& displayName, bool defaultOpen, SAssetFolderTreeNode& outNode)
    {
        outNode = {};
        outNode.m_DisplayName = displayName;
        outNode.m_FolderPath = folderPath;
        outNode.m_NormalizedPath = folderPath.lexically_normal().generic_string();
        outNode.m_NodeId = "##tree_" + outNode.m_NormalizedPath;
        outNode.m_DefaultOpen = defaultOpen;

        std::vector<fs::path> vSubdirs;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(folderPath, ec))
        {
            if (entry.is_directory(ec))
            {
                const std::string subName = entry.path().filename().string();
                if (!subName.empty() && subName[0] != '.')
                {
                    vSubdirs.push_back(entry.path());
                }
            }
        }
        std::sort(vSubdirs.begin(), vSubdirs.end());

        outNode.m_vChildren.reserve(vSubdirs.size());
        for (const fs::path& subDir : vSubdirs)
        {
            SAssetFolderTreeNode childNode = {};
            BuildFolderTreeNodeRecursive(subDir, subDir.filename().string(), false, childNode);
            outNode.m_vChildren.push_back(std::move(childNode));
        }
    }

    void DrawFolderTreeRecursive(CEditor& editor, const SAssetFolderTreeNode& node)
    {
        SAssetBrowserState& browser = editor.m_Ui.m_AssetBrowser;
        SAssetBrowserDrag& drag = browser.m_Drag;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node.m_DefaultOpen)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (node.m_vChildren.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (browser.m_FavoriteFilter == 0 &&
            browser.m_CachedNormalizedCurrentPath == node.m_NormalizedPath)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool isOpen = ImGui::TreeNodeEx(node.m_NodeId.c_str(), flags);
        const bool nodeClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
            {
                if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                {
                    const int entityIdx = *static_cast<const int*>(pPayload->Data);
                    SaveEntityBranchToDirectory(editor, entityIdx, node.m_FolderPath);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (editor.m_Ui.m_Layout.m_FileDragging && ImGui::IsMouseHoveringRect(itemMin, itemMax))
        {
            ImGui::GetWindowDrawList()->AddRect(itemMin, itemMax, IM_COL32(100, 200, 100, 220), 2.0f, 0, 1.5f);
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !drag.m_FilePath.empty())
            {
                const fs::path dest = node.m_FolderPath / drag.m_FilePath.filename();
                if (dest.lexically_normal() != drag.m_FilePath.lexically_normal())
                {
                    if (fs::exists(dest))
                    {
                        browser.m_DuplicatePopupOpen = true;
                        browser.m_DuplicateName = drag.m_FilePath.filename().string();
                    }
                    else
                    {
                        std::error_code moveEc;
                        fs::rename(drag.m_FilePath, dest, moveEc);
                        if (!moveEc)
                        {
                            if (CTextureMetadataStore::IsImageFile(drag.m_FilePath) &&
                                fs::exists(drag.m_FilePath.string() + ".meta"))
                            {
                                fs::rename(drag.m_FilePath.string() + ".meta", dest.string() + ".meta", moveEc);
                            }
                            editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                            browser.m_AssetListCacheDirty = true;
                            editor.m_SelectedAssetIndex = -1;
                        }
                    }
                }
                editor.m_Ui.m_Layout.m_FileDragging = false;
                drag.m_FilePath.clear();
                editor.m_Ui.m_Layout.m_DraggedFileIndex = -1;
            }
        }

        ImDrawList* pDraw = ImGui::GetWindowDrawList();
        const float arrowPad = ImGui::GetTreeNodeToLabelSpacing();
        const float iconSz = 16.0f;
        const ImVec2 iconPos(itemMin.x + arrowPad, itemMin.y + (itemMax.y - itemMin.y - iconSz) * 0.5f);
        DrawFolderSmallIcon(pDraw, iconPos, iconSz);
        pDraw->AddText(
            ImVec2(iconPos.x + iconSz + 5.0f, itemMin.y + (itemMax.y - itemMin.y - ImGui::GetTextLineHeight()) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text),
            node.m_DisplayName.c_str());

        if (nodeClicked)
        {
            browser.m_FavoriteFilter = 0;
            editor.m_CurrentAssetPath = node.m_FolderPath;
            browser.m_AssetListCacheDirty = true;
            editor.m_SelectedAssetIndex = -1;
            editor.m_SelectedAssetName.clear();
            editor.m_Previews.InvalidateModelPreviews();
        }

        if (isOpen)
        {
            for (const SAssetFolderTreeNode& childNode : node.m_vChildren)
            {
                DrawFolderTreeRecursive(editor, childNode);
            }
            ImGui::TreePop();
        }
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
        const int result = std::system(command.c_str());
        if (result != 0)
        {
            TraceLog(LogLevel::Error, "EDITOR", TextFormat("Failed to open file explorer for: %s", path.string().c_str()));
        }
#elif defined(__linux__)
        const std::string command = "xdg-open \"" + path.string() + "\"";
        const int result = std::system(command.c_str());
        if (result != 0)
        {
            TraceLog(LogLevel::Error, "EDITOR", TextFormat("Failed to open file explorer for: %s", path.string().c_str()));
        }
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

    Vec3 center = { 0, 0, 0 };
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
        return { 0 };
    }

    Model sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));

    Color albedo = WHITE;
    Texture2D tex = { 0 };
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
            float r = 1, g = 1, b = 1;
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
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = { 0 };
        UnloadModel(sphere);
        return rt;
    }

    Camera3D cam = {};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    cam.target = { 0, 0, 0 };
    cam.up = { 0, 1, 0 };
    cam.position = { 2, 2, 2 };

    BeginTextureMode(rt);
    ClearBackground({ 40, 40, 45, 255 });
    BeginMode3D(cam);

    DrawModel(sphere, { 0, 0, 0 }, 1.0f, WHITE);
    DrawModelWires(sphere, { 0, 0, 0 }, 1.0f, DARKGRAY);

    EndMode3D();
    EndTextureMode();

    sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = { 0 };
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
    if (!ImGui::Begin(lang.Word("assets"), &editor.m_Preferences.m_ShowAssets,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::End();
        return;
    }

    editor.m_Previews.EnsureIcons();

    std::string popupName = std::string(lang.Word("create_new")) + "##CreateNew";

    SAssetBrowserState& browser = editor.m_Ui.m_AssetBrowser;
    SAssetBrowserMarquee& marquee = browser.m_Marquee;
    SAssetBrowserDrag& drag = browser.m_Drag;
    SAssetBrowserCreatePopup& create = browser.m_Create;

    if (editor.m_CurrentAssetPath.empty() && !editor.m_ProjectPath.empty())
    {
        editor.m_CurrentAssetPath = fs::path(editor.m_ProjectPath) / "resources";
        browser.m_AssetListCacheDirty = true;
    }

    const fs::path projectRoot = fs::path(editor.m_ProjectPath);
    const fs::path assetsRoot = projectRoot / "resources";

    if (ImGui::SmallButton("Create v"))
    {
        ImGui::OpenPopup("AssetBrowserCreatePopup");
    }
    if (ImGui::BeginPopup("AssetBrowserCreatePopup"))
    {
        if (ImGui::MenuItem(lang.Word("new_folder")))
        {
            create.m_CreatingFolder = true;
            create.m_CreatingScript = false;
            create.m_aNewItemName[0] = '\0';
            create.m_Open = true;
        }
        if (ImGui::MenuItem(lang.Word("new_script")))
        {
            create.m_CreatingFolder = false;
            create.m_CreatingScript = true;
            create.m_aNewItemName[0] = '\0';
            create.m_Open = true;
        }
        if (ImGui::MenuItem("Material"))
        {
            fs::path matPath = editor.m_CurrentAssetPath / "NewMaterial.mtl";
            int suffix = 1;
            while (fs::exists(matPath))
            {
                matPath = editor.m_CurrentAssetPath / ("NewMaterial_" + std::to_string(suffix++) + ".mtl");
            }
            std::ofstream matOut(matPath);
            matOut << "newmtl Default\nKd 0.8 0.8 0.8\n";
            matOut.close();
            editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
            browser.m_AssetListCacheDirty = true;
        }
        if (ImGui::MenuItem("Scene"))
        {
            fs::path scenePath = editor.m_CurrentAssetPath / "NewScene.json";
            int suffix = 1;
            while (fs::exists(scenePath))
            {
                scenePath = editor.m_CurrentAssetPath / ("NewScene_" + std::to_string(suffix++) + ".json");
            }
            CScene emptyScene;
            std::ofstream sceneOut(scenePath);
            sceneOut << quark::CSceneDocument::Serialize(emptyScene, 4);
            sceneOut.close();
            browser.m_AssetListCacheDirty = true;
        }
        if (ImGui::MenuItem(lang.Word("new_file")))
        {
            create.m_CreatingFolder = false;
            create.m_CreatingScript = false;
            create.m_aNewItemName[0] = '\0';
            create.m_Open = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 12.0f);
    if (ImGui::SmallButton(browser.m_ListViewMode ? "View: List" : "View: Grid"))
    {
        browser.m_ListViewMode = !browser.m_ListViewMode;
    }

    ImGui::SameLine(0.0f, 12.0f);
    const char* apAssetFilterNames[] = {
        lang.Word("all"), lang.Word("images_models"), lang.Word("materials"),
        lang.Word("texture_metadata"), lang.Word("prefabs")
    };
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::Combo("##asset_type_filter_browser", &editor.m_Preferences.m_AssetFilter,
                     apAssetFilterNames, IM_ARRAYSIZE(apAssetFilterNames)))
    {
        browser.m_FavoriteFilter = 0;
        editor.m_Preferences.Save();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##asset_search", lang.Word("search_assets"), browser.m_aSearchBuffer,
                             IM_ARRAYSIZE(browser.m_aSearchBuffer));

    ImGui::Separator();

    const std::string search = Lowercase(browser.m_aSearchBuffer);
    auto populateEntryFromDirEntry = [](const fs::directory_entry& pathEntry, SLocalEntry& outEntry) -> bool
    {
        std::error_code entryEc;
        outEntry = {};
        outEntry.m_FullPath = pathEntry.path();
        outEntry.m_FileName = pathEntry.path().filename().string();
        outEntry.m_IsDirectory = pathEntry.is_directory(entryEc);
        outEntry.m_IsDirectoryEmpty = outEntry.m_IsDirectory && fs::is_empty(pathEntry.path(), entryEc);
        outEntry.m_IsImage = CTextureMetadataStore::IsImageFile(pathEntry.path());
        outEntry.m_IsModel = CModelService::IsModelFile(pathEntry.path());

        std::string ext = pathEntry.path().extension().string();
        if (!ext.empty() && ext[0] == '.')
        {
            ext.erase(ext.begin());
        }
        ext = Lowercase(std::move(ext));
        outEntry.m_Extension = ext;
        outEntry.m_IsMaterial = outEntry.m_Extension == "mtl";
        outEntry.m_IsTextureMeta = outEntry.m_Extension == "meta" &&
            CTextureMetadataStore::IsImageFile(pathEntry.path().stem());
        if (outEntry.m_IsTextureMeta && !fs::exists(pathEntry.path().parent_path() / pathEntry.path().stem()))
        {
            return false;
        }
        outEntry.m_IsPrefab = outEntry.m_Extension == "prefab";
        outEntry.m_IsScript = (outEntry.m_Extension == "hpp" || outEntry.m_Extension == "h" || outEntry.m_Extension == "cpp");
        outEntry.m_IsScene = (outEntry.m_Extension == "json" || outEntry.m_Extension == "scene");
        if (outEntry.m_Extension == "meta" && !outEntry.m_IsTextureMeta)
        {
            return false;
        }

        outEntry.m_DisplayLabel = outEntry.m_FileName;
        if (!outEntry.m_IsDirectory &&
            (outEntry.m_IsPrefab || outEntry.m_IsMaterial || outEntry.m_IsScript || outEntry.m_IsScene || outEntry.m_IsModel))
        {
            const std::string stem = pathEntry.path().stem().string();
            if (!stem.empty())
            {
                outEntry.m_DisplayLabel = stem;
                if (outEntry.m_Extension == "hpp" || outEntry.m_Extension == "h")
                {
                    outEntry.m_DisplayLabel += " (Header)";
                }
            }
        }
        return true;
    };

    const bool projectPathChanged = (browser.m_CachedProjectPath != editor.m_ProjectPath);
    const bool currentPathChanged = (browser.m_CachedPath != editor.m_CurrentAssetPath);
    if (browser.m_AssetListCacheDirty ||
        projectPathChanged ||
        currentPathChanged ||
        browser.m_CachedFilter != editor.m_Preferences.m_AssetFilter ||
        browser.m_CachedFavoriteFilter != browser.m_FavoriteFilter ||
        browser.m_CachedSearch != search)
    {
        if (browser.m_AssetListCacheDirty || projectPathChanged)
        {
            std::error_code dirCreateEc;
            fs::create_directories(assetsRoot, dirCreateEc);

            browser.m_vCachedFolderRoots.clear();
            SAssetFolderTreeNode assetsNode = {};
            BuildFolderTreeNodeRecursive(assetsRoot, "Assets", true, assetsNode);
            browser.m_vCachedFolderRoots.push_back(std::move(assetsNode));

            std::error_code pkgEc;
            const fs::path packagesRoot("assets");
            if (fs::exists(packagesRoot, pkgEc) && !pkgEc)
            {
                SAssetFolderTreeNode packagesNode = {};
                BuildFolderTreeNodeRecursive(packagesRoot, "Packages", false, packagesNode);
                browser.m_vCachedFolderRoots.push_back(std::move(packagesNode));
            }
            browser.m_CachedProjectPath = editor.m_ProjectPath;
        }

        browser.m_CachedNormalizedCurrentPath = editor.m_CurrentAssetPath.lexically_normal().generic_string();
        {
            fs::path baseRoot = assetsRoot;
            std::string rootTitle = "Assets";
            const std::string pkgNorm = fs::path("assets").lexically_normal().generic_string();
            if (browser.m_CachedNormalizedCurrentPath == pkgNorm ||
                browser.m_CachedNormalizedCurrentPath.find(pkgNorm + "/") == 0)
            {
                baseRoot = fs::path("assets");
                rootTitle = "Packages";
            }

            browser.m_vCachedBreadcrumbs.clear();
            browser.m_vCachedBreadcrumbs.emplace_back(rootTitle, baseRoot);
            const fs::path relToRoot = editor.m_CurrentAssetPath.lexically_normal().lexically_relative(baseRoot.lexically_normal());
            if (!relToRoot.empty() && relToRoot != "." && relToRoot.generic_string().find("..") != 0)
            {
                fs::path accum = baseRoot;
                for (const auto& part : relToRoot)
                {
                    if (part == ".")
                    {
                        continue;
                    }
                    accum /= part;
                    browser.m_vCachedBreadcrumbs.emplace_back(part.string(), accum);
                }
            }
        }

        std::vector<SLocalEntry> vDirectories;
        std::vector<SLocalEntry> vFiles;
        std::error_code dirError;

        if (browser.m_FavoriteFilter > 0)
        {
            for (const auto& pathEntry : fs::recursive_directory_iterator(
                     assetsRoot, fs::directory_options::skip_permission_denied, dirError))
            {
                if (pathEntry.path().generic_string().find("/.script_build") != std::string::npos)
                {
                    continue;
                }
                SLocalEntry entry = {};
                if (!populateEntryFromDirEntry(pathEntry, entry))
                {
                    continue;
                }
                if (!MatchesFavoriteFilter(entry, browser.m_FavoriteFilter, search))
                {
                    continue;
                }
                vFiles.push_back(std::move(entry));
            }
        }
        else
        {
            for (const auto& pathEntry : fs::directory_iterator(editor.m_CurrentAssetPath, dirError))
            {
                SLocalEntry entry = {};
                if (!populateEntryFromDirEntry(pathEntry, entry))
                {
                    continue;
                }
                if (!MatchesAssetFilter(entry, editor.m_Preferences.m_AssetFilter, search))
                {
                    continue;
                }

                if (entry.m_IsDirectory)
                {
                    vDirectories.push_back(std::move(entry));
                }
                else
                {
                    vFiles.push_back(std::move(entry));
                }
            }
        }

        auto sortByName = [](const SLocalEntry& a, const SLocalEntry& b)
        {
            return Lowercase(a.m_FileName) < Lowercase(b.m_FileName);
        };
        std::sort(vDirectories.begin(), vDirectories.end(), sortByName);
        std::sort(vFiles.begin(), vFiles.end(), sortByName);

        browser.m_vCachedEntries.clear();
        browser.m_vCachedEntries.reserve(vDirectories.size() + vFiles.size());
        browser.m_vCachedEntries.insert(browser.m_vCachedEntries.end(), vDirectories.begin(), vDirectories.end());
        browser.m_vCachedEntries.insert(browser.m_vCachedEntries.end(), vFiles.begin(), vFiles.end());
        browser.m_CachedPath = editor.m_CurrentAssetPath;
        browser.m_CachedFilter = editor.m_Preferences.m_AssetFilter;
        browser.m_CachedFavoriteFilter = browser.m_FavoriteFilter;
        browser.m_CachedSearch = search;
        browser.m_AssetListCacheDirty = false;
    }

    const std::vector<SLocalEntry>& vEntries = browser.m_vCachedEntries;

    if (!editor.m_SelectedAssetName.empty())
    {
        const auto selectedEntry = std::find_if(vEntries.begin(), vEntries.end(),
                                                [&editor](const SLocalEntry& entry)
                                                {
                                                    return entry.m_FileName == editor.m_SelectedAssetName;
                                                });
        editor.m_SelectedAssetIndex = selectedEntry == vEntries.end()
            ? -1
            : static_cast<int>(std::distance(vEntries.begin(), selectedEntry));
    }

    ImGui::BeginChild("AssetLeftTreePane", ImVec2(browser.m_LeftSidebarWidth, 0.0f), true);
    {
        ImGuiTreeNodeFlags favFlags = ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        const bool favOpen = ImGui::TreeNodeEx("##favorites_root", favFlags);
        {
            const ImVec2 favMin = ImGui::GetItemRectMin();
            const ImVec2 favMax = ImGui::GetItemRectMax();
            ImDrawList* pDraw = ImGui::GetWindowDrawList();
            const float arrowPad = ImGui::GetTreeNodeToLabelSpacing();
            const float iconSz = 15.0f;
            const ImVec2 starPos(favMin.x + arrowPad, favMin.y + (favMax.y - favMin.y - iconSz) * 0.5f);
            DrawStarIcon(pDraw, starPos, iconSz);
            pDraw->AddText(
                ImVec2(starPos.x + iconSz + 5.0f, favMin.y + (favMax.y - favMin.y - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text),
                "Favorites");
        }
        if (favOpen)
        {
            struct SFavItem
            {
                const char* m_pLabel;
                int m_FilterId;
            };
            const SFavItem aFavItems[] = {
                { "All GUI", 1 },
                { "All Materials", 2 },
                { "All Models", 3 },
                { "All Prefabs", 4 },
                { "All Scripts", 5 },
                { "All Scenes", 6 }
            };
            for (const SFavItem& fav : aFavItems)
            {
                ImGui::PushID(fav.m_FilterId);
                const bool isSelected = (browser.m_FavoriteFilter == fav.m_FilterId);
                if (ImGui::Selectable("##fav_item", isSelected, 0, ImVec2(0.0f, 19.0f)))
                {
                    browser.m_FavoriteFilter = (browser.m_FavoriteFilter == fav.m_FilterId) ? 0 : fav.m_FilterId;
                    browser.m_AssetListCacheDirty = true;
                    editor.m_SelectedAssetIndex = -1;
                    editor.m_SelectedAssetName.clear();
                }
                const ImVec2 rowMin = ImGui::GetItemRectMin();
                const ImVec2 rowMax = ImGui::GetItemRectMax();
                ImDrawList* pDraw = ImGui::GetWindowDrawList();
                const float iconSz = 14.0f;
                const ImVec2 lensPos(rowMin.x + 8.0f, rowMin.y + (rowMax.y - rowMin.y - iconSz) * 0.5f);
                DrawSearchLensIcon(pDraw, lensPos, iconSz);
                pDraw->AddText(
                    ImVec2(lensPos.x + iconSz + 6.0f, rowMin.y + (rowMax.y - rowMin.y - ImGui::GetTextLineHeight()) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_Text),
                    fav.m_pLabel);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();
        for (const SAssetFolderTreeNode& rootNode : browser.m_vCachedFolderRoots)
        {
            DrawFolderTreeRecursive(editor, rootNode);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("AssetRightContentPane", ImVec2(0.0f, 0.0f), false);

    if (browser.m_FavoriteFilter > 0)
    {
        const char* apFavTitles[] = {
            "Assets", "All GUI", "All Materials", "All Models", "All Prefabs", "All Scripts", "All Scenes"
        };
        if (ImGui::SmallButton("Favorites"))
        {
            browser.m_FavoriteFilter = 0;
            browser.m_AssetListCacheDirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled(">");
        ImGui::SameLine();
        ImGui::TextUnformatted(apFavTitles[std::clamp(browser.m_FavoriteFilter, 0, 6)]);
    }
    else
    {
        const auto& vBreadcrumbs = browser.m_vCachedBreadcrumbs;
        for (int index = 0; index < static_cast<int>(vBreadcrumbs.size()); ++index)
        {
            const std::string& label = vBreadcrumbs[index].first;
            const fs::path& rebuilt = vBreadcrumbs[index].second;
            ImGui::PushID(index);
            const ImVec2 btnMin = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::SmallButton(label.c_str());
            const ImVec2 btnMax = ImVec2(
                btnMin.x + ImGui::GetItemRectSize().x,
                btnMin.y + ImGui::GetItemRectSize().y);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
                {
                    if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                    {
                        const int entityIdx = *static_cast<const int*>(pPayload->Data);
                        SaveEntityBranchToDirectory(editor, entityIdx, rebuilt);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();

            if (editor.m_Ui.m_Layout.m_FileDragging && ImGui::IsMouseHoveringRect(btnMin, btnMax))
            {
                ImGui::GetWindowDrawList()->AddRectFilled(btnMin, btnMax, IM_COL32(100, 200, 100, 120));
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (editor.m_Ui.m_Layout.m_FileDragging && ImGui::IsMouseReleased(0) &&
                ImGui::IsMouseHoveringRect(btnMin, btnMax) && !drag.m_FilePath.empty())
            {
                const fs::path dest = rebuilt / drag.m_FilePath.filename();
                if (fs::exists(dest))
                {
                    browser.m_DuplicatePopupOpen = true;
                    browser.m_DuplicateName = drag.m_FilePath.filename().string();
                }
                else
                {
                    std::error_code ec;
                    fs::rename(drag.m_FilePath, dest, ec);
                    if (!ec)
                    {
                        if (CTextureMetadataStore::IsImageFile(drag.m_FilePath) &&
                            fs::exists(drag.m_FilePath.string() + ".meta"))
                        {
                            fs::rename(drag.m_FilePath.string() + ".meta", dest.string() + ".meta", ec);
                        }
                        editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                        browser.m_AssetListCacheDirty = true;
                        editor.m_SelectedAssetIndex = -1;
                    }
                }
                editor.m_Ui.m_Layout.m_FileDragging = false;
                drag.m_FilePath.clear();
                editor.m_Ui.m_Layout.m_DraggedFileIndex = -1;
            }

            if (clicked)
            {
                browser.m_FavoriteFilter = 0;
                editor.m_CurrentAssetPath = rebuilt;
                browser.m_AssetListCacheDirty = true;
                editor.m_SelectedAssetIndex = -1;
                editor.m_SelectedAssetName.clear();
                editor.m_Previews.InvalidateModelPreviews();
            }

            if (index + 1 < static_cast<int>(vBreadcrumbs.size()))
            {
                ImGui::SameLine();
                ImGui::TextDisabled(">");
                ImGui::SameLine();
            }
        }
    }
    ImGui::Separator();

    bool navigated = false;
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
                if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                {
                    const int idx = *static_cast<const int*>(pPayload->Data);
                    SaveEntityBranchToDirectory(editor, idx, editor.m_CurrentAssetPath);
                }
            }
            if (ImGui::IsDragDropPayloadBeingAccepted())
            {
                ImGui::GetWindowDrawList()->AddRect(
                    ImGui::GetItemRectMin(),
                    ImGui::GetItemRectMax(),
                    IM_COL32(80, 180, 255, 255),
                    2.0f,
                    0,
                    2.0f);
            }

            ImGui::EndDragDropTarget();
        }

        const char* pText = search.empty() ? lang.Word("empty_folder") : "No matching assets.";
        const ImVec2 ts = ImGui::CalcTextSize(pText);
        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(wp.x + (ws.x - ts.x) * 0.5f, wp.y + (ws.y - ts.y) * 0.5f),
            IM_COL32(255, 255, 255, 255),
            pText);

        if (ImGui::BeginPopupContextItem("AssetBgContextEmpty", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem(TextFormat("%s##empty_new_file", lang.Word("new_file"))))
            {
                create.m_CreatingFolder = false;
                create.m_aNewItemName[0] = '\0';
                create.m_Open = true;
            }

            if (ImGui::MenuItem(TextFormat("%s##empty_new_script", lang.Word("new_script"))))
            {
                create.m_CreatingFolder = false;
                create.m_CreatingScript = true;
                create.m_aNewItemName[0] = '\0';
                create.m_Open = true;
            }

            if (ImGui::MenuItem(TextFormat("%s##empty_new_folder", lang.Word("new_folder"))))
            {
                create.m_CreatingFolder = true;
                create.m_aNewItemName[0] = '\0';
                create.m_Open = true;
            }

            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }
    else
    {
        ImGui::BeginChild("AssetScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (ImGui::IsWindowHovered() && !editor.m_Ui.m_Layout.m_FileDragging)
        {
            if (ImGui::IsMouseClicked(0))
            {
                marquee.m_Start = ImGui::GetMousePos();
                marquee.m_End = marquee.m_Start;
                marquee.m_Active = true;
            }
            if (ImGui::IsMouseDown(0) && marquee.m_Active)
            {
                marquee.m_End = ImGui::GetMousePos();
            }
            if (ImGui::IsMouseReleased(0))
            {
                marquee.m_Active = false;
            }
        }

        editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex = -1;
        bool entityDroppedOnSubfolder = false;
        const bool isListMode = browser.m_ListViewMode;
        const float listRowHeight = 22.0f;
        const float listIconSize = 17.0f;
        const float itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
        const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
        const float itemWidth = ICON_SIZE + itemSpacingX;
        const float contentWidth = std::max(ImGui::GetContentRegionAvail().x, ICON_SIZE);
        const int columnCount = isListMode ? 1 : std::max(1, static_cast<int>((contentWidth + itemSpacingX) / itemWidth));
        const int rowCount = (static_cast<int>(vEntries.size()) + columnCount - 1) / columnCount;
        const float rowStepHeight = isListMode ? (listRowHeight + 2.0f) : (ICON_SIZE + 20.0f + itemSpacingY);
        ImGuiListClipper clipper;
        clipper.Begin(rowCount, rowStepHeight);
        while (clipper.Step())
        {
            const int firstItem = clipper.DisplayStart * columnCount;
            const int lastItem = std::min(clipper.DisplayEnd * columnCount, static_cast<int>(vEntries.size()));
            for (int i = firstItem; i < lastItem; ++i)
            {
                auto& entry = vEntries[i];
                const fs::path entryFullPath = !entry.m_FullPath.empty()
                    ? entry.m_FullPath
                    : (editor.m_CurrentAssetPath / entry.m_FileName);
                ImGui::PushID(i);
                ImGui::BeginGroup();

                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 size = isListMode
                    ? ImVec2(std::max(120.0f, ImGui::GetContentRegionAvail().x), listRowHeight)
                    : ImVec2(ICON_SIZE, ICON_SIZE + 20.0f);
                ImGui::InvisibleButton("asset_btn", size);

                if ((entry.m_IsDirectory || entry.m_IsPrefab) && ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
                    {
                        if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                        {
                            const int entityIdx = *static_cast<const int*>(pPayload->Data);
                            if (entry.m_IsDirectory)
                            {
                                SaveEntityBranchToDirectory(editor, entityIdx, entryFullPath);
                            }
                            else if (entry.m_IsPrefab)
                            {
                                if (CEntityFactory::SavePrefabToFile(editor.m_Scene, entityIdx, entryFullPath))
                                {
                                    const int syncedCount = CEntityFactory::SyncPrefabInstances(
                                        editor.m_Scene, editor.m_Assets, entryFullPath,
                                        editor.m_ComponentFactories, entityIdx);
                                    editor.SetStatusMessage(
                                        TextFormat("Updated prefab: %s (synced %d instance(s))",
                                                   entry.m_FileName.c_str(), syncedCount));
                                    editor.m_SceneDirty = true;
                                }
                            }
                            entityDroppedOnSubfolder = true;
                        }
                    }
                    if (ImGui::IsDragDropPayloadBeingAccepted())
                    {
                        ImGui::GetWindowDrawList()->AddRect(
                            pos,
                            ImVec2(pos.x + size.x, pos.y + size.y),
                            IM_COL32(80, 180, 255, 255),
                            2.0f,
                            0,
                            2.0f);
                    }
                    ImGui::EndDragDropTarget();
                }

                const bool itemHovered = ImGui::IsItemHovered();
                if (itemHovered)
                {
                    ImGui::SetTooltip("%s", entryFullPath.generic_string().c_str());
                }
                if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0))
                {
                    editor.m_SelectedAssetIndex = i;
                    editor.m_SelectedAssetName = entry.m_FileName;
                    editor.m_Scene.m_Selected = -1;
                    editor.m_Scene.m_vSelectedEntities.clear();
                }

                if (entry.m_IsDirectory && itemHovered && ImGui::IsMouseDoubleClicked(0))
                {
                    browser.m_FavoriteFilter = 0;
                    editor.m_CurrentAssetPath = entryFullPath;
                    browser.m_AssetListCacheDirty = true;
                    editor.m_SelectedAssetIndex = -1;
                    editor.m_SelectedAssetName.clear();
                    editor.m_Previews.InvalidateModelPreviews();

                    ImGui::EndGroup();
                    ImGui::PopID();
                    navigated = true;
                    break;
                }

                if (!entry.m_IsDirectory && itemHovered && ImGui::IsMouseDoubleClicked(0))
                {
                    if (entry.m_IsModel)
                    {
                        CModelAsset* pAsset = editor.m_Assets.FindModelByPath(entryFullPath, editor.m_ProjectPath);
                        if (pAsset)
                        {
                            OpenModelViewerForAsset(editor.m_Ui.m_ModelViewer, *pAsset);
                        }
                    }
                    else if (entry.m_IsMaterial)
                    {
                        OpenMaterialViewerForPath(editor, editor.m_Ui.m_MaterialViewer, entryFullPath);
                    }
                    else if (entry.m_IsPrefab)
                    {
                        SpawnPrefabInScene(editor, entryFullPath);
                    }
                    else
                    {
#ifdef _WIN32
                        ShellExecuteA(nullptr, "open", entryFullPath.string().c_str(), nullptr, nullptr, 1);
#elif defined(__APPLE__)
                        std::string cmd = "open \"" + entryFullPath.string() + "\"";
                        const int result = std::system(cmd.c_str());
                        if (result != 0)
                        {
                            TraceLog(LogLevel::Error, "EDITOR", TextFormat("Failed to open file: %s", entryFullPath.string().c_str()));
                        }
#elif defined(__linux__)
                        std::string cmd = "xdg-open \"" + entryFullPath.string() + "\"";
                        const int result = std::system(cmd.c_str());
                        if (result != 0)
                        {
                            TraceLog(LogLevel::Error, "EDITOR", TextFormat("Failed to open file: %s", entryFullPath.string().c_str()));
                        }
#endif
                    }
                }

                if (ImGui::BeginPopupContextItem("AssetContext"))
                {
                    if (entry.m_IsPrefab)
                    {
                        if (ImGui::MenuItem(TextFormat(lang.Word("spawn"), entry.m_FileName.c_str())))
                        {
                            SpawnPrefabInScene(editor, entryFullPath);
                        }
                        ImGui::Separator();
                    }

                    if (ImGui::MenuItem(lang.Word("open_file_explorer")))
                    {
                        OpenInSystemFileExplorer(entryFullPath, entry.m_IsDirectory);
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem(lang.Word("delete")))
                    {
                        editor.SaveState();

                        std::error_code ec;
                        if (entry.m_IsDirectory)
                        {
                            fs::remove_all(entryFullPath, ec);
                        }
                        else
                        {
                            fs::remove(entryFullPath, ec);
                        }
                        if (entry.m_IsMaterial)
                        {
                            fs::remove(entryFullPath.string() + ".meta", ec);
                        }
                        if (entry.m_IsTextureMeta)
                        {
                            fs::remove(CTextureMetadataStore::PathFromMeta(entryFullPath), ec);
                        }

                        editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                        browser.m_AssetListCacheDirty = true;
                        editor.m_SelectedAssetIndex = -1;
                        editor.m_SelectedAssetName.clear();
                        navigated = true;

                        ImGui::EndPopup();
                        ImGui::EndGroup();
                        ImGui::PopID();
                        break;
                    }

                    if (ImGui::MenuItem(lang.Word("rename")))
                    {
                        browser.m_RenameTarget = i;
                        const size_t copied = entry.m_FileName.copy(editor.m_Ui.m_MeshEdit.m_aRenameBuffer, sizeof(editor.m_Ui.m_MeshEdit.m_aRenameBuffer) - 1);
                        editor.m_Ui.m_MeshEdit.m_aRenameBuffer[copied] = '\0';
                        ImGui::OpenPopup(("%s##RenameAsset", lang.Word("rename_asset")));
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem(TextFormat("%s##empty_new_file", lang.Word("new_file"))))
                    {
                        create.m_CreatingFolder = false;
                        create.m_aNewItemName[0] = '\0';
                        create.m_Open = true;
                    }

                    if (ImGui::MenuItem(TextFormat("%s##empty_new_script", lang.Word("new_script"))))
                    {
                        create.m_CreatingFolder = false;
                        create.m_CreatingScript = true;
                        create.m_aNewItemName[0] = '\0';
                        create.m_Open = true;
                    }

                    if (ImGui::MenuItem(TextFormat("%s##empty_new_folder", lang.Word("new_folder"))))
                    {
                        create.m_CreatingFolder = true;
                        create.m_aNewItemName[0] = '\0';
                        create.m_Open = true;
                    }

                    ImGui::EndPopup();
                }

                bool selected = editor.m_SelectedAssetIndex == i;
                if (marquee.m_Active && (fabsf(marquee.m_Start.x - marquee.m_End.x) > 5.0f || fabsf(marquee.m_Start.y - marquee.m_End.y) > 5.0f))
                {
                    const ImVec2 min(std::min(marquee.m_Start.x, marquee.m_End.x), std::min(marquee.m_Start.y, marquee.m_End.y));
                    const ImVec2 max(std::max(marquee.m_Start.x, marquee.m_End.x), std::max(marquee.m_Start.y, marquee.m_End.y));
                    if (!(pos.x + size.x < min.x || pos.x > max.x || pos.y + size.y < min.y || pos.y > max.y))
                    {
                        selected = true;
                    }
                }

                if (selected)
                {
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(70, 125, 220, 110), 2.0f);
                }
                else if (itemHovered)
                {
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 22), 2.0f);
                }

                if (isListMode)
                {
                    ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                    const ImVec2 iconPos(pos.x + 6.0f, pos.y + (listRowHeight - listIconSize) * 0.5f);
                    if (entry.m_IsDirectory)
                    {
                        DrawFolderSmallIcon(pDrawList, iconPos, listIconSize);
                    }
                    else if (entry.m_IsPrefab)
                    {
                        DrawPrefabCubeIcon(pDrawList, iconPos, listIconSize);
                    }
                    else if (entry.m_IsMaterial)
                    {
                        DrawMaterialSphereIcon(pDrawList, iconPos, listIconSize);
                    }
                    else if (entry.m_IsScript)
                    {
                        DrawScriptDocIcon(pDrawList, iconPos, listIconSize);
                    }
                    else if (entry.m_IsScene)
                    {
                        DrawSceneIcon(pDrawList, iconPos, listIconSize);
                    }
                    else if (entry.m_IsModel)
                    {
                        DrawPrefabCubeIcon(pDrawList, iconPos, listIconSize);
                    }
                    else if (entry.m_IsImage)
                    {
                        const std::string full = entryFullPath.string();
                        const Texture2D* pTexture = editor.m_Textures.Load(full);
                        if (pTexture && pTexture->id != 0)
                        {
                            ImGui::SetCursorScreenPos(iconPos);
                            QcImGuiImage(pTexture, ImVec2(listIconSize, listIconSize));
                        }
                        else
                        {
                            pDrawList->AddRectFilled(iconPos, ImVec2(iconPos.x + listIconSize, iconPos.y + listIconSize), IM_COL32(140, 100, 220, 255), 2.0f);
                        }
                    }
                    else
                    {
                        pDrawList->AddRectFilled(
                            ImVec2(iconPos.x + 2.0f, iconPos.y + 1.0f),
                            ImVec2(iconPos.x + listIconSize - 2.0f, iconPos.y + listIconSize - 1.0f),
                            IM_COL32(180, 188, 200, 240), 1.5f);
                    }

                    const char* pDisplayLabel = !entry.m_DisplayLabel.empty()
                        ? entry.m_DisplayLabel.c_str()
                        : entry.m_FileName.c_str();
                    pDrawList->AddText(
                        ImVec2(iconPos.x + listIconSize + 7.0f, pos.y + (listRowHeight - ImGui::GetTextLineHeight()) * 0.5f),
                        ImGui::GetColorU32(ImGuiCol_Text),
                        pDisplayLabel);
                }
                else
                {
                    ImGui::SetCursorScreenPos(pos);
                    if (entry.m_IsDirectory)
                    {
                        const Texture texture = entry.m_IsDirectoryEmpty ? editor.m_Previews.IconFolder() : editor.m_Previews.IconFullFolder();
                        if (texture.id != 0)
                        {
                            QcImGuiImage(&texture, ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                        else
                        {
                            ImGui::Button(lang.Word("folder"), ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                    }
                    else if (entry.m_IsImage)
                    {
                        const std::string full = entryFullPath.string();
                        const Texture2D* pTexture = nullptr;
                        if (ImGui::IsRectVisible(pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE)))
                        {
                            pTexture = editor.m_Textures.Load(full);
                        }
                        else
                        {
                            pTexture = editor.m_Textures.Find(full);
                        }
                        if (pTexture)
                        {
                            QcImGuiImage(pTexture, ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                    }
                    else if (entry.m_IsModel)
                    {
                        const std::string full = entryFullPath.string();
                        Texture previewTexture = { 0 };
                        bool loadFailed = false;

                        if (editor.m_Previews.HasModelPreview(full))
                        {
                            previewTexture = editor.m_Previews.ModelPreview(full);
                        }
                        else if (ImGui::IsRectVisible(pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE)))
                        {
                            CModelAsset* pAsset = editor.m_Assets.FindModelByPath(entryFullPath, editor.m_ProjectPath);
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
                            QcImGuiImage(&previewTexture, ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                        else
                        {
                            ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                            pDrawList->AddRectFilled(pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE), loadFailed ? IM_COL32(80, 80, 90, 255) : IM_COL32(100, 100, 120, 255));
                            if (loadFailed)
                            {
                                pDrawList->AddLine(ImVec2(pos.x + 10, pos.y + 10), ImVec2(pos.x + ICON_SIZE - 10, pos.y + ICON_SIZE - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                                pDrawList->AddLine(ImVec2(pos.x + ICON_SIZE - 10, pos.y + 10), ImVec2(pos.x + 10, pos.y + ICON_SIZE - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                            }
                        }
                    }
                    else if (entry.m_IsMaterial)
                    {
                        const std::string full = entryFullPath.string();
                        Texture preview = { 0 };

                        if (editor.m_Previews.HasMaterialPreview(full))
                        {
                            preview = editor.m_Previews.MaterialPreview(full);
                        }
                        else if (ImGui::IsRectVisible(pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE)))
                        {
                            preview = GetMaterialPreview(editor, full);
                        }

                        if (preview.id != 0)
                        {
                            QcImGuiImage(&preview, ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                        else
                        {
                            DrawMaterialSphereIcon(ImGui::GetWindowDrawList(), pos, ICON_SIZE);
                        }
                    }
                    else if (entry.m_IsPrefab)
                    {
                        DrawPrefabCubeIcon(ImGui::GetWindowDrawList(), pos, ICON_SIZE);
                    }
                    else if (entry.m_IsScript)
                    {
                        DrawScriptDocIcon(ImGui::GetWindowDrawList(), pos, ICON_SIZE);
                    }
                    else if (entry.m_IsScene)
                    {
                        DrawSceneIcon(ImGui::GetWindowDrawList(), pos, ICON_SIZE);
                    }
                    else
                    {
                        if (editor.m_Previews.IconFile().id != 0)
                        {
                            QcImGuiImage(&editor.m_Previews.IconFile(), ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                        else
                        {
                            ImGui::Button(entry.m_FileName.empty() ? "file" : entry.m_FileName.c_str(), ImVec2(ICON_SIZE, ICON_SIZE));
                        }
                    }

                    if (!entry.m_IsDirectory && !entry.m_FileName.empty())
                    {
                        ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                        std::string extDisplay = entry.m_IsPrefab ? "PFB" : entry.m_Extension;
                        if (extDisplay.size() > 3)
                        {
                            extDisplay.resize(3);
                        }
                        const ImVec2 extTextSize = ImGui::CalcTextSize(extDisplay.c_str());
                        const ImVec2 extPos(pos.x + ICON_SIZE - extTextSize.x - 2.0f, pos.y + ICON_SIZE - extTextSize.y - 2.0f);
                        const ImU32 badgeBg = entry.m_IsPrefab ? IM_COL32(30, 120, 200, 220) : IM_COL32(0, 0, 0, 180);
                        pDrawList->AddRectFilled(ImVec2(extPos.x - 2.0f, extPos.y - 1.0f), ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE), badgeBg);
                        pDrawList->AddText(extPos, IM_COL32(255, 255, 255, 255), extDisplay.c_str());
                    }
                }

                if (itemHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editor.m_Ui.m_Layout.m_FileDragging)
                {
                    drag.m_FileName = entry.m_FileName;
                    drag.m_StartPos = ImGui::GetMousePos();

                    editor.m_Ui.m_Layout.m_DraggedFileIndex = i;
                    drag.m_FilePath = entryFullPath;
                }

                if (drag.m_FileName == entry.m_FileName && ImGui::IsMouseDown(0))
                {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    ImVec2 delta = ImVec2(mousePos.x - drag.m_StartPos.x, mousePos.y - drag.m_StartPos.y);
                    if (fabsf(delta.x) > 5.0f || fabsf(delta.y) > 5.0f)
                    {
                        editor.m_Ui.m_Layout.m_FileDragging = true;
                        editor.m_Ui.m_Layout.m_DraggedFileIndex = i;

                        if (entry.m_IsModel)
                        {
                            editor.m_Ui.m_Layout.m_SceneAssetDragging = true;
                            editor.m_Ui.m_Layout.m_DraggedSceneAssetName = entryFullPath.string();
                        }

                        if (entry.m_Extension == "prefab")
                        {
                            editor.m_Ui.m_Layout.m_SceneAssetDragging = true;
                            editor.m_Ui.m_Layout.m_DraggedSceneAssetName = entryFullPath.string();
                        }

                        if (CModelService::IsModelFile(fs::path(entry.m_FileName)))
                        {
                            const std::string assetName = CAssetLibrary::AssetNameForPath(fs::path(editor.m_ProjectPath), entryFullPath);
                            editor.m_Ui.m_Layout.m_SceneAssetDragging = true;
                            editor.m_Ui.m_Layout.m_DraggedSceneAssetName = assetName;
                        }
                    }
                }

                const bool hasValidDraggedFile = editor.m_Ui.m_Layout.m_DraggedFileIndex >= 0 &&
                    editor.m_Ui.m_Layout.m_DraggedFileIndex < static_cast<int>(vEntries.size());
                if (entry.m_IsDirectory && editor.m_Ui.m_Layout.m_FileDragging && hasValidDraggedFile &&
                    vEntries[editor.m_Ui.m_Layout.m_DraggedFileIndex].m_FileName != entry.m_FileName)
                {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    bool mouseOverFolder = mousePos.x >= pos.x && mousePos.x <= pos.x + size.x &&
                        mousePos.y >= pos.y && mousePos.y <= pos.y + size.y;

                    if (mouseOverFolder)
                    {
                        ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(100, 200, 100, 150));
                        editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex = i;
                    }
                }

                if (editor.m_Ui.m_Layout.m_SceneAssetDragging &&
                    !entry.m_IsDirectory && (entry.m_IsModel || entry.m_IsMaterial || entry.m_IsPrefab))
                {
                    if (entry.m_IsPrefab)
                    {
                        const std::string fullPrefabPath = entryFullPath.string();
                        if (editor.m_Ui.m_Layout.m_DraggedSceneAssetName == fullPrefabPath)
                        {
                            ImGui::SetTooltip(lang.Word("spawn"), entry.m_FileName.c_str());
                        }
                    }
                    else
                    {
                        const std::string assetName = CAssetLibrary::AssetNameForPath(fs::path(editor.m_ProjectPath), entryFullPath);

                        if (editor.m_Ui.m_Layout.m_SceneAssetDragging && editor.m_Ui.m_Layout.m_DraggedSceneAssetName == assetName)
                        {
                            if (entry.m_IsModel)
                            {
                                ImGui::SetTooltip(lang.Word("spawn"), editor.m_Ui.m_Layout.m_DraggedSceneAssetName.c_str());
                            }
                        }
                    }
                }

                if (!isListMode)
                {
                    std::string label = entry.m_FileName;
                    if (label.size() > 10)
                    {
                        label = label.substr(0, 8) + "..";
                    }
                    const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
                    ImGui::SetCursorScreenPos(ImVec2(pos.x + (ICON_SIZE - labelSize.x) * 0.5f, pos.y + ICON_SIZE + 2.0f));
                    ImGui::TextUnformatted(label.c_str());
                }
                ImGui::EndGroup();

                if (!isListMode && (i + 1 < lastItem))
                {
                    ImGui::SameLine();
                }
                ImGui::PopID();
            }

            if (navigated)
            {
                break;
            }
        }
        clipper.End();

        const ImVec2 scrollWindowPos = ImGui::GetWindowPos();
        const ImVec2 scrollWindowSize = ImGui::GetWindowSize();
        const ImRect assetDropZone(
            scrollWindowPos,
            ImVec2(scrollWindowPos.x + scrollWindowSize.x, scrollWindowPos.y + scrollWindowSize.y));
        if (!entityDroppedOnSubfolder &&
            ImGui::BeginDragDropTargetCustom(assetDropZone, ImGui::GetID("##AssetBrowserEntityDropZone")))
        {
            if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
            {
                if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                {
                    const int idx = *static_cast<const int*>(pPayload->Data);
                    SaveEntityBranchToDirectory(editor, idx, editor.m_CurrentAssetPath);
                }
            }
            if (ImGui::IsDragDropPayloadBeingAccepted())
            {
                ImGui::GetWindowDrawList()->AddRect(
                    assetDropZone.Min,
                    assetDropZone.Max,
                    IM_COL32(80, 180, 255, 255),
                    2.0f,
                    0,
                    2.0f);
            }
            ImGui::EndDragDropTarget();
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

        if (!entityDroppedOnSubfolder && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
            {
                if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                {
                    const int idx = *static_cast<const int*>(pPayload->Data);
                    SaveEntityBranchToDirectory(editor, idx, editor.m_CurrentAssetPath);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (marquee.m_Active && (fabsf(marquee.m_Start.x - marquee.m_End.x) > 5.0f || fabsf(marquee.m_Start.y - marquee.m_End.y) > 5.0f))
        {
            const ImVec2 min(std::min(marquee.m_Start.x, marquee.m_End.x), std::min(marquee.m_Start.y, marquee.m_End.y));
            const ImVec2 max(std::max(marquee.m_Start.x, marquee.m_End.x), std::max(marquee.m_Start.y, marquee.m_End.y));
            ImGui::GetForegroundDrawList()->AddRectFilled(min, max, IM_COL32(80, 140, 255, 40));
        }

        if (editor.m_Ui.m_Layout.m_FileDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex >= 0 &&
                editor.m_Ui.m_Layout.m_DraggedFileIndex >= 0 &&
                editor.m_Ui.m_Layout.m_DraggedFileIndex < static_cast<int>(vEntries.size()) &&
                editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex < static_cast<int>(vEntries.size()) &&
                editor.m_Ui.m_Layout.m_DraggedFileIndex != editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex &&
                vEntries[editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex].m_IsDirectory)
            {
                editor.SaveState();
                const fs::path sourcePath = editor.m_CurrentAssetPath / vEntries[editor.m_Ui.m_Layout.m_DraggedFileIndex].m_FileName;
                const fs::path destDir = editor.m_CurrentAssetPath / vEntries[editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex].m_FileName;
                const fs::path destPath = destDir / fs::path(sourcePath).filename();

                if (fs::exists(destPath))
                {
                    browser.m_DuplicatePopupOpen = true;
                    browser.m_DuplicateName = sourcePath.filename().string();
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
                        browser.m_AssetListCacheDirty = true;
                        editor.m_SelectedAssetIndex = -1;
                    }
                }
            }
            editor.m_Ui.m_Layout.m_FileDragging = false;
            editor.m_Ui.m_Layout.m_DraggedFileIndex = -1;
            editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex = -1;
            drag.m_FileName.clear();
            drag.m_StartPos = ImVec2(0, 0);
        }

        if (browser.m_RenameTarget >= 0)
        {
            ImGui::OpenPopup(("%s##RenameAsset", lang.Word("rename_asset")));
            browser.m_RenameTarget = -2;
        }

        if (browser.m_RenameTarget == -2 && ImGui::BeginPopupModal(("%s##RenameAsset", lang.Word("rename_asset")), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("##rename", editor.m_Ui.m_MeshEdit.m_aRenameBuffer, IM_ARRAYSIZE(editor.m_Ui.m_MeshEdit.m_aRenameBuffer));

            if (ImGui::Button(lang.Word("ok")))
            {
                const std::string newFilename = editor.m_Ui.m_MeshEdit.m_aRenameBuffer;
                if (browser.m_LastAppliedRename != newFilename)
                {
                    editor.SaveState();
                    browser.m_LastAppliedRename = newFilename;

                    if (editor.m_SelectedAssetIndex >= 0 && editor.m_SelectedAssetIndex < static_cast<int>(vEntries.size()))
                    {
                        const fs::path oldPath = editor.m_CurrentAssetPath / vEntries[editor.m_SelectedAssetIndex].m_FileName;
                        const fs::path newPath = editor.m_CurrentAssetPath / editor.m_Ui.m_MeshEdit.m_aRenameBuffer;

                        if (editor.m_Ui.m_MeshEdit.m_aRenameBuffer[0] != '\0' && oldPath != newPath && fs::exists(oldPath))
                        {
                            try
                            {
                                fs::rename(oldPath, newPath);
                                if (CTextureMetadataStore::IsImageFile(oldPath) && fs::exists(oldPath.string() + ".meta"))
                                {
                                    fs::rename(oldPath.string() + ".meta", newPath.string() + ".meta");
                                }
                                editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                                browser.m_AssetListCacheDirty = true;
                                editor.m_SelectedAssetIndex = -1;
                                editor.m_SelectedAssetName.clear();
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }

                browser.m_RenameTarget = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(lang.Word("cancel")))
            {
                browser.m_RenameTarget = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupContextItem("AssetBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem(TextFormat("%s##bg_new_file", lang.Word("new_file"))))
            {
                create.m_CreatingFolder = false;
                create.m_aNewItemName[0] = '\0';
                create.m_Open = true;
            }

            if (ImGui::MenuItem(TextFormat("%s##bg_new_script", lang.Word("new_script"))))
            {
                create.m_CreatingFolder = false;
                create.m_CreatingScript = true;
                create.m_aNewItemName[0] = '\0';
                create.m_Open = true;
            }

            if (ImGui::MenuItem(TextFormat("%s##bg_new_folder", lang.Word("new_folder"))))
            {
                create.m_CreatingFolder = true;
                create.m_aNewItemName[0] = '\0';
                create.m_Open = true;
            }

            ImGui::EndPopup();
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            editor.m_Ui.m_Layout.m_FileDragging = false;
            editor.m_Ui.m_Layout.m_SceneAssetDragging = false;
            editor.m_Ui.m_Layout.m_DraggedFileIndex = -1;
            editor.m_Ui.m_Layout.m_DraggedTargetFolderIndex = -1;
            drag.m_FileName.clear();
            drag.m_FilePath.clear();
            drag.m_StartPos = ImVec2(0, 0);
        }

        if (browser.m_DuplicatePopupOpen)
        {
            ImGui::OpenPopup(("%s##DuplicateName", lang.Word("move_error")));
            browser.m_DuplicatePopupOpen = false;
        }

        if (ImGui::BeginPopupModal(("%s##DuplicateName", lang.Word("move_error")), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("%s", lang.Word("unable_to_move"));
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", browser.m_DuplicateName.c_str());
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
    }

    if (create.m_Open && !navigated)
    {
        ImGui::OpenPopup(popupName.c_str());
        create.m_Open = false;
    }

    if (ImGui::BeginPopupModal(popupName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const char* pModalTitle = create.m_CreatingScript ? lang.Word("new_script") : (create.m_CreatingFolder ? lang.Word("new_folder") : lang.Word("new_file"));
        ImGui::TextUnformatted(pModalTitle);
        const char* pFieldLabel = create.m_CreatingScript ? lang.Word("script_name") : lang.Word("name");
        ImGui::InputText(pFieldLabel, create.m_aNewItemName, IM_ARRAYSIZE(create.m_aNewItemName));
        ImGui::Spacing();

        if (ImGui::Button(lang.Word("ok"), ImVec2(120, 0)))
        {
            if (create.m_aNewItemName[0] != '\0')
            {
                fs::path path;

                if (create.m_CreatingScript)
                {
                    std::string scriptName = create.m_aNewItemName;
                    if (scriptName.size() > 4 && scriptName.substr(scriptName.size() - 4) == ".hpp")
                    {
                        scriptName = scriptName.substr(0, scriptName.size() - 4);
                    }
                    else if (scriptName.size() > 4 && scriptName.substr(scriptName.size() - 4) == ".cpp")
                    {
                        scriptName = scriptName.substr(0, scriptName.size() - 4);
                    }

                    CreateScriptFiles(editor.m_CurrentAssetPath, scriptName);
                    editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                    browser.m_AssetListCacheDirty = true;
                    create.m_CreatingScript = false;
                }
                else if (create.m_CreatingFolder)
                {
                    path = editor.m_CurrentAssetPath / create.m_aNewItemName;

                    int suffix = 1;
                    while (fs::exists(path))
                    {
                        path = editor.m_CurrentAssetPath / (std::string(create.m_aNewItemName) + "_" + std::to_string(suffix++));
                    }

                    std::error_code ec;
                    fs::create_directory(path, ec);
                }
                else
                {
                    std::string filename = create.m_aNewItemName;
                    if (filename.find('.') == std::string::npos)
                    {
                        filename += ".txt";
                    }

                    path = editor.m_CurrentAssetPath / filename;

                    int suffix = 1;
                    while (fs::exists(path))
                    {
                        path = editor.m_CurrentAssetPath / (std::string(create.m_aNewItemName) + "_" + std::to_string(suffix++) + ".txt");
                    }

                    std::ofstream f(path);
                    f.close();
                }

                editor.m_Assets.RequestRefresh(editor.m_ProjectPath);
                browser.m_AssetListCacheDirty = true;
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

    ImGui::EndChild();
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
    std::vector<std::pair<int, std::string>> vEntityReferences;
    SAssetBrowserState& browser = editor.m_Ui.m_AssetBrowser;
    if (browser.m_DependencyCachePath != selected.string())
    {
        browser.m_DependencyCachePath = selected.string();
        browser.m_CachedDependencyRelativePath =
            selected.lexically_normal().lexically_relative(resourceDir.lexically_normal()).generic_string();
        std::error_code selEc;
        browser.m_CachedSelectedIsRegularFile = fs::is_regular_file(selected, selEc) && !selEc;
        const fs::path texPath = CTextureMetadataStore::PathFromMeta(selected);
        browser.m_CachedSelectedIsTexture = CTextureMetadataStore::IsImageFile(texPath) && fs::exists(texPath, selEc) && !selEc;
        browser.m_vCachedFileDependencies.clear();
        browser.m_DependencyCacheReady = false;
    }

    const std::string& selectedRelative = browser.m_CachedDependencyRelativePath;
    const bool isMaterial = Lowercase(selected.extension().string()) == ".mtl";
    const bool isObjModel = Lowercase(selected.extension().string()) == ".obj";
    const bool isPrefab = Lowercase(selected.extension().string()) == ".prefab";
    const bool isTexture = CTextureMetadataStore::IsImageFile(selected);
    unsigned int textureId = 0;
    if (isTexture && !selectedRelative.empty())
    {
        for (const STextureOption& option : editor.m_Assets.Textures())
        {
            if (option.m_Name == selectedRelative)
            {
                textureId = option.m_Texture.id;
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
            fs::path(pMesh->m_AssetName).lexically_normal().generic_string() == selectedRelative)
        {
            vEntityReferences.emplace_back(entityIndex, "Model used by: " + entity.m_Name);
        }

        if (!pMaterial)
        {
            continue;
        }

        if (isMaterial && !pMaterial->m_TextureName.empty() &&
            fs::path(pMaterial->m_TextureName).lexically_normal() == selected.lexically_normal())
        {
            vEntityReferences.emplace_back(entityIndex, "Material used by: " + entity.m_Name);
        }

        if (isTexture && textureId != 0 && pMaterial->m_Texture.id == textureId)
        {
            vEntityReferences.emplace_back(entityIndex, "Texture used by: " + entity.m_Name);
        }
    }

    if (!browser.m_DependencyCacheReady && isPrefab)
    {
        std::ifstream prefabFile(selected);
        if (prefabFile.is_open())
        {
            std::stringstream buffer;
            buffer << prefabFile.rdbuf();
            const quark::SParsedSceneDocument parsed = quark::CSceneDocument::ParsePrefab(buffer.str());
            if (parsed.m_IsValid && parsed.m_Document.contains("entities"))
            {
                const auto& entities = parsed.m_Document["entities"];
                browser.m_vCachedFileDependencies.push_back(
                    "Entities in prefab: " + std::to_string(entities.size()));
                for (const auto& ej : entities)
                {
                    if (!ej.contains("components") || !ej["components"].is_array())
                    {
                        continue;
                    }
                    for (const auto& comp : ej["components"])
                    {
                        if (!comp.contains("type") || !comp.contains("data"))
                        {
                            continue;
                        }
                        const std::string compType = comp.value("type", "");
                        const auto& data = comp["data"];
                        if (compType == "Mesh" && data.contains("asset_name") && data["asset_name"].is_string())
                        {
                            const std::string assetName = data["asset_name"].get<std::string>();
                            if (!assetName.empty())
                            {
                                browser.m_vCachedFileDependencies.push_back("Uses model: " + assetName);
                            }
                        }
                        else if (compType == "Material")
                        {
                            const std::string albedo = data.value("albedo_texture_name", "");
                            const std::string tex = data.value("texture_name", "");
                            if (!albedo.empty())
                            {
                                browser.m_vCachedFileDependencies.push_back("Uses texture: " + albedo);
                            }
                            else if (!tex.empty())
                            {
                                browser.m_vCachedFileDependencies.push_back("Uses material/texture: " + tex);
                            }
                        }
                    }
                }
            }
        }
    }
    else if (!browser.m_DependencyCacheReady && isObjModel)
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
                browser.m_vCachedFileDependencies.push_back("Uses material: " +
                                                            (selected.parent_path() / materialName).lexically_normal().string());
            }
        }
    }
    else if (!browser.m_DependencyCacheReady && isMaterial)
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
                browser.m_vCachedFileDependencies.push_back("Uses texture: " +
                                                            (selected.parent_path() / fs::path(textureName)).lexically_normal().string());
            }
        }
    }
    else if (!browser.m_DependencyCacheReady && isTexture)
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
                    browser.m_vCachedFileDependencies.push_back("Used by material: " + entry.path().string());
                }
            }
        }
    }
    browser.m_DependencyCacheReady = true;

    ImGui::Separator();
    ImGui::TextUnformatted(lang.Word("dependencies"));
    for (const std::string& reference : browser.m_vCachedFileDependencies)
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
    if (browser.m_vCachedFileDependencies.empty() && vEntityReferences.empty())
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

    SAssetBrowserState& browser = editor.m_Ui.m_AssetBrowser;
    fs::path selected = editor.m_CurrentAssetPath / editor.m_SelectedAssetName;
    bool isDirectory = false;
    if (editor.m_SelectedAssetIndex >= 0 &&
        editor.m_SelectedAssetIndex < static_cast<int>(browser.m_vCachedEntries.size()))
    {
        const SLocalEntry& cachedEntry = browser.m_vCachedEntries[editor.m_SelectedAssetIndex];
        if (!cachedEntry.m_FullPath.empty())
        {
            selected = cachedEntry.m_FullPath;
        }
        isDirectory = cachedEntry.m_IsDirectory;
    }

    if (isDirectory)
    {
        return;
    }

    const fs::path dependencyPath = selected.extension() == ".meta"
        ? CTextureMetadataStore::PathFromMeta(selected)
        : selected;
    DrawAssetDependencies(editor, dependencyPath);
    if (!browser.m_CachedSelectedIsRegularFile || !browser.m_CachedSelectedIsTexture)
    {
        return;
    }

    const fs::path texturePath = CTextureMetadataStore::PathFromMeta(selected);
    static std::string s_CachedMetaPath;
    static STextureMeta s_CachedMeta;
    const std::string texturePathStr = texturePath.string();
    if (s_CachedMetaPath != texturePathStr)
    {
        s_CachedMetaPath = texturePathStr;
        s_CachedMeta = {};
        if (!CTextureMetadataStore::Load(texturePath, s_CachedMeta))
        {
            CTextureMetadataStore::Ensure(texturePath);
            CTextureMetadataStore::Load(texturePath, s_CachedMeta);
        }
    }
    STextureMeta& meta = s_CachedMeta;

    ImGui::Separator();
    ImGui::TextUnformatted(lang.Word("texture_importer"));
    ImGui::TextWrapped("%s", texturePath.filename().string().c_str());
    ImGui::TextDisabled("GUID: %s", meta.m_Guid.c_str());

    bool changed = false;
    changed |= ImGui::Checkbox(lang.Word("enable_mip_maps"), &meta.m_EnableMipMap);
    changed |= ImGui::Checkbox(lang.Word("srgb_texture"), &meta.m_SrgbTexture);
    changed |= ImGui::Checkbox(lang.Word("readable"), &meta.m_IsReadable);
    changed |= ImGui::Checkbox(lang.Word("alpha_transparency"), &meta.m_AlphaIsTransparency);
    const char* apFilterModes[] = { lang.Word("nearest"), lang.Word("linear") };
    const char* apWrapModes[] = { lang.Word("repeat"), lang.Word("clamp") };
    const char* apSpriteModes[] = { lang.Word("none"), lang.Word("single"), lang.Word("multiple") };
    const char* apTextureTypes[] = {
        lang.Word("default"), lang.Word("normal"), lang.Word("sprite"), lang.Word("cursor"),
        lang.Word("cookie"), lang.Word("lightmap"), lang.Word("shadowmask"), lang.Word("directional"),
        lang.Word("single_channel")
    };
    changed |= ImGui::Combo(lang.Word("filter_mode"), &meta.m_FilterMode, apFilterModes, IM_ARRAYSIZE(apFilterModes));
    changed |= ImGui::Combo(lang.Word("wrap_u"), &meta.m_WrapU, apWrapModes, IM_ARRAYSIZE(apWrapModes));
    changed |= ImGui::Combo(lang.Word("wrap_v"), &meta.m_WrapV, apWrapModes, IM_ARRAYSIZE(apWrapModes));
    changed |= ImGui::SliderInt(lang.Word("max_texture_size"), &meta.m_MaxTextureSize, 32, 8192);
    changed |= ImGui::SliderInt(lang.Word("compression_quality"), &meta.m_CompressionQuality, 0, 100);
    changed |= ImGui::Combo(lang.Word("sprite_mode"), &meta.m_SpriteMode, apSpriteModes, IM_ARRAYSIZE(apSpriteModes));
    changed |= ImGui::Combo(lang.Word("texture_type"), &meta.m_TextureType, apTextureTypes, IM_ARRAYSIZE(apTextureTypes));

    if (changed)
    {
        CTextureMetadataStore::Save(texturePath, meta);
        const std::string relativeName = fs::relative(texturePath, fs::path(editor.m_ProjectPath) / "resources").generic_string();
        for (auto& option : editor.m_Assets.Textures())
        {
            if (option.m_Name == relativeName)
            {
                CTextureMetadataStore::ApplyToTexture(option.m_Texture, meta);
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
