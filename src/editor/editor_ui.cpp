#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#define Rectangle WinRectangle
#include <windows.h>
#include <commdlg.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef near
#undef far
#endif

#include "editor/editor.h"

#include "application_plugin_bridge.h"

#include "camera.h"
#include "editor/editor_assets.h"
#include "editor/editor_components_ui.h"
#include "editor/editor_desktop.h"
#include "editor/editor_entity.h"
#include "editor/editor_entity_clipboard.h"
#include "editor/editor_entity_commands.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_hierarchy_utils.h"
#include "editor/editor_language_catalog.h"
#include "editor/editor_layout.h"
#include "editor/editor_polygon_edit.h"
#include "editor/editor_preferences.h"
#include "editor/editor_scene_drop.h"
#include "editor/editor_scene_file.h"
#include "editor/editor_state.h"
#include "editor/editor_status_bar.h"
#include "editor/editor_theme.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "language_manager.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "version.h"

#include "ImGuizmo.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "qcImGui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#define lang CLanguageManager::Get()
void CViewportState::Release()
{
    if (m_RenderTexture.id != 0)
    {
        UnloadRenderTexture(m_RenderTexture);
        m_RenderTexture = { 0 };
    }
}

void CViewportState::Unload()
{
    Release();
}

void CEditor::DrawUi(Shader shader, CFlyCamera& camera, SPluginContext* pPluginCtx)
{
    const auto drawHistoryRestoreOverlay = [this]()
    {
        ImGuiViewport* pViewport = ImGui::GetMainViewport();
        if (!pViewport)
        {
            return;
        }
        ImGui::SetNextWindowPos(
            ImVec2(pViewport->GetCenter().x, pViewport->GetCenter().y),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.9f);
        if (ImGui::Begin("Restoring scene", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs))
        {
            ImGui::TextUnformatted(m_StatusMessage.c_str());
        }
        ImGui::End();
    };

    ImVec4& selectionStyle = ImGui::GetStyle().Colors[ImGuiCol_HeaderActive];
    selectionStyle = ImVec4(
        m_Preferences.m_SelectionRed / 255.0f,
        m_Preferences.m_SelectionGreen / 255.0f,
        m_Preferences.m_SelectionBlue / 255.0f,
        1.0f
    );

    ImGuizmo::BeginFrame();

    const ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

    ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), m_Ui.m_Layout.DockspaceFlags);

    CEditorLayout::EnsureInitialized(*this, dockspaceId);

    ImGui::BeginDisabled(IsHistoryRestorePending());
    DrawMainMenuBar(pPluginCtx, dockspaceId);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(IsHistoryRestorePending());
    CStatusBar::Draw(*this);

    if (m_Preferences.m_ShowHierarchy)
    {
        DrawHierarchyPanel(pPluginCtx);
    }
    if (m_Preferences.m_ShowInspector)
    {
        DrawInspectorPanel(shader, pPluginCtx);
    }
    if (m_Preferences.m_ShowScene)
    {
        DrawScenePanel(camera, pPluginCtx);
    }

    DrawRenameModal();

    if (m_Preferences.m_ShowAssets)
    {
        DrawAssetsUi();
    }
    DrawModelViewerWindow(m_Ui.m_ModelViewer);
    DrawMaterialViewerWindow(*this, m_Ui.m_MaterialViewer, m_Scene.GetSelected());

    DrawAboutModal();
    DrawPreferencesUi(camera, pPluginCtx);
    DrawConfirmationModals();
    ImGui::EndDisabled();

    if (IsHistoryRestorePending())
    {
        drawHistoryRestoreOverlay();
    }
}

void CEditor::DrawMainMenuBar(SPluginContext* pPluginCtx, ImGuiID dockspaceId)
{
    SModalState& modal = m_Ui.m_Modal;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu(lang.Word("file")))
        {
            m_pPluginManager->DrawUiRegion(UI_MENU_FILE, *pPluginCtx);

            if (ImGui::MenuItem(lang.Word("save"), "Ctrl+S"))
            {
                CProjectService::Save(m_ProjectPath, m_Scene);
                DispatchPluginEvent(PLUGIN_EVENT_SCENE_SAVED);
                m_SceneDirty = false;
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
            {
                CSceneFileService::SaveAs(*this);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(lang.Word("exit")))
            {
                if (m_Preferences.m_ConfirmExit && m_SceneDirty)
                {
                    modal.ShowExitConfirmation = true;
                }
                else
                {
                    CloseWindow();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.Word("edit")))
        {
            m_pPluginManager->DrawUiRegion(UI_MENU_EDIT, *pPluginCtx);

            if (ImGui::MenuItem(lang.Word("undo"), "Ctrl+Z"))
            {
                Undo();
            }
            if (ImGui::MenuItem(lang.Word("redo"), "Ctrl+Y"))
            {
                Redo();
            }

            ImGui::Separator();

            CEntity* pEntity = m_Scene.GetSelected();
            if (ImGui::MenuItem(lang.Word("copy"), "Ctrl+C", false, pEntity != nullptr))
            {
                CEntityClipboard::Copy(*this, pEntity);
            }

            if (ImGui::MenuItem(lang.Word("paste"), "Ctrl+V", false, m_Ui.m_MeshEdit.HasClipboard))
            {
                CEntityClipboard::Paste(*this);
            }

            if (ImGui::MenuItem(lang.Word("dublicate"), "Ctrl+D", false, pEntity != nullptr))
            {
                CSceneEntityCommands::Duplicate(*this, pEntity);
            }

            ImGui::Separator();

            if (ImGui::MenuItem(lang.Word("delete"), "Del", false, pEntity != nullptr))
            {
                CSceneEntityCommands::Delete(*this, pEntity);
            }

            ImGui::Separator();
            if (ImGui::MenuItem(lang.Word("preferences")))
            {
                modal.ShowPreferences = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.Word("layout")))
        {
            if (ImGui::MenuItem(lang.Word("reset_layout")))
            {
                CEditorLayout::Reset(dockspaceId, m_Preferences);
            }
            ImGui::Separator();
            ImGui::MenuItem(lang.Word("hierarchy"), nullptr, &m_Preferences.m_ShowHierarchy);
            ImGui::MenuItem(lang.Word("inspector"), nullptr, &m_Preferences.m_ShowInspector);
            ImGui::MenuItem(lang.Word("assets"), nullptr, &m_Preferences.m_ShowAssets);
            ImGui::MenuItem(lang.Word("scene"), nullptr, &m_Preferences.m_ShowScene);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.Word("create")))
        {
            for (auto& asset : m_Assets.Models())
            {
                if (!asset.m_IsProcedural)
                {
                    continue;
                }
                if (ImGui::MenuItem(asset.m_Name.c_str()))
                {
                    SaveState();
                    CEntity created = CEntityFactory::FromAsset(m_Scene, asset);
                    const CMeshComponent* pCreatedMesh = created.GetMeshComponent();
                    if (pCreatedMesh && HasValidModelData(pCreatedMesh->m_Model))
                    {
                        m_Scene.m_vEntities.push_back(std::move(created));
                        const int entityIndex = static_cast<int>(m_Scene.m_vEntities.size()) - 1;
                        DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
                        m_Scene.SelectEntity(entityIndex, false);
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(lang.Word("light")))
            {
                SaveState();
                CEntity created = CEntityFactory::Light(m_Scene, -1);
                m_Scene.m_vEntities.push_back(std::move(created));
                const int entityIndex = static_cast<int>(m_Scene.m_vEntities.size()) - 1;
                DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
                m_Scene.SelectEntity(entityIndex, false);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.Word("help")))
        {
            m_pPluginManager->DrawUiRegion(UI_MENU_HELP, *pPluginCtx);

            if (ImGui::MenuItem(lang.Word("about")))
            {
                m_Ui.m_Layout.ShowAboutWindow = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void CEditor::DrawHierarchyPanel(SPluginContext* pPluginCtx)
{
    SModalState& modal = m_Ui.m_Modal;

    ImGui::Begin(lang.Word("hierarchy"), &m_Preferences.m_ShowHierarchy);
    m_pPluginManager->DrawUiRegion(UI_HIERARCHY, *pPluginCtx);

    if (m_Scene.m_vSelectedEntities.empty() && m_Scene.m_Selected >= 0)
    {
        m_Scene.SelectEntity(m_Scene.m_Selected, false);
    }

    HierarchyDrawEntityTree(-1);

    const ImVec2 hierarchySpace = ImGui::GetContentRegionAvail();
    if (hierarchySpace.x > 1.0f && hierarchySpace.y > 1.0f)
    {
        const ImVec2 dropMin = ImGui::GetCursorScreenPos();
        const ImRect dropZone(dropMin, ImVec2(dropMin.x + hierarchySpace.x, dropMin.y + hierarchySpace.y));
        if (ImGui::BeginDragDropTargetCustom(dropZone, ImGui::GetID("HierarchyRootDropZone")))
        {
            if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
            {
                if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
                {
                    const int droppedIndex = *static_cast<const int*>(pPayload->Data);
                    if (droppedIndex >= 0 && droppedIndex < static_cast<int>(m_Scene.m_vEntities.size()) &&
                        m_Scene.m_vEntities[droppedIndex].m_ParentId != -1)
                    {
                        SaveState();
                        MoveEntityToParent(m_Scene, droppedIndex, -1);
                    }
                }
            }

            if (ImGui::IsDragDropPayloadBeingAccepted())
            {
                ImGui::GetWindowDrawList()->AddRect(
                    dropZone.Min,
                    dropZone.Max,
                    IM_COL32(80, 180, 255, 255),
                    2.0f,
                    0,
                    2.0f
                );
            }
            ImGui::EndDragDropTarget();
        }
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems))
    {
        HierarchyDrawCreateMenu(-1);

        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
        {
            int droppedIndex = *(const int*)pPayload->Data;
            SaveState();
            MoveEntityToParent(m_Scene, droppedIndex, -1);
        }
        ImGui::EndDragDropTarget();
    }

    if (modal.DeferredHierarchyDelete.pEditor == this && modal.DeferredHierarchyDelete.Index >= 0)
    {
        const int index = modal.DeferredHierarchyDelete.Index;
        modal.DeferredHierarchyDelete.pEditor = nullptr;
        modal.DeferredHierarchyDelete.Index = -1;
        CSceneEntityCommands::Erase(*this, index);
    }
    ImGui::End();
}

void CEditor::HierarchyAcceptEntityDrop(int targetIndex)
{
    if (!ImGui::BeginDragDropTarget())
    {
        return;
    }

    if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("ENTITY_INDEX"))
    {
        if (pPayload->IsDelivery() && pPayload->DataSize == sizeof(int))
        {
            const int droppedIndex = *static_cast<const int*>(pPayload->Data);
            if (droppedIndex != targetIndex)
            {
                SaveState();
                MoveEntityToParent(m_Scene, droppedIndex, targetIndex);
            }
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
            2.0f
        );
    }

    ImGui::EndDragDropTarget();
}

void CEditor::HierarchyDrawCreateMenu(int parentIndex)
{
    if (ImGui::BeginMenu(lang.Word("create")))
    {
        for (int assetIndex = 0; assetIndex < static_cast<int>(m_Assets.ModelCount()); assetIndex++)
        {
            auto& asset = *m_Assets.ModelByIndex(assetIndex);
            const std::string label = asset.m_Name + "##create_" + std::to_string(assetIndex);
            if (ImGui::MenuItem(label.c_str()))
            {
                SaveState();
                CEntity entity = CEntityFactory::FromAsset(m_Scene, asset);
                const CMeshComponent* pMesh = entity.GetMeshComponent();
                if (!pMesh || !HasValidModelData(pMesh->m_Model))
                {
                    continue;
                }
                entity.m_ParentId = parentIndex;
                m_Scene.m_vEntities.push_back(std::move(entity));
                const int entityIndex = static_cast<int>(m_Scene.m_vEntities.size()) - 1;
                DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
                m_Scene.SelectEntity(entityIndex, false);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(lang.Word("light")))
        {
            SaveState();
            CEntity entity = CEntityFactory::Light(m_Scene, parentIndex);
            m_Scene.m_vEntities.push_back(std::move(entity));
            const int entityIndex = static_cast<int>(m_Scene.m_vEntities.size()) - 1;
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED, entityIndex);
            m_Scene.SelectEntity(entityIndex, false);
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem(lang.Word("create_group")))
    {
        SaveState();
        CreateGroup(m_Scene, "Group", parentIndex);
    }
}

void CEditor::HierarchyDrawEntityItem(int entityIndex)
{
    CEntity& entity = m_Scene.m_vEntities[entityIndex];
    const bool selected = m_Scene.IsSelected(entityIndex);

    ImGui::PushID(entityIndex);

    if (ImGui::Selectable(entity.m_Name.c_str(), selected))
    {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        m_Scene.SelectEntity(entityIndex, ctrl);
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        ImGui::SetDragDropPayload("ENTITY_INDEX", &entityIndex, sizeof(int));
        ImGui::Text("%s", entity.m_Name.c_str());
        ImGui::EndDragDropSource();
    }

    HierarchyAcceptEntityDrop(entityIndex);

    if (ImGui::BeginPopupContextItem(TextFormat("context_%d", entity.m_Id)))
    {
        if (ImGui::MenuItem(lang.Word("delete")))
        {
            SaveState();

            m_Ui.m_Modal.DeferredHierarchyDelete.pEditor = this;
            m_Ui.m_Modal.DeferredHierarchyDelete.Index = entityIndex;

            ImGui::EndPopup();
            ImGui::PopID();
            return;
        }

        if (ImGui::MenuItem(lang.Word("rename")))
        {
            SaveState();
            m_Ui.m_MeshEdit.RenamingIndex = entityIndex;
            const size_t copied = entity.m_Name.copy(m_Ui.m_MeshEdit.aRenameBuffer, sizeof(m_Ui.m_MeshEdit.aRenameBuffer) - 1);
            m_Ui.m_MeshEdit.aRenameBuffer[copied] = '\0';
        }

        if (ImGui::MenuItem(lang.Word("dublicate")))
        {
            SaveState();
            CEntity copy = CSceneEntityCommands::CloneInstance(entity, m_Scene);
            m_Scene.m_vEntities.push_back(std::move(copy));
            DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED,
                static_cast<int>(m_Scene.m_vEntities.size()) - 1);
        }

        ImGui::Separator();
        HierarchyDrawCreateMenu(entityIndex);

        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void CEditor::HierarchyDrawEntityTree(int parentId)
{
    const auto vChildren = GetEntityChildren(m_Scene, parentId);
    for (int childIndex : vChildren)
    {
        CEntity& child = m_Scene.m_vEntities[childIndex];

        if (child.m_IsGroup)
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_Scene.IsSelected(childIndex))
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            const bool open = ImGui::TreeNodeEx(child.m_Name.c_str(), flags);

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("ENTITY_INDEX", &childIndex, sizeof(int));
                ImGui::Text("%s", child.m_Name.c_str());
                ImGui::EndDragDropSource();
            }

            HierarchyAcceptEntityDrop(childIndex);

            if (ImGui::BeginPopupContextItem(TextFormat("group_context_%d", child.m_Id)))
            {
                if (ImGui::MenuItem(lang.Word("delete")))
                {
                    SaveState();
                    m_Ui.m_Modal.DeferredHierarchyDelete.pEditor = this;
                    m_Ui.m_Modal.DeferredHierarchyDelete.Index = childIndex;
                    ImGui::EndPopup();
                    if (open)
                    {
                        ImGui::TreePop();
                    }
                    return;
                }

                if (ImGui::MenuItem(lang.Word("rename")))
                {
                    SaveState();
                    m_Ui.m_MeshEdit.RenamingIndex = childIndex;
                    const size_t copied = child.m_Name.copy(m_Ui.m_MeshEdit.aRenameBuffer, sizeof(m_Ui.m_MeshEdit.aRenameBuffer) - 1);
                    m_Ui.m_MeshEdit.aRenameBuffer[copied] = '\0';
                }

                if (ImGui::MenuItem(lang.Word("dublicate")))
                {
                    SaveState();
                    CEntity copy = CSceneEntityCommands::CloneInstance(child, m_Scene);
                    m_Scene.m_vEntities.push_back(std::move(copy));
                    DispatchPluginEvent(PLUGIN_EVENT_ENTITY_CREATED,
                        static_cast<int>(m_Scene.m_vEntities.size()) - 1);
                }

                ImGui::Separator();
                HierarchyDrawCreateMenu(childIndex);

                ImGui::EndPopup();
            }

            ImGui::PushID(childIndex);
            if (ImGui::IsItemClicked())
            {
                const bool ctrl = ImGui::GetIO().KeyCtrl;
                m_Scene.SelectEntity(childIndex, ctrl);
            }
            ImGui::PopID();

            if (open)
            {
                HierarchyDrawEntityTree(childIndex);
                ImGui::TreePop();
            }
        }
        else
        {
            HierarchyDrawEntityItem(childIndex);
            const auto vNestedChildren = GetEntityChildren(m_Scene, childIndex);
            if (!vNestedChildren.empty())
            {
                ImGui::Indent();
                HierarchyDrawEntityTree(childIndex);
                ImGui::Unindent();
            }
        }
    }
}

void CEditor::DrawInspectorPanel(Shader shader, SPluginContext* pPluginCtx)
{
    SInspectorUiState& inspector = m_Ui.m_Inspector;

    ImGui::Begin(lang.Word("inspector"), &m_Preferences.m_ShowInspector);
    m_pPluginManager->DrawUiRegion(UI_INSPECTOR, *pPluginCtx);

    ImGui::Text("%s", lang.Word("mode"));
    ImGui::SameLine();
    if (ImGui::Button("P"))
    {
        m_Ui.m_MeshEdit.GizmoMode = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("R"))
    {
        m_Ui.m_MeshEdit.GizmoMode = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("S"))
    {
        m_Ui.m_MeshEdit.GizmoMode = ImGuizmo::SCALE;
    }

    CEntity* pEntity = m_Scene.GetSelected();
    if (pEntity)
    {
        ImGui::Separator();
        ImGui::Spacing();

        char aInspectorName[128] = {};
        const size_t copied = pEntity->m_Name.copy(aInspectorName, sizeof(aInspectorName) - 1);
        aInspectorName[copied] = '\0';

        SEntityRenameState& rename = m_Ui.m_EntityRename;
        if (ImGui::InputText(lang.Word("name"), aInspectorName, IM_ARRAYSIZE(aInspectorName)))
        {
            if (rename.LastName != aInspectorName)
            {
                if (!rename.StateSaved || rename.EntityId != pEntity->m_Id)
                {
                    SaveState();
                    rename.StateSaved = true;
                    rename.EntityId = pEntity->m_Id;
                }
                CEntityFactory::AssignName(*pEntity, aInspectorName);
                rename.LastName = aInspectorName;
            }
        }
        else
        {
            rename.LastName = aInspectorName;
        }
        if (!ImGui::IsItemActive())
        {
            rename.StateSaved = false;
            rename.EntityId = -1;
        }

        ImGui::Spacing();
        const char* pCurrentTag = pEntity->m_vTags.empty() ? "Untagged" : pEntity->m_vTags.front().c_str();
        bool openAddTagPopup = false;
        if (ImGui::BeginCombo("Tag", pCurrentTag))
        {
            if (ImGui::Selectable("Untagged", pEntity->m_vTags.empty()))
            {
                if (!pEntity->m_vTags.empty())
                {
                    SaveState();
                    pEntity->m_vTags.clear();
                }
            }

            for (size_t tagIndex = 0; tagIndex < pEntity->m_vTags.size(); ++tagIndex)
            {
                ImGui::PushID(static_cast<int>(tagIndex));
                ImGui::Selectable(pEntity->m_vTags[tagIndex].c_str(), tagIndex == 0);
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                {
                    SaveState();
                    pEntity->m_vTags.erase(pEntity->m_vTags.begin() + static_cast<std::ptrdiff_t>(tagIndex));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::Separator();
            if (ImGui::Selectable("Add Tag..."))
            {
                openAddTagPopup = true;
            }
            ImGui::EndCombo();
        }

        if (openAddTagPopup)
        {
            ImGui::OpenPopup("AddTagPopup");
        }
        if (ImGui::BeginPopup("AddTagPopup"))
        {
            ImGui::TextUnformatted("Add Tag");
            ImGui::InputText("##tag_name", inspector.aTagBuffer, IM_ARRAYSIZE(inspector.aTagBuffer));
            if (ImGui::Button("Add") && inspector.aTagBuffer[0] != '\0')
            {
                const std::string newTag(inspector.aTagBuffer);
                if (std::find(pEntity->m_vTags.begin(), pEntity->m_vTags.end(), newTag) == pEntity->m_vTags.end())
                {
                    SaveState();
                    pEntity->m_vTags.push_back(newTag);
                }
                inspector.aTagBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                inspector.aTagBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        CComponentUIHelper::DrawEntityInspector(*this, *pEntity, shader);
    }
    else
    {
        DrawSelectedTextureInspector(*this);
    }

    ImGui::End();
}

void CEditor::DrawScenePanel(CFlyCamera& camera, SPluginContext* pPluginCtx)
{
    CViewportState& viewport = m_Ui.m_Viewport;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin(lang.Word("scene"), &m_Preferences.m_ShowScene))
    {
        m_pPluginManager->DrawUiRegion(UI_INSPECTOR, *pPluginCtx);
        viewport.m_WindowPos = ImGui::GetCursorScreenPos();
        viewport.m_WindowSize = ImGui::GetContentRegionAvail();

        if (viewport.m_WindowSize.x > 0 && viewport.m_WindowSize.y > 0)
        {
            if (viewport.m_RenderTexture.id == 0 ||
                viewport.m_RenderTexture.texture.width  != (int)viewport.m_WindowSize.x ||
                viewport.m_RenderTexture.texture.height != (int)viewport.m_WindowSize.y)
            {
                viewport.Release();
                viewport.m_RenderTexture = LoadRenderTexture((int)viewport.m_WindowSize.x, (int)viewport.m_WindowSize.y);
            }

            if (viewport.m_RenderTexture.id > 0)
            {
                const bool topLeftTextureBackend =
                    GetCurrentBackend() == RendererType::Vulkan ||
                    GetCurrentBackend() == RendererType::D3D11;
                QcImGuiAddImage(
                    ImGui::GetWindowDrawList(),
                    &viewport.m_RenderTexture.texture,
                    viewport.m_WindowPos,
                    ImVec2(viewport.m_WindowPos.x + viewport.m_WindowSize.x,
                        viewport.m_WindowPos.y + viewport.m_WindowSize.y),
                    topLeftTextureBackend ? ImVec2(0, 0) : ImVec2(0, 1),
                    topLeftTextureBackend ? ImVec2(1, 1) : ImVec2(1, 0)
                );
            }

            viewport.m_Hovered = ImGui::IsWindowHovered();
            viewport.m_Focused  = ImGui::IsWindowFocused();

            CGizmoController::Draw(*this, camera);
            CPolygonEditor::Draw(*this, camera.GetCamera());
            CSceneAssetDrop::Handle(*this, camera.GetCamera());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void CEditor::DrawRenameModal()
{
    if (m_Ui.m_MeshEdit.RenamingIndex != -1)
    {
        ImGui::OpenPopup(lang.Word("rename"));
    }

    if (ImGui::BeginPopupModal(lang.Word("rename"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("##rename", m_Ui.m_MeshEdit.aRenameBuffer, IM_ARRAYSIZE(m_Ui.m_MeshEdit.aRenameBuffer));
        if (ImGui::Button(lang.Word("ok")))
        {
            if (m_Ui.m_MeshEdit.RenamingIndex >= 0 && m_Ui.m_MeshEdit.RenamingIndex < static_cast<int>(m_Scene.m_vEntities.size()))
            {
                SaveState();
                CEntityFactory::AssignName(m_Scene.m_vEntities[m_Ui.m_MeshEdit.RenamingIndex], m_Ui.m_MeshEdit.aRenameBuffer);
            }
            m_Ui.m_MeshEdit.RenamingIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(lang.Word("cancel")))
        {
            m_Ui.m_MeshEdit.RenamingIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void CEditor::DrawAboutModal()
{
    if (m_Ui.m_Layout.ShowAboutWindow)
    {
        ImGui::OpenPopup(lang.Word("about_quark_engine"));
        m_Ui.m_Layout.ShowAboutWindow = false;
    }

    if (ImGui::BeginPopupModal(lang.Word("about_quark_engine"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        constexpr float kLogoWidth = 400.0f;
        constexpr float kLogoHeight = kLogoWidth * 648.0f / 1500.0f;

        const Texture2D* pLogo = m_Textures.Load("assets/quark_engine.png");
        if (pLogo != nullptr)
        {
            const float logoOffsetX = (ImGui::GetContentRegionAvail().x - kLogoWidth) * 0.5f;
            if (logoOffsetX > 0.0f)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + logoOffsetX);
            }
            QcImGuiImage(pLogo, ImVec2(kLogoWidth, kLogoHeight));
            ImGui::Spacing();
        }

        const auto backend = GetCurrentBackend();

        ImGui::Text("Quark Engine %s (Build %s | %s) using %s",
            QUARK_ENGINE_VERSION,
            QUARK_ENGINE_BUILD_NUMBER,
            QUARK_ENGINE_BUILD_DATE_STRING,
            backend == RendererType::Vulkan ? "Vulkan" :
            backend == RendererType::OpenGL ? "OpenGL" :
            backend == RendererType::Auto ? "Auto" :
            "Unknown"
        );
        ImGui::Separator();
        ImGui::Text(lang.Word("quarkcore_version"), QC_VERSION_STRING, "stable");
        ImGui::Text(lang.Word("imgui_version"), IMGUI_VERSION);
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "%s", lang.Word("website"));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", lang.Word("open_website"));
            if (ImGui::IsMouseClicked(0))
            {
                CDesktopIntegration::OpenUrl("https://quark-engine.github.io/");
            }
        }

        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "%s", lang.Word("discord_server"));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", lang.Word("join_discord_server"));
            if (ImGui::IsMouseClicked(0))
            {
                CDesktopIntegration::OpenUrl("https://discord.gg/ttzpFBhy9Y");
            }
        }

        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "%s", lang.Word("api_docs"));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", lang.Word("open_docs"));
            if (ImGui::IsMouseClicked(0))
            {
                CDesktopIntegration::OpenUrl("https://quark-engine.gitbook.io/quark-engine-docs");
            }
        }

        ImGui::Spacing();
        if (ImGui::Button(lang.Word("close"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void CEditor::DrawPreferencesUi(CFlyCamera& camera, SPluginContext* pPluginCtx)
{
    if (!m_Ui.m_Modal.ShowPreferences)
    {
        return;
    }

    ImGui::Begin(lang.Word("preferences"), &m_Ui.m_Modal.ShowPreferences);
    ImGui::Text("%s", lang.Word("preferences"));
    ImGui::Separator();

    bool preferencesChanged = false;
    if (ImGui::BeginTabBar("PreferencesTabs"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            preferencesChanged |= DrawPreferencesGeneralTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering"))
        {
            preferencesChanged |= DrawPreferencesRenderingTab(camera);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Interface"))
        {
            preferencesChanged |= DrawPreferencesInterfaceTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Plugins"))
        {
            preferencesChanged |= DrawPreferencesPluginsTab(pPluginCtx);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (preferencesChanged)
    {
        m_Preferences.Save();
    }

    ImGui::End();
}

bool CEditor::DrawPreferencesGeneralTab()
{
    SPreferencesUiState& prefs = m_Ui.m_Preferences;

    if (prefs.LanguageIndex == -1)
    {
        prefs.LanguageIndex = CLanguageCatalog::IndexOf(CLanguageManager::Get().m_Current.c_str());
    }

    bool changed = false;

    ImGui::Text("%s", lang.Word("language"));
    if (ImGui::Combo("##language_combo", &prefs.LanguageIndex, CLanguageCatalog::GetLabel, nullptr, CLanguageCatalog::Count()))
    {
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
        lang.SetLang(CLanguageCatalog::Code(prefs.LanguageIndex));
        ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);
        changed = true;
    }

    changed |= ImGui::Checkbox("Wireframe", &m_Preferences.m_WireframeEnabled);
    changed |= ImGui::Checkbox("Show collision shapes", &m_Preferences.m_ShowColliders);
    changed |= ImGui::Checkbox("Confirm delete", &m_Preferences.m_ConfirmDelete);
    changed |= ImGui::Checkbox("Show bounding boxes", &m_Preferences.m_ShowBoundingBoxes);
    changed |= ImGui::Checkbox("Focus camera on selection", &m_Preferences.m_FocusOnSelection);
    changed |= ImGui::Checkbox("Confirm exit with unsaved changes", &m_Preferences.m_ConfirmExit);
    changed |= ImGui::Checkbox("Open last project", &m_Preferences.m_OpenLastProject);

    return changed;
}

bool CEditor::DrawPreferencesPluginsTab(SPluginContext* pPluginCtx)
{
    bool changed = false;

    ImGui::TextUnformatted("Loaded plugins");

    if (!m_pPluginManager)
    {
        ImGui::TextUnformatted("Plugin manager is not available.");
        return false;
    }

    const std::vector<SLoadedPlugin>& vPlugins = m_pPluginManager->GetPlugins();
    const auto normalizePluginPath = [](const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    };

    const auto findDisabledPlugin = [&normalizePluginPath, this](const std::string& pluginPath)
    {
        const std::string normalizedPath = normalizePluginPath(pluginPath);
        return std::find_if(
            m_Preferences.m_vDisabledPlugins.begin(),
            m_Preferences.m_vDisabledPlugins.end(),
            [&normalizePluginPath, &normalizedPath](const std::string& disabledPath)
            {
                return normalizePluginPath(disabledPath) == normalizedPath;
            });
    };

    const auto findLoadedPlugin = [&vPlugins, &normalizePluginPath](const std::string& pluginPath)
    {
        const auto iterator = std::find_if(
            vPlugins.begin(),
            vPlugins.end(),
            [&normalizePluginPath, &pluginPath](const SLoadedPlugin& plugin)
            {
                return normalizePluginPath(plugin.FilePath) == normalizePluginPath(pluginPath);
            });

        if (iterator == vPlugins.end())
        {
            return static_cast<const SLoadedPlugin*>(nullptr);
        }

        return &(*iterator);
    };

    std::vector<std::pair<std::string, bool>> vRows;
    const auto addPluginRow = [&vRows, &normalizePluginPath](const std::string& pluginPath, bool enabled)
    {
        const std::string normalizedPath = normalizePluginPath(pluginPath);
        const auto iterator = std::find_if(
            vRows.begin(),
            vRows.end(),
            [&normalizePluginPath, &normalizedPath](const std::pair<std::string, bool>& row)
            {
                return normalizePluginPath(row.first) == normalizedPath;
            });

        if (iterator != vRows.end())
        {
            iterator->second = enabled;
            return;
        }

        vRows.emplace_back(normalizedPath, enabled);
    };

    for (const SLoadedPlugin& plugin : vPlugins)
    {
        const std::string normalizedPath = normalizePluginPath(plugin.FilePath);
        addPluginRow(normalizedPath, findDisabledPlugin(normalizedPath) == m_Preferences.m_vDisabledPlugins.end());
    }

    for (const std::string& path : m_Preferences.m_vDisabledPlugins)
    {
        const std::string normalizedPath = normalizePluginPath(path);
        if (findLoadedPlugin(normalizedPath) == nullptr)
        {
            addPluginRow(normalizedPath, false);
        }
    }

    std::sort(
        vRows.begin(),
        vRows.end(),
        [](const std::pair<std::string, bool>& lhs, const std::pair<std::string, bool>& rhs)
        {
            return lhs.first < rhs.first;
        });

    std::vector<std::pair<std::string, bool>> vPendingToggles;

    if (vRows.empty())
    {
        ImGui::TextDisabled("No plugins are currently loaded.");
    }
    else if (ImGui::BeginTable("PluginsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (std::pair<std::string, bool>& row : vRows)
        {
            const std::string& pluginPath = row.first;
            const SLoadedPlugin* pLoadedPlugin = findLoadedPlugin(pluginPath);
            bool enabled = row.second;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(pluginPath.c_str());
            if (ImGui::Checkbox("", &enabled))
            {
                vPendingToggles.emplace_back(pluginPath, enabled);
                changed = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::BeginGroup();
            if (pLoadedPlugin != nullptr && pLoadedPlugin->pPlugin != nullptr)
            {
                const char* pPluginName = pLoadedPlugin->pPlugin->pName;
                const char* pPluginVersion = pLoadedPlugin->pPlugin->pVersion;
                if (pPluginName != nullptr)
                {
                    ImGui::TextUnformatted(pPluginName);
                }
                else
                {
                    const std::string fileName = std::filesystem::path(pluginPath).filename().string();
                    ImGui::TextUnformatted(fileName.c_str());
                }
                if (pPluginVersion != nullptr && pPluginVersion[0] != '\0')
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("v%s", pPluginVersion);
                }
            }
            else
            {
                const std::string fileName = std::filesystem::path(pluginPath).filename().string();
                ImGui::TextUnformatted(fileName.c_str());
            }
            ImGui::TextDisabled("%s", pluginPath.c_str());
            ImGui::EndGroup();
        }

        ImGui::EndTable();
    }

    for (const std::pair<std::string, bool>& pendingToggle : vPendingToggles)
    {
        const std::string& pluginPath = pendingToggle.first;
        const bool enabled = pendingToggle.second;
        const auto loadedPluginIterator = std::find_if(
            vPlugins.begin(),
            vPlugins.end(),
            [&normalizePluginPath, &pluginPath](const SLoadedPlugin& plugin)
            {
                return normalizePluginPath(plugin.FilePath) == normalizePluginPath(pluginPath);
            });
        const bool isLoaded = loadedPluginIterator != vPlugins.end();

        if (enabled)
        {
            auto iterator = findDisabledPlugin(pluginPath);
            if (iterator != m_Preferences.m_vDisabledPlugins.end())
            {
                m_Preferences.m_vDisabledPlugins.erase(iterator);
            }

            if (!isLoaded)
            {
                m_pPluginManager->LoadOne(pluginPath, pPluginCtx);
            }
        }
        else
        {
            if (findDisabledPlugin(pluginPath) == m_Preferences.m_vDisabledPlugins.end())
            {
                m_Preferences.m_vDisabledPlugins.push_back(pluginPath);
            }

            if (isLoaded)
            {
                const int pluginIndex = static_cast<int>(std::distance(vPlugins.begin(), loadedPluginIterator));
                m_pPluginManager->UnloadPlugin(pluginIndex);
            }
        }
    }

    return changed;
}

bool CEditor::DrawPreferencesRenderingTab(CFlyCamera& camera)
{
    bool changed = false;
    changed |= ImGui::Checkbox("Show scene grid", &m_Preferences.m_ShowGrid);
    changed |= ImGui::Checkbox("Show coordinate axes", &m_Preferences.m_ShowAxes);
    changed |= ImGui::Checkbox("Limit frame rate", &m_Preferences.m_LimitFps);
    ImGui::BeginDisabled(!m_Preferences.m_LimitFps);
    changed |= ImGui::SliderInt("Target FPS", &m_Preferences.m_TargetFps, 30, 240);
    ImGui::EndDisabled();
    changed |= ImGui::SliderFloat("Camera speed", &m_Preferences.m_CameraSpeed, 0.1f, 20.0f, "%.1f");
    changed |= ImGui::SliderFloat("Camera sensitivity", &m_Preferences.m_CameraSensitivity, 0.0005f, 0.02f, "%.4f");
    changed |= ImGui::SliderFloat("Zoom sensitivity", &m_Preferences.m_CameraZoomSensitivity, 0.1f, 5.0f, "%.1f");
    changed |= ImGui::SliderFloat("Camera FOV", &m_Preferences.m_CameraFov, 20.0f, 120.0f, "%.0f deg");
    changed |= ImGui::SliderFloat("Shadow bias", &m_Preferences.m_ShadowBias, 0.0001f, 0.05f, "%.4f");

    const char* apShadowFilterNames[] = { "Hard", "9 samples", "25 samples" };
    changed |= ImGui::Combo("Shadow filtering", &m_Preferences.m_ShadowFilterQuality, apShadowFilterNames, 3);
    changed |= ImGui::SliderInt("Undo history limit", &m_Preferences.m_UndoHistoryLimit, 10, 500);
    camera.m_Cam.fovy = m_Preferences.m_CameraFov;

    const char* apBackendNames[] = { "Auto", "OpenGL", "Vulkan", "Direct3D 11" };
    ImGui::Text("Renderer backend (restart required)");
    changed |= ImGui::Combo("##renderer_backend", &m_Preferences.m_RendererBackend, apBackendNames, 4);

    const char* apMsaaNames[] = { "Off", "2x", "4x", "8x" };
    int msaaIndex = m_Preferences.m_MsaaSamples == 2 ? 1 : m_Preferences.m_MsaaSamples == 4 ? 2 : m_Preferences.m_MsaaSamples == 8 ? 3 : 0;
    ImGui::Text("MSAA (restart required)");
    if (ImGui::Combo("##msaa", &msaaIndex, apMsaaNames, 4))
    {
        m_Preferences.m_MsaaSamples = msaaIndex == 1 ? 2 : msaaIndex == 2 ? 4 : msaaIndex == 3 ? 8 : 1;
        changed = true;
    }

    const char* apFilterNames[] = { "Nearest", "Linear" };
    ImGui::Text("Texture filtering (restart required)");
    changed |= ImGui::Combo("##texture_filter", &m_Preferences.m_TextureFilter, apFilterNames, 2);

    float aBackgroundColor[3] = {
        m_Preferences.m_BackgroundRed / 255.0f,
        m_Preferences.m_BackgroundGreen / 255.0f,
        m_Preferences.m_BackgroundBlue / 255.0f
    };
    if (ImGui::ColorEdit3("Scene background", aBackgroundColor))
    {
        m_Preferences.m_BackgroundRed = static_cast<int>(std::round(aBackgroundColor[0] * 255.0f));
        m_Preferences.m_BackgroundGreen = static_cast<int>(std::round(aBackgroundColor[1] * 255.0f));
        m_Preferences.m_BackgroundBlue = static_cast<int>(std::round(aBackgroundColor[2] * 255.0f));
        changed = true;
    }

    changed |= ImGui::Checkbox("Enable autosave", &m_Preferences.m_AutosaveEnabled);
    changed |= ImGui::Checkbox("Create autosave backup", &m_Preferences.m_AutosaveBackupEnabled);
    ImGui::BeginDisabled(!m_Preferences.m_AutosaveEnabled);
    changed |= ImGui::SliderInt("Autosave interval (minutes)", &m_Preferences.m_AutosaveIntervalMinutes, 1, 60);
    ImGui::EndDisabled();
    ImGui::Separator();

    changed |= ImGui::Checkbox("Enable Gizmo snapping", &m_Preferences.m_GizmoSnapEnabled);
    ImGui::BeginDisabled(!m_Preferences.m_GizmoSnapEnabled);
    changed |= ImGui::SliderFloat("Translation snap", &m_Preferences.m_GizmoTranslationSnap, 0.01f, 10.0f, "%.2f");
    changed |= ImGui::SliderFloat("Rotation snap", &m_Preferences.m_GizmoRotationSnap, 1.0f, 90.0f, "%.1f deg");
    changed |= ImGui::SliderFloat("Scale snap", &m_Preferences.m_GizmoScaleSnap, 0.01f, 1.0f, "%.2f");
    ImGui::EndDisabled();

    changed |= ImGui::Checkbox("Enable shadows", &m_Preferences.m_ShadowsEnabled);
    const char* apShadowSizes[] = { "512", "1024", "2048" };
    int shadowSizeIndex = m_Preferences.m_ShadowMapSize == 512 ? 0 :
        m_Preferences.m_ShadowMapSize == 2048 ? 2 : 1;
    if (ImGui::Combo("Shadow map size (restart required)", &shadowSizeIndex, apShadowSizes, 3))
    {
        m_Preferences.m_ShadowMapSize = shadowSizeIndex == 0 ? 512 :
            shadowSizeIndex == 2 ? 2048 : 1024;
        changed = true;
    }

    if (changed)
    {
        SetTargetFPS(m_Preferences.m_LimitFps ? m_Preferences.m_TargetFps : 0);
    }
    return changed;
}

bool CEditor::DrawPreferencesInterfaceTab()
{
    SInterfaceStyleState& iface = m_Ui.m_InterfaceStyle;
    if (!iface.Initialized)
    {
        iface.BaseStyle = ImGui::GetStyle();
        iface.BaseScale = m_Preferences.m_InterfaceScale;
        iface.Initialized = true;
    }

    bool changed = false;
    changed |= ImGui::Checkbox("Hierarchy", &m_Preferences.m_ShowHierarchy);
    changed |= ImGui::Checkbox("Inspector", &m_Preferences.m_ShowInspector);
    changed |= ImGui::Checkbox("Assets", &m_Preferences.m_ShowAssets);
    changed |= ImGui::Checkbox("Scene", &m_Preferences.m_ShowScene);
    changed |= ImGui::Checkbox("VSync (restart required)", &m_Preferences.m_VsyncEnabled);

    static const std::vector<SEditorTheme> s_vThemes = CThemeManager::GetAvailableThemes();
    const auto selectedTheme = std::find_if(s_vThemes.begin(), s_vThemes.end(),
        [this](const SEditorTheme& theme) { return theme.id == m_Preferences.m_ThemeName; });
    const std::string preview = selectedTheme != s_vThemes.end()
        ? selectedTheme->name : "Missing theme: " + m_Preferences.m_ThemeName;
    if (ImGui::BeginCombo("Theme", preview.c_str()))
    {
        for (const SEditorTheme& theme : s_vThemes)
        {
            const bool isSelected = theme.id == m_Preferences.m_ThemeName;
            ImGui::PushID(theme.id.c_str());
            if (ImGui::Selectable(theme.name.c_str(), isSelected) &&
                CThemeManager::Apply(theme.id))
            {
                m_Preferences.m_ThemeName = theme.id;
                m_Preferences.m_LightTheme = theme.id == "quark-light";
                ImGui::GetStyle().ScaleAllSizes(m_Preferences.m_InterfaceScale);
                ImGui::GetStyle().FontScaleMain = m_Preferences.m_InterfaceScale;
                iface.BaseStyle = ImGui::GetStyle();
                iface.BaseScale = m_Preferences.m_InterfaceScale;
                changed = true;
            }
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    changed |= ImGui::Checkbox("Show light helpers", &m_Preferences.m_ShowLightHelpers);
    changed |= ImGui::Checkbox("Show camera frustum", &m_Preferences.m_ShowCameras);
    changed |= ImGui::Checkbox("Show selection visualization", &m_Preferences.m_ShowSelectionVisualization);
    changed |= ImGui::SliderInt("Asset preview size", &m_Preferences.m_AssetPreviewSize, 32, 128);

    const char* apAssetFilterNames[] = { "All", "Images + Models", "Materials", "Texture Metadata", "Prefabs" };
    ImGui::Text("Asset type filter");
    changed |= ImGui::Combo("##asset_type_filter", &m_Preferences.m_AssetFilter, apAssetFilterNames, IM_ARRAYSIZE(apAssetFilterNames));

    float aSelectionColor[3] = {
        m_Preferences.m_SelectionRed / 255.0f,
        m_Preferences.m_SelectionGreen / 255.0f,
        m_Preferences.m_SelectionBlue / 255.0f
    };
    if (ImGui::ColorEdit3("Selection color", aSelectionColor))
    {
        m_Preferences.m_SelectionRed = static_cast<int>(aSelectionColor[0] * 255.0f);
        m_Preferences.m_SelectionGreen = static_cast<int>(aSelectionColor[1] * 255.0f);
        m_Preferences.m_SelectionBlue = static_cast<int>(aSelectionColor[2] * 255.0f);
        changed = true;
    }

    float aWireframeColor[3] = {
        m_Preferences.m_WireframeRed / 255.0f,
        m_Preferences.m_WireframeGreen / 255.0f,
        m_Preferences.m_WireframeBlue / 255.0f
    };
    if (ImGui::ColorEdit3("Wireframe color", aWireframeColor))
    {
        m_Preferences.m_WireframeRed = static_cast<int>(aWireframeColor[0] * 255.0f);
        m_Preferences.m_WireframeGreen = static_cast<int>(aWireframeColor[1] * 255.0f);
        m_Preferences.m_WireframeBlue = static_cast<int>(aWireframeColor[2] * 255.0f);
        changed = true;
    }

    float aBoundsColor[3] = {
        m_Preferences.m_BoundsRed / 255.0f,
        m_Preferences.m_BoundsGreen / 255.0f,
        m_Preferences.m_BoundsBlue / 255.0f
    };
    if (ImGui::ColorEdit3("Bounding box color", aBoundsColor))
    {
        m_Preferences.m_BoundsRed = static_cast<int>(aBoundsColor[0] * 255.0f);
        m_Preferences.m_BoundsGreen = static_cast<int>(aBoundsColor[1] * 255.0f);
        m_Preferences.m_BoundsBlue = static_cast<int>(aBoundsColor[2] * 255.0f);
        changed = true;
    }

    if (ImGui::SliderFloat("Interface scale", &m_Preferences.m_InterfaceScale, 0.75f, 2.0f, "%.2fx"))
    {
        ImGui::GetStyle() = iface.BaseStyle;
        ImGui::GetStyle().ScaleAllSizes(m_Preferences.m_InterfaceScale / iface.BaseScale);
        if (ImGui::GetStyle().SeparatorSize <= 0.0f)
        {
            ImGui::GetStyle().SeparatorSize = 1.0f;
        }
        if (ImGui::GetStyle().WindowBorderHoverPadding <= 0.0f)
        {
            ImGui::GetStyle().WindowBorderHoverPadding = 1.0f;
        }
        if (ImGui::GetStyle().WindowMinSize.x < 1.0f)
        {
            ImGui::GetStyle().WindowMinSize.x = 1.0f;
        }
        if (ImGui::GetStyle().WindowMinSize.y < 1.0f)
        {
            ImGui::GetStyle().WindowMinSize.y = 1.0f;
        }
        ImGui::GetStyle().FontScaleMain = m_Preferences.m_InterfaceScale;
        changed = true;
    }

    return changed;
}

void CEditor::DrawConfirmationModals()
{
    SModalState& modal = m_Ui.m_Modal;

    if (modal.PendingDelete.pEditor && modal.PendingDelete.pEntity)
    {
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Delete selected entity?");
            if (ImGui::Button("Delete"))
            {
                CEditor* pEditorToDelete = modal.PendingDelete.pEditor;
                CEntity* pEntityToDelete = modal.PendingDelete.pEntity;
                modal.PendingDelete.pEditor = nullptr;
                modal.PendingDelete.pEntity = nullptr;
                const bool previousConfirm = m_Preferences.m_ConfirmDelete;
                m_Preferences.m_ConfirmDelete = false;
                CSceneEntityCommands::Delete(*pEditorToDelete, pEntityToDelete);
                m_Preferences.m_ConfirmDelete = previousConfirm;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                modal.PendingDelete.pEditor = nullptr;
                modal.PendingDelete.pEntity = nullptr;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (modal.ShowExitConfirmation)
    {
        ImGui::OpenPopup("Unsaved Changes");
        modal.ShowExitConfirmation = false;
    }
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("The scene has unsaved changes.");
        if (ImGui::Button("Save and Exit"))
        {
            CProjectService::Save(m_ProjectPath, m_Scene);
            DispatchPluginEvent(PLUGIN_EVENT_SCENE_SAVED);
            m_SceneDirty = false;
            CloseWindow();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Exit Without Saving"))
        {
            CloseWindow();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
