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

void RestoreSnapshot(CScene& scene, const quark::SSceneSnapshot& snapshot,
                     CAssetLibrary& assets, CLightRegistry& lights,
                     const CComponentFactoryRegistry& factories)
{
    scene.ReleaseResources();
    if (!quark::CSceneDocument::Deserialize(snapshot.Document, scene, factories))
    {
        return;
    }

    scene.m_Selected = snapshot.Selected;
    scene.m_vSelectedEntities = snapshot.vSelectedEntities;
    quark::CSceneRuntime::RestoreSceneEntityModels(scene, assets);
    quark::CSceneRuntime::ResetSceneLightRuntime(scene, lights);
}

} // anonymous

CEditor::CEditor()
{
    m_Assets.SetTextMesh(m_Text);
}

CEditor::~CEditor() = default;

void CEditor::Unload()
{
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
    m_SceneDirty = true;
    PushHistory(m_UndoStack, quark::CSceneDocument::CaptureSnapshot(m_Scene), m_Preferences.m_UndoHistoryLimit);
    while (!m_RedoStack.empty())
    {
        m_RedoStack.pop();
    }
}

void CEditor::Undo()
{
    if (m_UndoStack.empty())
    {
        return;
    }

    const quark::SSceneSnapshot previous = m_UndoStack.top();
    m_UndoStack.pop();

    PushHistory(m_RedoStack, quark::CSceneDocument::CaptureSnapshot(m_Scene), m_Preferences.m_UndoHistoryLimit);
    RestoreSnapshot(m_Scene, previous, m_Assets, m_Lights, m_ComponentFactories);
}

void CEditor::Redo()
{
    if (m_RedoStack.empty())
    {
        return;
    }

    const quark::SSceneSnapshot next = m_RedoStack.top();
    m_RedoStack.pop();

    PushHistory(m_UndoStack, quark::CSceneDocument::CaptureSnapshot(m_Scene), m_Preferences.m_UndoHistoryLimit);
    RestoreSnapshot(m_Scene, next, m_Assets, m_Lights, m_ComponentFactories);
}

void CEditor::HandleInput()
{
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
            m_Assets.Refresh(m_ProjectPath, m_Scene);
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

    m_Assets.PollResources(m_ProjectPath, m_Scene, GetTime());
}
