#include "editor/editor.h"

#include "editor/editor_assets.h"
#include "editor/editor_entity_clipboard.h"
#include "editor/editor_entity_commands.h"
#include "editor/editor_scene_file.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "project.h"
#include "tex.h"
#include "imgui.h"
#include "editor/editor_preferences.h"
#include "engine/scene_document.h"
#include "engine/scene_runtime.h"

#include <chrono>
#include <exception>

using namespace qc;

namespace fs = std::filesystem;

namespace
{

void PushHistory(std::stack<quark::SSceneSnapshot>& stack, const quark::SSceneSnapshot& snapshot,
    int historyLimit)
{
    stack.push(snapshot);
    while (stack.size() > static_cast<size_t>(historyLimit))
    {
        stack.pop();
    }
}

bool RestoreSnapshot(CScene& scene, const quark::SParsedSceneDocument& document,
                     const quark::SSceneSnapshot& snapshot,
                     CAssetLibrary& assets, CLightRegistry& lights,
                     const CComponentFactoryRegistry& factories,
                     const std::vector<std::optional<SEditableMeshBuildData>>& vGeometry)
{
    CScene previousScene;
    previousScene.m_vEntities = std::move(scene.m_vEntities);
    try
    {
        if (!quark::CSceneDocument::Deserialize(document, scene, factories))
        {
            scene.m_vEntities = std::move(previousScene.m_vEntities);
            return false;
        }
    }
    catch (const std::exception& exception)
    {
        TraceLog(LogLevel::Error, "EDITOR", TextFormat(
            "Failed to deserialize history snapshot: %s", exception.what()));
        scene.m_vEntities = std::move(previousScene.m_vEntities);
        return false;
    }

    scene.m_Selected = snapshot.Selected;
    scene.m_vSelectedEntities = snapshot.vSelectedEntities;
    quark::CSceneRuntime::RestoreSceneEntityModels(scene, assets, &previousScene, &vGeometry);
    quark::CSceneRuntime::ResetSceneLightRuntime(scene, lights);
    return true;
}

} // anonymous

CEditor::CEditor()
{
    m_Assets.SetTextMesh(m_Text);
    m_Assets.SetTaskPool(m_CpuTaskPool);
}

CEditor::~CEditor() = default;

void CEditor::Unload()
{
    if (m_HistoryRestoreFuture.valid())
    {
        m_HistoryRestoreFuture.wait();
        m_HistoryRestoreFuture = {};
    }
    m_PendingHistorySnapshot.reset();
    m_PendingCurrentSnapshot.reset();
    m_PendingParsedSceneDocument.reset();
    m_vPendingGeometryFutures.clear();
    m_vPendingGeometryResults.clear();
    m_PluginCommandActive = false;
    m_Scene.ReleaseResources();
    m_Ui.Unload();
    CleanupAssetsUi(*this);
    m_Textures.Unload();
    m_Previews.Unload();
    m_Assets.Unload();
    m_Text.Unload();

    while (!m_RedoStack.empty())
    {
        m_RedoStack.pop();
    }
    while (!m_UndoStack.empty())
    {
        m_UndoStack.pop();
    }
}

void CEditor::SaveState()
{
    if (IsHistoryRestorePending())
    {
        return;
    }
    m_SceneDirty = true;
    PushHistory(m_UndoStack, quark::CSceneDocument::CaptureSnapshot(m_Scene), m_Preferences.m_UndoHistoryLimit);
    while (!m_RedoStack.empty())
    {
        m_RedoStack.pop();
    }
}

void CEditor::BeginPluginCommand(const char* pDescription)
{
    (void)pDescription;
    if (m_PluginCommandActive)
    {
        return;
    }
    SaveState();
    m_PluginCommandActive = true;
}

void CEditor::EndPluginCommand()
{
    m_PluginCommandActive = false;
}

void CEditor::SetStatusMessage(const char* pMessage)
{
    m_StatusMessage = pMessage ? pMessage : "";
}

void CEditor::RequestSceneRedraw()
{
    m_SceneRedrawRequested = true;
}

bool CEditor::IsHistoryRestorePending() const
{
    return m_PendingHistorySnapshot.has_value();
}

void CEditor::StartHistoryRestore(bool undo)
{
    if (IsHistoryRestorePending())
    {
        return;
    }

    std::stack<quark::SSceneSnapshot>& source = undo ? m_UndoStack : m_RedoStack;
    if (source.empty())
    {
        return;
    }

    m_PendingHistorySnapshot = source.top();
    m_PendingCurrentSnapshot = quark::CSceneDocument::CaptureSnapshot(m_Scene);
    m_PendingHistoryIsUndo = undo;
    const std::string document = m_PendingHistorySnapshot->Document;
    m_StatusMessage = undo ? "Undo in progress..." : "Redo in progress...";
    try
    {
        m_HistoryRestoreFuture = m_CpuTaskPool.Submit("Parse scene snapshot for Undo/Redo", [document]()
        {
            return quark::CSceneDocument::Parse(document);
        });
    }
    catch (const std::exception& exception)
    {
        m_StatusMessage = "Undo/Redo failed to start background parsing";
        TraceLog(LogLevel::Error, "EDITOR", TextFormat("%s: %s",
            m_StatusMessage.c_str(), exception.what()));
        m_PendingHistorySnapshot.reset();
        m_PendingCurrentSnapshot.reset();
    }
}

void CEditor::PollHistoryRestore()
{
    if (!IsHistoryRestorePending())
    {
        return;
    }

    const auto clearPendingRestore = [this]()
    {
        m_PendingHistorySnapshot.reset();
        m_PendingCurrentSnapshot.reset();
        m_PendingParsedSceneDocument.reset();
        m_vPendingGeometryFutures.clear();
        m_vPendingGeometryResults.clear();
    };

    if (m_HistoryRestoreFuture.valid())
    {
        if (m_HistoryRestoreFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }

        try
        {
            m_PendingParsedSceneDocument = m_HistoryRestoreFuture.get();
        }
        catch (const std::exception& exception)
        {
            m_StatusMessage = "Undo/Redo failed while parsing the scene snapshot";
            TraceLog(LogLevel::Error, "EDITOR", TextFormat("%s: %s",
                m_StatusMessage.c_str(), exception.what()));
            clearPendingRestore();
            return;
        }

        if (!m_PendingParsedSceneDocument->IsValid)
        {
            m_StatusMessage = "Undo/Redo failed: the scene snapshot is invalid";
            TraceLog(LogLevel::Error, "EDITOR", m_StatusMessage.c_str());
            clearPendingRestore();
            return;
        }

        m_vPendingGeometryResults.resize(
            m_PendingParsedSceneDocument->Document["entities"].size());
        try
        {
            for (auto& meshSnapshot : m_PendingParsedSceneDocument->vEditableMeshes)
            {
                const size_t entityIndex = meshSnapshot.EntityIndex;
                CEditableMesh editableMesh = std::move(meshSnapshot.Mesh);
                m_vPendingGeometryFutures.emplace_back(
                    entityIndex,
                    m_CpuTaskPool.Submit("Build editable mesh geometry", [editableMesh = std::move(editableMesh)]()
                    {
                        return BuildEditableMeshData(editableMesh);
                    }));
            }
        }
        catch (const std::exception& exception)
        {
            m_StatusMessage = "Undo/Redo failed to schedule geometry processing";
            TraceLog(LogLevel::Error, "EDITOR", TextFormat("%s: %s",
                m_StatusMessage.c_str(), exception.what()));
            clearPendingRestore();
            return;
        }
    }

    for (const auto& geometryFuture : m_vPendingGeometryFutures)
    {
        if (geometryFuture.second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }
    }

    try
    {
        for (auto& geometryFuture : m_vPendingGeometryFutures)
        {
            SEditableMeshBuildData buildData = geometryFuture.second.get();
            if (!buildData.IsValid || geometryFuture.first >= m_vPendingGeometryResults.size())
            {
                m_StatusMessage = "Undo/Redo failed: editable mesh geometry is invalid";
                TraceLog(LogLevel::Error, "EDITOR", m_StatusMessage.c_str());
                clearPendingRestore();
                return;
            }
            m_vPendingGeometryResults[geometryFuture.first] = std::move(buildData);
        }
    }
    catch (const std::exception& exception)
    {
        m_StatusMessage = "Undo/Redo failed while processing editable mesh geometry";
        TraceLog(LogLevel::Error, "EDITOR", TextFormat("%s: %s",
            m_StatusMessage.c_str(), exception.what()));
        clearPendingRestore();
        return;
    }

    std::stack<quark::SSceneSnapshot>& source = m_PendingHistoryIsUndo ? m_UndoStack : m_RedoStack;
    std::stack<quark::SSceneSnapshot>& destination = m_PendingHistoryIsUndo ? m_RedoStack : m_UndoStack;
    if (source.empty())
    {
        m_StatusMessage = "Undo/Redo cancelled: history changed while restoring";
        clearPendingRestore();
        return;
    }

    if (!RestoreSnapshot(m_Scene, *m_PendingParsedSceneDocument, *m_PendingHistorySnapshot,
        m_Assets, m_Lights, m_ComponentFactories, m_vPendingGeometryResults))
    {
        m_StatusMessage = "Undo/Redo failed: could not restore the scene snapshot";
        TraceLog(LogLevel::Error, "EDITOR", m_StatusMessage.c_str());
        clearPendingRestore();
        return;
    }

    source.pop();
    PushHistory(destination, *m_PendingCurrentSnapshot, m_Preferences.m_UndoHistoryLimit);
    m_SceneDirty = true;
    m_StatusMessage.clear();
    clearPendingRestore();
}

void CEditor::Undo()
{
    StartHistoryRestore(true);
}

void CEditor::Redo()
{
    StartHistoryRestore(false);
}

void CEditor::HandleInput()
{
    if (IsHistoryRestorePending())
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const bool keyboardAvailable = !io.WantTextInput;

    if (keyboardAvailable && IsKeyPressed(KeyboardKey::P))
    {
        m_Ui.m_MeshEdit.GizmoMode = ImGuizmo::TRANSLATE;
    }
    if (keyboardAvailable && IsKeyPressed(KeyboardKey::R))
    {
        m_Ui.m_MeshEdit.GizmoMode = ImGuizmo::ROTATE;
    }
    if (keyboardAvailable && IsKeyPressed(KeyboardKey::S))
    {
        m_Ui.m_MeshEdit.GizmoMode = ImGuizmo::SCALE;
    }

    if (IsFileDropped())
    {
        FilePathList dropped = LoadDroppedFiles();
        bool importedAny = false;

        std::error_code ec;

        if (m_CurrentAssetPath.empty())
        {
            if (!m_ProjectPath.empty())
            {
                m_CurrentAssetPath = fs::path(m_ProjectPath) / "resources";
            }
            else
            {
                m_CurrentAssetPath = fs::current_path() / "projects" / "default" / "resources";
            }
            fs::create_directories(m_CurrentAssetPath, ec);
        }

        for (unsigned int i = 0; i < dropped.count; i++)
        {
            importedAny = ImportPathToResources(fs::path(dropped.paths[i]), m_CurrentAssetPath) || importedAny;
        }

        UnloadDroppedFiles(dropped);

        if (importedAny)
        {
            SaveState();
            m_Assets.RequestRefresh(m_ProjectPath);
        }
    }

    const bool ctrl = (IsKeyDown(KeyboardKey::LeftControl) || IsKeyDown(KeyboardKey::RightControl)) && keyboardAvailable;
    const bool shiftDown = IsKeyDown(KeyboardKey::LeftShift) || IsKeyDown(KeyboardKey::RightShift);

    if (ctrl && shiftDown && IsKeyPressed(KeyboardKey::S))
    {
        CSceneFileService::SaveAs(*this);
    }
    else if (ctrl && IsKeyPressed(KeyboardKey::S))
    {
        CProjectService::Save(m_ProjectPath, m_Scene);
        m_SceneDirty = false;
    }

    const float now = static_cast<float>(GetTime());

    if (ctrl && IsKeyDown(KeyboardKey::Z))
    {
        if (m_Ui.m_MeshEdit.Undo.ShouldFire(now))
        {
            Undo();
        }
    }
    else
    {
        m_Ui.m_MeshEdit.Undo.Release();
    }

    if (ctrl && IsKeyDown(KeyboardKey::Y))
    {
        if (m_Ui.m_MeshEdit.Redo.ShouldFire(now))
        {
            Redo();
        }
    }
    else
    {
        m_Ui.m_MeshEdit.Redo.Release();
    }

    CEntity* pEntity = m_Scene.GetSelected();

    if (ctrl && IsKeyDown(KeyboardKey::C))
    {
        if (m_Ui.m_MeshEdit.Copy.ShouldFire(now))
        {
            CEntityClipboard::Copy(*this, pEntity);
        }
    }
    else
    {
        m_Ui.m_MeshEdit.Copy.Release();
    }

    if (ctrl && IsKeyDown(KeyboardKey::V))
    {
        if (m_Ui.m_MeshEdit.Paste.ShouldFire(now))
        {
            CEntityClipboard::Paste(*this);
        }
    }
    else
    {
        m_Ui.m_MeshEdit.Paste.Release();
    }

    if (ctrl && IsKeyDown(KeyboardKey::D))
    {
        if (m_Ui.m_MeshEdit.Duplicate.ShouldFire(now))
        {
            CSceneEntityCommands::Duplicate(*this, pEntity);
        }
    }
    else
    {
        m_Ui.m_MeshEdit.Duplicate.Release();
    }

    if (keyboardAvailable && IsKeyPressed(KeyboardKey::Delete))
    {
        CSceneEntityCommands::Delete(*this, pEntity);
    }

    if (m_Assets.PollResources(m_ProjectPath, m_Scene, GetTime()))
    {
        m_Previews.InvalidateModelPreviews();
        m_Previews.InvalidateMaterialPreviews();
        m_Ui.m_AssetBrowser.dependencyCachePath.clear();
        m_Ui.m_AssetBrowser.vCachedFileDependencies.clear();
        m_Ui.m_AssetBrowser.dependencyCacheReady = false;

        if (m_Ui.m_ModelViewer.m_Visible && !m_Ui.m_ModelViewer.m_AssetName.empty())
        {
            const CModelAsset* pAsset = m_Assets.FindModelByName(m_Ui.m_ModelViewer.m_AssetName);
            if (!pAsset || !OpenModelViewerForAsset(m_Ui.m_ModelViewer, *pAsset))
            {
                m_Ui.m_ModelViewer.ReleasePreviewModel();
                m_Ui.m_ModelViewer.m_Visible = false;
                m_Ui.m_ModelViewer.m_AssetName.clear();
            }
        }

        if (m_Ui.m_MaterialViewer.m_Visible)
        {
            if (!OpenMaterialViewerForPath(*this, m_Ui.m_MaterialViewer, m_Ui.m_MaterialViewer.m_CurrentPath))
            {
                m_Ui.m_MaterialViewer.ReleasePreviewModel();
                m_Ui.m_MaterialViewer.m_Visible = false;
            }
        }
    }
}
