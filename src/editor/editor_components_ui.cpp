#include "tex.h"
#include "editor/editor_components_ui.h"
#include "scripting/script_component.h"
#include "scripting/script_engine.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "editor/editor_desktop.h"
#include "imgui.h"
#include "text_mesh.h"
#include "models.h"
#include "entity.h"
#include "editor/editor.h"
#include "application_plugin_bridge.h"
#include "language_manager.h"
#include <filesystem>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#endif
#define lang CLanguageManager::Get()

namespace
{

    std::string BrowseForFileDialog(const char* pFilterLabel, const char* pFilter)
    {
        return CDesktopIntegration::OpenFileDialog(pFilterLabel, pFilter);
    }

} // anonymous

void CComponentUIHelper::DrawEntityInspector(CEditor& editor, CEntity& entity, Shader shader)
{
    ImGui::Spacing();

    if (entity.GetComponents())
    {
        auto pComponentsManager = entity.GetComponents();
        size_t componentCount = pComponentsManager->GetComponentCount();
        editor.m_Ui.m_Inspector.m_ComponentToRemove = -1;

        for (size_t i = 0; i < componentCount; ++i)
        {
            auto pComp = pComponentsManager->GetComponent(i);
            if (!pComp)
            {
                continue;
            }

            std::string tabLabel = pComp->GetTypeName();
            ImGui::PushID(static_cast<int>(i));
            bool open = ImGui::CollapsingHeader(tabLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            if (open)
            {
                ImGui::PushID("ComponentBody");
                ImGui::Spacing();

                bool isEnabled = pComp->m_Enabled;
                if (ImGui::Checkbox(lang.Word("enabled"), &isEnabled))
                {
                    editor.SaveState();
                    pComp->m_Enabled = isEnabled;
                    editor.m_SceneDirty = true;
                }

                ImGui::Spacing();

                if (auto pTransform = std::dynamic_pointer_cast<CTransformComponent>(pComp))
                {
                    DrawTransformComponent(editor, entity, pTransform.get());
                }
                else if (auto pMesh = std::dynamic_pointer_cast<CMeshComponent>(pComp))
                {
                    DrawMeshComponent(editor, entity, pMesh.get());
                }
                else if (auto pLight = std::dynamic_pointer_cast<CLightComponent>(pComp))
                {
                    DrawLightComponent(editor, entity, pLight.get(), shader);
                }
                else if (auto pMaterial = std::dynamic_pointer_cast<CMaterialComponent>(pComp))
                {
                    DrawMaterialComponent(editor, entity, pMaterial.get());
                }
                else if (auto pCollision = std::dynamic_pointer_cast<CCollisionComponent>(pComp))
                {
                    DrawCollisionComponent(editor, entity, pCollision.get());
                }
                else if (auto pCamera = std::dynamic_pointer_cast<CCameraComponent>(pComp))
                {
                    float fieldOfView = pCamera->m_FieldOfView;
                    const bool fieldOfViewChanged =
                        ImGui::SliderFloat("Field of View", &fieldOfView, 20.0f, 120.0f, "%.0f deg");
                    if (ImGui::IsItemActivated())
                    {
                        editor.SaveState();
                    }
                    if (fieldOfViewChanged)
                    {
                        pCamera->m_FieldOfView = fieldOfView;
                    }
                }
                else if (auto pText = std::dynamic_pointer_cast<CText3DComponent>(pComp))
                {
                    Draw3dTextComponent(editor, entity, pText.get());
                }
                else if (auto pText = std::dynamic_pointer_cast<CTextComponent>(pComp))
                {
                    DrawUiTextComponent(editor, pText.get());
                }
                else if (auto pRectTransform = std::dynamic_pointer_cast<CUiRectTransformComponent>(pComp))
                {
                    DrawUiRectTransformComponent(editor, pRectTransform.get());
                }
                else if (auto pCanvas = std::dynamic_pointer_cast<CCanvasComponent>(pComp))
                {
                    DrawCanvasComponent(editor, entity, pCanvas.get());
                }
                else if (auto pImage = std::dynamic_pointer_cast<CImageComponent>(pComp))
                {
                    DrawImageComponent(editor, entity, pImage.get());
                }
                else if (auto pPanel = std::dynamic_pointer_cast<CPanelComponent>(pComp))
                {
                    DrawPanelComponent(editor, entity, pPanel.get());
                }
                else if (auto pButton = std::dynamic_pointer_cast<CButtonComponent>(pComp))
                {
                    DrawButtonComponent(editor, entity, pButton.get());
                }
                else if (auto pScript = std::dynamic_pointer_cast<CScriptComponent>(pComp))
                {
                    DrawScriptComponent(editor, entity, pScript.get());
                }
                else if (auto pUi = std::dynamic_pointer_cast<CUiComponent>(pComp))
                {
                    DrawUiComponent(editor, entity, pUi.get());
                }

                ImGui::Spacing();

                bool canRemove = (pComp->GetType() != COMPONENT_TRANSFORM &&
                                  pComp->GetType() != COMPONENT_UI_RECT_TRANSFORM &&
                                  pComp->GetType() != COMPONENT_MESH);
                if (!canRemove)
                {
                    ImGui::BeginDisabled();
                }

                if (canRemove)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button(lang.Word("remove_component"), ImVec2(-1, 0)))
                    {
                        editor.m_Ui.m_Inspector.m_ComponentToRemove = static_cast<int>(i);
                    }
                    ImGui::PopStyleColor(2);
                }

                if (!canRemove)
                {
                    ImGui::EndDisabled();
                }
                ImGui::PopID();
            }
            ImGui::PopID();
            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::Spacing();
        if (ImGui::Button(lang.Word("add_component"), ImVec2(-1, 0)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            auto pComponentsManager = entity.GetComponents();

            const bool hasMaterial = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_MATERIAL)
                    {
                        return true;
                    }
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.Word("material")))
            {
                if (!hasMaterial)
                {
                    editor.SaveState();
                    pComponentsManager->AddComponent(std::make_shared<CMaterialComponent>());
                }
            }

            const bool hasCollision = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_COLLISION)
                    {
                        return true;
                    }
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.Word("collision")))
            {
                if (!hasCollision)
                {
                    editor.SaveState();
                    pComponentsManager->AddComponent(std::make_shared<CCollisionComponent>());
                }
            }

            const bool hasLight = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_LIGHT)
                    {
                        return true;
                    }
                }
                return false;
            }();
            const bool hasCamera = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_CAMERA)
                    {
                        return true;
                    }
                }
                return false;
            }();
            if (ImGui::MenuItem("Camera"))
            {
                if (!hasCamera)
                {
                    editor.SaveState();
                    pComponentsManager->AddComponent(std::make_shared<CCameraComponent>());
                }
            }

            if (ImGui::MenuItem(lang.Word("light")))
            {
                if (!hasLight)
                {
                    editor.SaveState();
                    auto pLight = std::make_shared<CLightComponent>();
                    if (auto pTransform = entity.GetTransformComponent())
                    {
                        pLight->m_Light.m_Position = pTransform->m_Position;
                    }
                    pComponentsManager->AddComponent(pLight);
                }
            }

            const bool hasText = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_CUSTOM)
                    {
                        return true;
                    }
                }
                return false;
            }();
            const bool hasScript = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_SCRIPT)
                    {
                        return true;
                    }
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.Word("script")))
            {
                if (!hasScript)
                {
                    editor.SaveState();
                    pComponentsManager->AddComponent(std::make_shared<CScriptComponent>());
                }
            }

            if (ImGui::MenuItem(lang.Word("text")))
            {
                if (!hasText)
                {
                    editor.SaveState();
                    pComponentsManager->AddComponent(std::make_shared<CText3DComponent>());
                }
            }

            const bool hasUi = [&]()
            {
                for (size_t j = 0; j < pComponentsManager->GetComponentCount(); ++j)
                {
                    auto pExisting = pComponentsManager->GetComponent(j);
                    if (pExisting && pExisting->GetType() == COMPONENT_UI)
                    {
                        return true;
                    }
                }
                return false;
            }();
            if (ImGui::MenuItem("Canvas"))
            {
                if (!pComponentsManager->GetComponentOfType<CCanvasComponent>())
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi(true);
                    pComponentsManager->AddComponent(std::make_shared<CCanvasComponent>());
                }
            }
            if (ImGui::MenuItem("Rect Transform"))
            {
                if (!pComponentsManager->GetComponentOfType<CUiRectTransformComponent>())
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi(false);
                    pComponentsManager->AddComponent(std::make_shared<CUiRectTransformComponent>());
                }
            }
            if (ImGui::MenuItem("Image"))
            {
                if (!pComponentsManager->GetComponentOfType<CImageComponent>())
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi(false);
                    if (!pComponentsManager->GetComponentOfType<CUiRectTransformComponent>())
                    {
                        pComponentsManager->AddComponent(std::make_shared<CUiRectTransformComponent>());
                    }
                    pComponentsManager->AddComponent(std::make_shared<CImageComponent>());
                }
            }
            if (ImGui::MenuItem("Panel"))
            {
                if (!pComponentsManager->GetComponentOfType<CPanelComponent>())
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi(false);
                    if (!pComponentsManager->GetComponentOfType<CUiRectTransformComponent>())
                    {
                        pComponentsManager->AddComponent(std::make_shared<CUiRectTransformComponent>());
                    }
                    pComponentsManager->AddComponent(std::make_shared<CPanelComponent>());
                }
            }
            if (ImGui::MenuItem("Button"))
            {
                if (!pComponentsManager->GetComponentOfType<CButtonComponent>())
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi(false);
                    if (!pComponentsManager->GetComponentOfType<CUiRectTransformComponent>())
                    {
                        pComponentsManager->AddComponent(std::make_shared<CUiRectTransformComponent>());
                    }
                    pComponentsManager->AddComponent(std::make_shared<CButtonComponent>());
                }
            }
            if (ImGui::MenuItem("Text"))
            {
                if (!pComponentsManager->GetComponentOfType<CTextComponent>())
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi(false);
                    if (!pComponentsManager->GetComponentOfType<CUiRectTransformComponent>())
                    {
                        pComponentsManager->AddComponent(std::make_shared<CUiRectTransformComponent>());
                    }
                    pComponentsManager->AddComponent(std::make_shared<CTextComponent>());
                }
            }
            if (ImGui::MenuItem("UI"))
            {
                if (!hasUi)
                {
                    editor.SaveState();
                    pComponentsManager->RemoveMeshAndMaterialForUi();
                    auto pUi = std::make_shared<CUiComponent>();
                    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
                    auto pText = std::make_shared<CTextComponent>();
                    pText->m_Text = "Button";
                    pComponentsManager->AddComponent(pRectTransform);
                    pComponentsManager->AddComponent(pUi);
                    pComponentsManager->AddComponent(pText);
                    pComponentsManager->RemoveMeshAndMaterialForUi();
                }
            }
            ImGui::EndPopup();
        }

        if (editor.m_Ui.m_Inspector.m_ComponentToRemove != -1)
        {
            const int componentToRemove = editor.m_Ui.m_Inspector.m_ComponentToRemove;
            const int entityIndex = static_cast<int>(&entity - editor.m_Scene.m_vEntities.data());
            editor.SaveState();
            std::shared_ptr<IComponent> pComp = pComponentsManager->GetComponent(componentToRemove);

            if (std::shared_ptr<CLightComponent> pLight = std::dynamic_pointer_cast<CLightComponent>(pComp))
            {
                if (pLight->m_Created)
                {
                    pLight->m_Light.m_Enabled = false;

                    if (pLight->m_Light.m_Id != -1)
                    {
                        UpdateLighting(shader, pLight->m_Light);
                        editor.m_Lights.Free(pLight->m_Light.m_Id);
                    }
                }
            }

            pComponentsManager->RemoveComponent(componentToRemove);
            editor.m_Ui.m_Inspector.m_ComponentToRemove = -1;
        }
    }
}

void CComponentUIHelper::DrawTransformComponent(CEditor& editor, CEntity& entity, CTransformComponent* pTransform)
{
    if (!pTransform)
    {
        return;
    }

    const auto trackTransformEdit = [&]()
    {
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    };

    float aPosition[3] = { pTransform->m_Position.x, pTransform->m_Position.y, pTransform->m_Position.z };
    float aRotation[3] = { pTransform->m_Rotation.x, pTransform->m_Rotation.y, pTransform->m_Rotation.z };
    float aScale[3] = { pTransform->m_Scale.x, pTransform->m_Scale.y, pTransform->m_Scale.z };

    if (ImGui::DragFloat3(lang.Word("position"), aPosition, 0.1f))
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Position = Vec3(aPosition[0], aPosition[1], aPosition[2]);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED,
                            static_cast<int>(&entity - editor.m_Scene.m_vEntities.data()));
        CEntityTextureService::MarkEntityBoundsDirty(&entity);
    }
    trackTransformEdit();

    if (ImGui::DragFloat3(lang.Word("rotation"), aRotation, 1.0f))
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Rotation = Vec3(aRotation[0], aRotation[1], aRotation[2]);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED,
                            static_cast<int>(&entity - editor.m_Scene.m_vEntities.data()));
        CEntityTextureService::MarkEntityBoundsDirty(&entity);
    }
    trackTransformEdit();

    if (ImGui::DragFloat3(lang.Word("scale"), aScale, 0.1f))
    {
        pTransform->ClearLocalMatrixOverride();
        pTransform->m_Scale = Vec3(aScale[0], aScale[1], aScale[2]);
        DispatchPluginEvent(PLUGIN_EVENT_TRANSFORM_CHANGED,
                            static_cast<int>(&entity - editor.m_Scene.m_vEntities.data()));
        CEntityTextureService::MarkEntityBoundsDirty(&entity);
        CEntityTextureService::MarkEntityUVDirty(&entity);
    }
    trackTransformEdit();
}

void CComponentUIHelper::DrawMeshComponent(CEditor& editor, CEntity& entity, CMeshComponent* pMesh)
{
    SVertexEditState& edit = editor.m_Ui.m_VertexEdit;

    if (!pMesh)
    {
        return;
    }

    ImGui::Text("%s", lang.Word("mesh_config"));
    ImGui::Spacing();

    if (pMesh->m_pAsset)
    {
        ImGui::Text("%s: %s", lang.Word("asset"), pMesh->m_pAsset->m_Name.c_str());
    }
    else
    {
        ImGui::Text("%s: %s", lang.Word("asset"), lang.Word("none"));
    }

    const char* apObjectTypeNames[] = {
        lang.Word("cube"),
        lang.Word("sphere"),
        lang.Word("cone"),
        lang.Word("cylinder"),
        lang.Word("hemisphere"),
        lang.Word("torus")
    };

    int currentObjectTypeIndex = static_cast<int>(pMesh->m_Type);
    bool isProceduralAsset = pMesh->m_pAsset && pMesh->m_pAsset->m_IsProcedural;

    if (!isProceduralAsset)
    {
        ImGui::Text(lang.Word("mesh_type"), pMesh->m_AssetName.c_str());
        ImGui::BeginDisabled();
    }

    if (ImGui::Combo(lang.Word("mesh_type_combo"), &currentObjectTypeIndex, apObjectTypeNames, IM_ARRAYSIZE(apObjectTypeNames)))
    {
        if (currentObjectTypeIndex != static_cast<int>(pMesh->m_Type))
        {
            editor.SaveState();
            EObjectType newType = static_cast<EObjectType>(currentObjectTypeIndex);

            CModelAsset* pNewAsset = nullptr;
            for (auto& assetEntry : editor.m_Assets.Models())
            {
                if (assetEntry.m_IsProcedural && assetEntry.m_Type == newType)
                {
                    pNewAsset = &assetEntry;
                    break;
                }
            }

            if (pNewAsset)
            {
                pMesh->m_Type = newType;
                pMesh->m_pAsset = pNewAsset;
                pMesh->m_AssetName = pNewAsset->m_Name;
                CModelService::UpdateModel(&entity, editor.m_Text);
                CEntityTextureService::MarkEntityBoundsDirty(&entity);
                CEntityTextureService::MarkEntityUVDirty(&entity);
                CEntityTextureService::StoreUV(&entity);
                CEntityTextureService::StoreMaterialTextures(&entity);
            }
        }
    }

    if (!isProceduralAsset)
    {
        ImGui::EndDisabled();
    }

    if (pMesh->m_pAsset && pMesh->m_pAsset->m_IsProcedural)
    {
        if (ImGui::DragInt(lang.Word("segments"), &pMesh->m_Segments, 1, 3, 100))
        {
            editor.SaveState();
            CModelService::UpdateModel(&entity, editor.m_Text);
            CEntityTextureService::StoreUV(&entity);
            CEntityTextureService::StoreMaterialTextures(&entity);
            CEntityTextureService::MarkEntityBoundsDirty(&entity);
            CEntityTextureService::MarkEntityUVDirty(&entity);
        }
    }
    else
    {
        ImGui::Text(lang.Word("segments_loaded"), pMesh->m_Segments);
    }

    ImGui::Separator();

    if (ImGui::Checkbox(lang.Word("vertex_gizmo"), &pMesh->m_VertexGizmo))
    {
        if (pMesh->m_VertexGizmo && pMesh->m_EditableMesh.m_vVertices.empty() && HasValidModelData(pMesh->m_Model))
        {
            pMesh->m_EditableMesh.m_vVertices.clear();
            pMesh->m_EditableMesh.m_vTriangles.clear();
            pMesh->m_IsEditableMesh = true;

            const Mesh& m = pMesh->m_Model.meshes[0];

            std::vector<int> vRemap(m.vertexCount, -1);
            for (int i = 0; i < m.vertexCount; i++)
            {
                Vec3 pos = {
                    m.vertices[i * 3 + 0],
                    m.vertices[i * 3 + 1],
                    m.vertices[i * 3 + 2]
                };

                vRemap[i] = (int)pMesh->m_EditableMesh.m_vVertices.size();
                SEditableVertex ev;
                ev.m_Position = pos;
                if (m.texcoords)
                {
                    ev.m_U = m.texcoords[i * 2 + 0];
                    ev.m_V = m.texcoords[i * 2 + 1];
                }
                pMesh->m_EditableMesh.m_vVertices.push_back(ev);
            }

            for (int t = 0; t < m.triangleCount; t++)
            {
                int ia, ib, ic;
                if (m.indices)
                {
                    ia = m.indices[t * 3 + 0];
                    ib = m.indices[t * 3 + 1];
                    ic = m.indices[t * 3 + 2];
                }
                else
                {
                    ia = t * 3 + 0;
                    ib = t * 3 + 1;
                    ic = t * 3 + 2;
                }

                if (ia >= m.vertexCount || ib >= m.vertexCount || ic >= m.vertexCount)
                {
                    continue;
                }

                SEditableTriangle tri;
                tri.m_A = vRemap[ia];
                tri.m_B = vRemap[ib];
                tri.m_C = vRemap[ic];

                if (tri.m_A == tri.m_B || tri.m_B == tri.m_C || tri.m_A == tri.m_C)
                {
                    continue;
                }

                pMesh->m_EditableMesh.m_vTriangles.push_back(tri);
            }

            if (edit.m_vSelectedVertices.empty() && !pMesh->m_EditableMesh.m_vVertices.empty())
            {
                edit.m_vSelectedVertices.push_back(0);
            }
        }
    }

    if (pMesh->m_VertexGizmo)
    {
        ImGui::Text(lang.Word("vertices"), (int)pMesh->m_EditableMesh.m_vVertices.size());
        ImGui::Text(lang.Word("triangles"), (int)pMesh->m_EditableMesh.m_vTriangles.size());

        if (!edit.m_vSelectedVertices.empty())
        {
            SEditableVertex& selectedVertex = pMesh->m_EditableMesh.m_vVertices[edit.m_vSelectedVertices[0]];
            float aVertexPos[3] = { selectedVertex.m_Position.x, selectedVertex.m_Position.y, selectedVertex.m_Position.z };

            ImGui::Separator();

            if (ImGui::DragFloat3("##vertex_position", aVertexPos, 0.1f))
            {
                editor.SaveState();

                selectedVertex.m_Position = Vec3(aVertexPos[0],
                                                 aVertexPos[1],
                                                 aVertexPos[2]);

                RebuildMeshFromEditable(pMesh->m_Model, pMesh->m_EditableMesh);

                CEntityTextureService::MarkEntityBoundsDirty(&entity);
                CEntityTextureService::MarkEntityUVDirty(&entity);
            }

            ImGui::Text("%s", lang.Word("vertex_position"));
        }
    }
}

void CComponentUIHelper::DrawMaterialComponent(CEditor& editor, CEntity& entity, CMaterialComponent* pMaterial)
{
    if (!pMaterial)
    {
        return;
    }

    ImGui::Text("%s", lang.Word("material_properties"));
    ImGui::Spacing();

    CMaterialComponent* pMat = entity.GetMaterialComponent();
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMat || !pMesh)
    {
        return;
    }

    ImGui::Text("Material slots: %d", pMesh->m_Model.materialCount);
    for (int slot = 0; slot < pMesh->m_Model.materialCount; ++slot)
    {
        const int materialIndex = slot;
        if (materialIndex < 0 || materialIndex >= pMesh->m_Model.materialCount || !pMesh->m_Model.materials)
        {
            continue;
        }

        const Material& material = pMesh->m_Model.materials[materialIndex];
        const bool hasAlbedo = material.maps && material.maps[MATERIAL_MAP_ALBEDO].texture.valid;
        const bool hasNormal = material.maps && material.maps[MATERIAL_MAP_NORMAL].texture.valid;
        const bool hasRoughness = material.maps && material.maps[MATERIAL_MAP_ROUGHNESS].texture.valid;
        const bool hasMetallic = material.maps && material.maps[MATERIAL_MAP_METALNESS].texture.valid;
        ImGui::Text("Slot %d: Albedo %s | Normal %s | Roughness %s | Metallic %s",
                    slot,
                    hasAlbedo ? "yes" : "no",
                    hasNormal ? "yes" : "no",
                    hasRoughness ? "yes" : "no",
                    hasMetallic ? "yes" : "no");
    }

    SMaterialPickerState& picker = editor.m_Ui.m_MaterialPicker;

    if (picker.m_NeedsUpdate)
    {
        picker.m_vMaterialPaths = GetAllMaterialsInProject();
        picker.m_vMaterialPaths.insert(picker.m_vMaterialPaths.begin(), "None");
        picker.m_vDisplayNames.clear();
        picker.m_vMaterialNames.clear();

        for (const auto& matPath : picker.m_vMaterialPaths)
        {
            picker.m_vDisplayNames.push_back(
                matPath == "None" ? "None" : std::filesystem::path(matPath).filename().generic_string());
        }
        for (const auto& displayName : picker.m_vDisplayNames)
        {
            picker.m_vMaterialNames.push_back(displayName.c_str());
        }

        picker.m_NeedsUpdate = false;
    }

    if (pMesh->m_Model.materialCount > 0)
    {
        pMat->m_vMaterialSlotSources.resize(pMesh->m_Model.materialCount);
        ImGui::Separator();
        ImGui::Text("Material assignment by slot");
        for (int slot = 0; slot < pMesh->m_Model.materialCount; ++slot)
        {
            int slotMaterialIndex = 0;
            for (int i = 1; i < static_cast<int>(picker.m_vMaterialPaths.size()); ++i)
            {
                if (picker.m_vMaterialPaths[i] == pMat->m_vMaterialSlotSources[slot])
                {
                    slotMaterialIndex = i;
                    break;
                }
            }

            ImGui::PushID(slot);
            ImGui::Text("Slot %d", slot);
            ImGui::SameLine();
            if (ImGui::Combo("##slot_material", &slotMaterialIndex, picker.m_vMaterialNames.data(),
                             static_cast<int>(picker.m_vMaterialNames.size())))
            {
                editor.SaveState();
                if (slotMaterialIndex == 0)
                {
                    if (pMesh->m_Model.materials && pMesh->m_Model.materials[slot].maps)
                    {
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_ALBEDO].texture = { 0 };
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_NORMAL].texture = { 0 };
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_ROUGHNESS].texture = { 0 };
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_METALNESS].texture = { 0 };
                    }
                    pMat->m_vMaterialSlotSources[slot].clear();
                }
                else if (slotMaterialIndex < static_cast<int>(picker.m_vMaterialPaths.size()))
                {
                    LoadMaterialToEntity(&entity, picker.m_vMaterialPaths[slotMaterialIndex], slot);
                    pMat->m_vMaterialSlotSources[slot] = picker.m_vMaterialPaths[slotMaterialIndex];
                }
            }
            ImGui::PopID();
        }
    }

    picker.m_SelectedIndex = 0;
    if (!pMat->m_TextureName.empty())
    {
        const std::filesystem::path currentPath = std::filesystem::current_path();
        std::filesystem::path materialPath(pMat->m_TextureName);
        if (materialPath.is_absolute())
        {
            std::error_code relativeError;
            materialPath = std::filesystem::relative(materialPath, currentPath, relativeError);
            if (relativeError)
            {
                materialPath = std::filesystem::path(pMat->m_TextureName).filename();
            }
        }

        const std::string displayPath = materialPath.generic_string();
        for (int i = 1; i < static_cast<int>(picker.m_vMaterialPaths.size()); ++i)
        {
            if (picker.m_vMaterialPaths[i] == displayPath)
            {
                picker.m_SelectedIndex = i;
                break;
            }
        }
    }

    ImGui::Text("%s", lang.Word("material_file"));
    if (!picker.m_vMaterialNames.empty())
    {
        if (ImGui::Combo("##material_combo", &picker.m_SelectedIndex, picker.m_vMaterialNames.data(),
                         static_cast<int>(picker.m_vMaterialNames.size())))
        {
            editor.SaveState();
            if (picker.m_SelectedIndex == 0)
            {
                CEntityTextureService::ClearMaterialTextures(&entity);
                pMat->m_TextureName.clear();
                pMat->m_Texture = { 0 };
                pMat->m_TextureSource = TEXTURE_NONE;
            }
            else if (picker.m_SelectedIndex > 0 && picker.m_SelectedIndex < static_cast<int>(picker.m_vMaterialPaths.size()))
            {
                const std::string& selectedPath = picker.m_vMaterialPaths[picker.m_SelectedIndex];
                if (std::filesystem::exists(selectedPath))
                {
                    LoadMaterialToEntity(&entity, selectedPath);
                }
            }
        }
    }
    else
    {
        ImGui::Text("%s", lang.Word("no_materials_found"));
    }

    std::vector<const char*> vTextureNames;
    vTextureNames.reserve(editor.m_Assets.TextureCount());
    editor.m_Ui.m_Inspector.m_SelectedTextureIndex = 0;
    for (size_t i = 0; i < editor.m_Assets.TextureCount(); ++i)
    {
        vTextureNames.push_back(editor.m_Assets.TextureByIndex(i).m_Name.c_str());
        if (editor.m_Assets.TextureByIndex(i).m_Name == pMat->m_AlbedoTextureName)
        {
            editor.m_Ui.m_Inspector.m_SelectedTextureIndex = static_cast<int>(i);
        }
    }

    ImGui::Text("Direct texture");
    if (!vTextureNames.empty() && ImGui::Combo("##direct_texture", &editor.m_Ui.m_Inspector.m_SelectedTextureIndex, vTextureNames.data(), static_cast<int>(vTextureNames.size())))
    {
        editor.SaveState();
        const int selectedTextureIndex = editor.m_Ui.m_Inspector.m_SelectedTextureIndex;
        if (selectedTextureIndex == 0)
        {
            pMat->m_AlbedoTextureName.clear();
            pMat->m_Texture = { 0 };
            if (pMat->m_TextureName.empty())
            {
                pMat->m_TextureSource = TEXTURE_NONE;
                CEntityTextureService::ClearMaterialTextures(&entity);
            }
            else if (std::filesystem::exists(pMat->m_TextureName))
            {
                LoadMaterialToEntity(&entity, pMat->m_TextureName);
            }
        }
        else
        {
            pMat->m_AlbedoTextureName = editor.m_Assets.TextureByIndex(selectedTextureIndex).m_Name;
            pMat->m_TextureName.clear();
            pMat->m_Texture = editor.m_Assets.TextureByIndex(selectedTextureIndex).m_Texture;
            pMat->m_TextureSource = TEXTURE_EXTERNAL;
        }
        CEntityTextureService::MarkEntityUVDirty(&entity);
    }

    editor.m_Ui.m_Inspector.m_SelectedNormalIndex = 0;
    for (size_t i = 0; i < editor.m_Assets.TextureCount(); ++i)
    {
        if (editor.m_Assets.TextureByIndex(i).m_Name == pMat->m_NormalTextureName)
        {
            editor.m_Ui.m_Inspector.m_SelectedNormalIndex = static_cast<int>(i);
            break;
        }
    }
    ImGui::Text("Normal map");
    if (!vTextureNames.empty() && ImGui::Combo("##normal_map", &editor.m_Ui.m_Inspector.m_SelectedNormalIndex, vTextureNames.data(), static_cast<int>(vTextureNames.size())))
    {
        editor.SaveState();
        const int selectedNormalIndex = editor.m_Ui.m_Inspector.m_SelectedNormalIndex;
        if (selectedNormalIndex == 0)
        {
            pMat->m_NormalTextureName.clear();
            for (int m = 0; m < pMesh->m_Model.materialCount; ++m)
            {
                if (pMesh->m_Model.materials[m].maps)
                {
                    pMesh->m_Model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = { 0 };
                }
            }
        }
        else
        {
            pMat->m_NormalTextureName = editor.m_Assets.TextureByIndex(selectedNormalIndex).m_Name;
            const Texture2D normalTexture = editor.m_Assets.TextureByIndex(selectedNormalIndex).m_Texture;
            for (int m = 0; m < pMesh->m_Model.materialCount; ++m)
            {
                if (pMesh->m_Model.materials[m].maps)
                {
                    pMesh->m_Model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = normalTexture;
                }
            }
        }
    }

    if (!pMat->m_TextureName.empty() && pMat->m_TextureName.length() > 4 &&
        pMat->m_TextureName.substr(pMat->m_TextureName.length() - 4) == ".mtl")
    {
        ImGui::TextDisabled("%s: %s", lang.Word("current"), pMat->m_TextureName.c_str());
    }
    else
    {
        ImGui::TextDisabled("%s: %s", lang.Word("current"), lang.Word("none"));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("%s", lang.Word("uv_settings"));
    ImGui::Spacing();

    bool uvChanged = false;

    const auto trackMaterialEdit = [&]()
    {
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    };

    bool textureStretch = pMat->m_TextureStretch;
    if (ImGui::Checkbox(lang.Word("stretch_texture"), &textureStretch))
    {
        trackMaterialEdit();
        pMat->m_TextureStretch = textureStretch;
        uvChanged = true;
    }

    if (!pMat->m_TextureStretch)
    {
        if (ImGui::DragFloat(lang.Word("repeat_u"), &pMat->m_TextureRepeatU, 0.1f, 0.1f, 10.0f))
        {
            trackMaterialEdit();
            uvChanged = true;
        }

        if (ImGui::DragFloat(lang.Word("repeat_v"), &pMat->m_TextureRepeatV, 0.1f, 0.1f, 10.0f))
        {
            trackMaterialEdit();
            uvChanged = true;
        }

        float aUvScale[2] = { pMat->m_UvScale.x, pMat->m_UvScale.y };
        if (ImGui::DragFloat2(lang.Word("uv_scale_x"), aUvScale, 0.1f, 0.1f, 5.0f))
        {
            trackMaterialEdit();
            pMat->m_UvScale = { aUvScale[0], aUvScale[1] };
            uvChanged = true;
        }
    }

    bool autoUv = pMat->m_AutoUv;
    if (ImGui::Checkbox(lang.Word("auto_uv"), &autoUv))
    {
        trackMaterialEdit();
        pMat->m_AutoUv = autoUv;
        uvChanged = true;
    }

    if (uvChanged)
    {
        CEntityTextureService::MarkEntityUVDirty(&entity);
    }
}

void CComponentUIHelper::DrawLightComponent(CEditor& editor, CEntity& entity, CLightComponent* pLight, Shader shader)
{
    if (!pLight)
    {
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        editor.SaveState();
    }

    ImGui::Text("%s", lang.Word("light_properties"));
    ImGui::Spacing();

    bool changed = false;

    CTransformComponent* pTransform = entity.GetTransformComponent();
    float aLightPosition[3] = {
        pTransform ? pTransform->m_Position.x : pLight->m_Light.m_Position.x,
        pTransform ? pTransform->m_Position.y : pLight->m_Light.m_Position.y,
        pTransform ? pTransform->m_Position.z : pLight->m_Light.m_Position.z
    };
    if (ImGui::DragFloat3(lang.Word("position"), aLightPosition, 0.1f))
    {
        if (pTransform)
        {
            pTransform->ClearLocalMatrixOverride();
            pTransform->m_Position = Vec3(aLightPosition[0], aLightPosition[1], aLightPosition[2]);
            CEntityTextureService::MarkEntityBoundsDirty(&entity);
        }
        pLight->m_Light.m_Position = Vec3(aLightPosition[0], aLightPosition[1], aLightPosition[2]);
        changed = true;
    }
    changed |= ImGui::DragFloat3(lang.Word("target"), (float*)&pLight->m_Light.m_Target, 0.1f);

    float aLightColor[4] = {
        pLight->m_Light.m_Color.r / 255.0f,
        pLight->m_Light.m_Color.g / 255.0f,
        pLight->m_Light.m_Color.b / 255.0f,
        pLight->m_Light.m_Color.a / 255.0f
    };
    if (ImGui::ColorEdit4(lang.Word("color"), aLightColor))
    {
        pLight->m_Light.m_Color = {
            static_cast<unsigned char>(aLightColor[0] * 255.0f),
            static_cast<unsigned char>(aLightColor[1] * 255.0f),
            static_cast<unsigned char>(aLightColor[2] * 255.0f),
            static_cast<unsigned char>(aLightColor[3] * 255.0f)
        };
        changed = true;
    }

    changed |= ImGui::DragFloat(lang.Word("intensity"), &pLight->m_Light.m_Intensity, 0.1f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(lang.Word("range"), &pLight->m_Light.m_Range, 0.1f, 0.1f, 100.0f);
    changed |= ImGui::DragFloat(lang.Word("spot_angle"), &pLight->m_Light.m_SpotAngle, 1.0f, 5.0f, 180.0f);

    const char* apLightTypes[] = {
        lang.Word("directional"),
        lang.Word("point"),
        lang.Word("spot")
    };

    const int aLightTypeValues[] = {
        LIGHT_TYPE_DIRECTIONAL,
        LIGHT_TYPE_POINT,
        LIGHT_TYPE_SPOT
    };

    int lightTypeIndex = 0;
    for (int i = 0; i < IM_ARRAYSIZE(aLightTypeValues); i++)
    {
        if (pLight->m_Light.m_Light.type == aLightTypeValues[i])
        {
            lightTypeIndex = i;
            break;
        }
    }

    if (ImGui::Combo(lang.Word("light_type"), &lightTypeIndex, apLightTypes, IM_ARRAYSIZE(apLightTypes)))
    {
        pLight->m_Light.m_Light.type = aLightTypeValues[lightTypeIndex];
        changed = true;
    }

    if (pLight->m_Enabled != pLight->m_Light.m_Enabled)
    {
        changed = true;
    }

    if (changed)
    {
        if (pTransform)
        {
            pLight->m_Light.m_Position = pTransform->m_Position;
        }
        pLight->m_Light.m_Enabled = pLight->m_Enabled;

        if (pLight->m_Light.m_Id != -1)
        {
            UpdateLighting(shader, pLight->m_Light);
        }
    }
}

void CComponentUIHelper::DrawCollisionComponent(CEditor& editor, CEntity& entity, CCollisionComponent* pCollision)
{
    if (!pCollision)
    {
        return;
    }

    ImGui::Text("%s", lang.Word("collision_properties"));
    ImGui::Spacing();

    bool changed = false;

    const char* apColliderTypes[] = {
        lang.Word("cube"),
        lang.Word("sphere"),
        lang.Word("capsule"),
        lang.Word("mesh")
    };

    int colliderType = static_cast<int>(pCollision->m_ColliderType);

    if (ImGui::Combo(lang.Word("collider_type"), &colliderType, apColliderTypes, IM_ARRAYSIZE(apColliderTypes)))
    {
        editor.SaveState();

        pCollision->m_ColliderType = static_cast<EColliderType>(colliderType);
        pCollision->m_Dirty = true;
        changed = true;
    }

    if (ImGui::Checkbox(lang.Word("visualize"), &pCollision->m_Visualize))
    {
        editor.SaveState();
        changed = true;
    }

    if (ImGui::DragFloat3(lang.Word("center"), (float*)&pCollision->m_Center, 0.1f))
    {
        editor.SaveState();

        pCollision->m_Dirty = true;
        changed = true;
    }

    ImGui::Spacing();

    switch (pCollision->m_ColliderType)
    {
        case COLLIDER_BOX:
        {
            if (ImGui::DragFloat3(lang.Word("size"), (float*)&pCollision->m_Size, 0.1f, 0.01f, 1000.0f))
            {
                editor.SaveState();

                pCollision->m_Dirty = true;
                changed = true;
            }

            break;
        }

        case COLLIDER_SPHERE:
        {
            if (ImGui::DragFloat(lang.Word("radius"), (float*)&pCollision->m_Radius, 0.1f, 0.01f, 1000.0f))
            {
                editor.SaveState();

                pCollision->m_Dirty = true;
                changed = true;
            }

            break;
        }

        case COLLIDER_CAPSULE:
        {
            if (ImGui::DragFloat(lang.Word("radius"), (float*)&pCollision->m_Radius, 0.1f, 0.01f, 1000.0f))
            {
                editor.SaveState();

                pCollision->m_Dirty = true;
                changed = true;
            }

            if (ImGui::DragFloat(lang.Word("height"), (float*)&pCollision->m_Height, 0.1f, 0.1f, 1000.0f))
            {
                editor.SaveState();

                pCollision->m_Dirty = true;
                changed = true;
            }

            break;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button(lang.Word("reset"), ImVec2(-1, 0)))
    {
        pCollision->m_Size = Vec3(1.0f, 1.0f, 1.0f);
        pCollision->m_Radius = 0.5f;
        pCollision->m_Height = 2.0f;
        pCollision->m_Center = Vec3(0.0f, 0.0f, 0.0f);
    }
    ImGui::PopStyleColor(2);

    if (changed)
    {
        CEntityTextureService::MarkEntityBoundsDirty(&entity);
    }
}

void CComponentUIHelper::Draw3dTextComponent(CEditor& editor, CEntity& entity, CText3DComponent* pText)
{
    if (!pText)
    {
        return;
    }

    char aBuf[256] = {};
    std::snprintf(aBuf, sizeof(aBuf), "%s", pText->m_Text.c_str());

    if (ImGui::InputText("Text", aBuf, sizeof(aBuf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        editor.SaveState();
        pText->m_Text = aBuf;
        CModelService::UpdateModel(&entity, editor.m_Text);
        CEntityTextureService::MarkEntityBoundsDirty(&entity);
    }

    auto drag = [&](const char* pLabel, float& val, float spd, float mn, float mx)
    {
        if (ImGui::DragFloat(pLabel, &val, spd, mn, mx))
        {
            editor.SaveState();
            CModelService::UpdateModel(&entity, editor.m_Text);
            CEntityTextureService::MarkEntityBoundsDirty(&entity);
        }
    };

    drag("Size", pText->m_Size, 0.01f, 0.05f, 10.f);
    drag("Thickness", pText->m_Thickness, 0.01f, 0.01f, 5.f);
    drag("Letter Spacing", pText->m_LetterSpacing, 0.005f, 0.f, 100);

    ImGui::Separator();

    SFontPickerState& fonts = editor.m_Ui.m_FontPicker;

    auto rebuildFontNamePtrs = [&fonts]()
    {
        fonts.m_vFontNames.clear();
        fonts.m_vFontNames.reserve(fonts.m_vFonts.size());
        for (const auto& f : fonts.m_vFonts)
        {
            fonts.m_vFontNames.push_back(f.first.c_str());
        }
    };

    if (!fonts.m_Scanned)
    {
        fonts.m_vFonts = CFreetypeTextMesh::GetSystemFonts();
        rebuildFontNamePtrs();
        fonts.m_Scanned = true;
    }

    if (!pText->m_FontPath.empty())
    {
        bool foundPath = false;
        for (size_t i = 0; i < fonts.m_vFonts.size(); ++i)
        {
            if (fonts.m_vFonts[i].second == pText->m_FontPath)
            {
                fonts.m_SelectedIndex = static_cast<int>(i);
                foundPath = true;
                break;
            }
        }
        if (!foundPath)
        {
            const std::string customName = std::filesystem::path(pText->m_FontPath).stem().string();
            fonts.m_vFonts.emplace_back(customName.empty() ? pText->m_FontPath : customName, pText->m_FontPath);
            rebuildFontNamePtrs();
            fonts.m_SelectedIndex = static_cast<int>(fonts.m_vFonts.size()) - 1;
        }
    }

    ImGui::Text("Font");
    if (!fonts.m_vFontNames.empty())
    {
        if (ImGui::Combo("##font", &fonts.m_SelectedIndex, fonts.m_vFontNames.data(), static_cast<int>(fonts.m_vFontNames.size())))
        {
            editor.SaveState();
            pText->m_FontPath = fonts.m_vFonts[fonts.m_SelectedIndex].second;
            CModelService::UpdateModel(&entity, editor.m_Text);
            CEntityTextureService::MarkEntityBoundsDirty(&entity);
        }
    }
    else
    {
        ImGui::TextDisabled("No fonts selected");
    }

    if (!pText->m_FontPath.empty() && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", pText->m_FontPath.c_str());
    }

    ImGui::SameLine();
    if (ImGui::Button("...##Browse3dFont"))
    {
        const std::string browsedFont = BrowseForFileDialog("Font Files (*.ttf;*.otf)", "Font Files (*.ttf;*.otf)\0*.ttf;*.otf\0All Files (*.*)\0*.*\0");
        if (!browsedFont.empty())
        {
            editor.SaveState();
            pText->m_FontPath = browsedFont;
            bool existsInList = false;
            for (size_t i = 0; i < fonts.m_vFonts.size(); ++i)
            {
                if (fonts.m_vFonts[i].second == browsedFont)
                {
                    fonts.m_SelectedIndex = static_cast<int>(i);
                    existsInList = true;
                    break;
                }
            }
            if (!existsInList)
            {
                const std::string customName = std::filesystem::path(browsedFont).stem().string();
                fonts.m_vFonts.emplace_back(customName.empty() ? browsedFont : customName, browsedFont);
                rebuildFontNamePtrs();
                fonts.m_SelectedIndex = static_cast<int>(fonts.m_vFonts.size()) - 1;
            }
            CModelService::UpdateModel(&entity, editor.m_Text);
            CEntityTextureService::MarkEntityBoundsDirty(&entity);
        }
    }
}

void CComponentUIHelper::DrawScriptComponent(CEditor& editor, CEntity& entity, CScriptComponent* pScript)
{
    if (!pScript)
    {
        return;
    }

    auto& scriptEngine = quark::script::CScriptEngine::Get();
    const auto& vAvailable = scriptEngine.GetAvailableScripts();
    if (vAvailable.empty())
    {
        scriptEngine.RefreshAvailableScripts(editor.m_ProjectPath);
    }

    std::vector<std::string> vScriptNames;
    vScriptNames.push_back("None");
    int selectedIndex = 0;

    for (size_t i = 0; i < vAvailable.size(); ++i)
    {
        vScriptNames.push_back(vAvailable[i].m_ClassName);
        if (vAvailable[i].m_ClassName == pScript->m_ScriptName)
        {
            selectedIndex = static_cast<int>(i) + 1;
        }
    }

    std::vector<const char*> vScriptNamePtrs;
    vScriptNamePtrs.reserve(vScriptNames.size());
    for (const auto& name : vScriptNames)
    {
        vScriptNamePtrs.push_back(name.c_str());
    }

    ImGui::Text("Script Class");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh"))
    {
        scriptEngine.RefreshAvailableScripts(editor.m_ProjectPath);
        if (!pScript->m_ScriptName.empty())
        {
            const std::string headerPath = scriptEngine.FindScriptHeaderPath(pScript->m_ScriptName);
            if (!headerPath.empty())
            {
                quark::script::SParsedScriptInfo headerInfo;
                if (quark::script::CScriptHeaderParser::ParseHeader(headerPath, headerInfo))
                {
                    pScript->SyncWithHeaderFields(headerInfo.m_vFields);
                }
            }
        }
    }
    int comboIndex = selectedIndex;
    if (ImGui::Combo("##script_class", &comboIndex, vScriptNamePtrs.data(), static_cast<int>(vScriptNamePtrs.size())))
    {
        editor.SaveState();
        if (comboIndex == 0)
        {
            pScript->m_ScriptName.clear();
            pScript->m_ScriptPath.clear();
            pScript->m_vFields.clear();
        }
        else
        {
            const auto& selectedInfo = vAvailable[comboIndex - 1];
            pScript->m_ScriptName = selectedInfo.m_ClassName;
            pScript->m_ScriptPath = selectedInfo.m_HeaderPath;
            pScript->SyncWithHeaderFields(selectedInfo.m_vFields);
        }
        editor.m_SceneDirty = true;
    }

    if (pScript->m_ScriptName.empty())
    {
        ImGui::TextDisabled("Select a script from the dropdown above, or create one via Assets right-click menu.");
        return;
    }

    if (pScript->m_vFields.empty())
    {
        for (const auto& info : vAvailable)
        {
            if (info.m_ClassName == pScript->m_ScriptName)
            {
                if (!info.m_vFields.empty())
                {
                    pScript->SyncWithHeaderFields(info.m_vFields);
                }
                break;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Public Variables (%s)", pScript->m_ScriptName.c_str());
    ImGui::Spacing();

    if (pScript->m_vFields.empty())
    {
        ImGui::TextDisabled("No public variables found in %s", pScript->m_ScriptName.c_str());
    }

    bool valueChanged = false;
    for (size_t fIdx = 0; fIdx < pScript->m_vFields.size(); ++fIdx)
    {
        auto& field = pScript->m_vFields[fIdx];
        ImGui::PushID(static_cast<int>(fIdx));

        switch (field.m_Type)
        {
            case quark::script::EScriptFieldType::FLOAT:
            {
                float val = field.m_FloatValue;
                if (ImGui::DragFloat(field.m_Name.c_str(), &val, 0.1f))
                {
                    field.m_FloatValue = val;
                    valueChanged = true;
                }
                break;
            }
            case quark::script::EScriptFieldType::INT:
            {
                int val = field.m_IntValue;
                if (ImGui::DragInt(field.m_Name.c_str(), &val, 1))
                {
                    field.m_IntValue = val;
                    valueChanged = true;
                }
                break;
            }
            case quark::script::EScriptFieldType::BOOL:
            {
                bool val = field.m_BoolValue;
                if (ImGui::Checkbox(field.m_Name.c_str(), &val))
                {
                    field.m_BoolValue = val;
                    valueChanged = true;
                }
                break;
            }
            case quark::script::EScriptFieldType::STRING:
            {
                char aBuf[256];
                size_t copied = field.m_StringValue.copy(aBuf, sizeof(aBuf) - 1);
                aBuf[copied] = '\0';
                if (ImGui::InputText(field.m_Name.c_str(), aBuf, sizeof(aBuf)))
                {
                    field.m_StringValue = aBuf;
                    valueChanged = true;
                }
                break;
            }
            case quark::script::EScriptFieldType::VEC3:
            {
                float aVec[3] = { field.m_Vec3Value.x, field.m_Vec3Value.y, field.m_Vec3Value.z };
                if (ImGui::DragFloat3(field.m_Name.c_str(), aVec, 0.1f))
                {
                    field.m_Vec3Value = { aVec[0], aVec[1], aVec[2] };
                    valueChanged = true;
                }
                break;
            }
        }

        ImGui::PopID();
    }

    if (valueChanged)
    {
        editor.SaveState();
        editor.m_SceneDirty = true;
        if (editor.m_IsPlaying && pScript->m_pInstance)
        {
            pScript->ApplyFieldsToInstance();
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Sync Fields from Header"))
    {
        const std::string headerPath = scriptEngine.FindScriptHeaderPath(pScript->m_ScriptName);
        if (!headerPath.empty())
        {
            quark::script::SParsedScriptInfo headerInfo;
            if (quark::script::CScriptHeaderParser::ParseHeader(headerPath, headerInfo))
            {
                editor.SaveState();
                pScript->SyncWithHeaderFields(headerInfo.m_vFields);
                editor.m_SceneDirty = true;
            }
        }
    }
}

namespace
{

    bool EditGuiColor4(CEditor& editor, const char* pLabel, Color& c)
    {
        static ImGuiID s_ActiveColorEditId = 0;
        float aRgba[4] = {
            c.r / 255.0f,
            c.g / 255.0f,
            c.b / 255.0f,
            c.a / 255.0f
        };
        const ImGuiID currentId = ImGui::GetID(pLabel);
        const bool colorChanged = ImGui::ColorEdit4(pLabel, aRgba, ImGuiColorEditFlags_NoInputs);
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
            s_ActiveColorEditId = currentId;
        }
        if (colorChanged)
        {
            if (s_ActiveColorEditId != currentId)
            {
                editor.SaveState();
                s_ActiveColorEditId = currentId;
            }
            c.r = static_cast<unsigned char>(std::clamp(aRgba[0] * 255.0f, 0.0f, 255.0f));
            c.g = static_cast<unsigned char>(std::clamp(aRgba[1] * 255.0f, 0.0f, 255.0f));
            c.b = static_cast<unsigned char>(std::clamp(aRgba[2] * 255.0f, 0.0f, 255.0f));
            c.a = static_cast<unsigned char>(std::clamp(aRgba[3] * 255.0f, 0.0f, 255.0f));
            return true;
        }
        if (s_ActiveColorEditId == currentId &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemActive())
        {
            s_ActiveColorEditId = 0;
        }
        return false;
    }

} // anonymous

void CComponentUIHelper::DrawUiRectTransformComponent(
    CEditor& editor,
    CUiRectTransformComponent* pRectTransform)
{
    if (!pRectTransform)
    {
        return;
    }

    bool changed = false;

    float aPos[2] = { pRectTransform->m_AnchoredPosition.x, pRectTransform->m_AnchoredPosition.y };
    if (ImGui::DragFloat2("Pos (X, Y)", aPos, 1.0f, -8192.0f, 8192.0f, "%.1f"))
    {
        pRectTransform->m_AnchoredPosition = { aPos[0], aPos[1] };
        pRectTransform->m_Position = pRectTransform->m_AnchoredPosition;
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    float aSize[2] = { pRectTransform->m_Size.x, pRectTransform->m_Size.y };
    if (ImGui::DragFloat2("Width / Height", aSize, 1.0f, 1.0f, 8192.0f, "%.1f"))
    {
        pRectTransform->m_Size = { std::max(1.0f, aSize[0]), std::max(1.0f, aSize[1]) };
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    float aAnchorMin[2] = { pRectTransform->m_AnchorMin.x, pRectTransform->m_AnchorMin.y };
    if (ImGui::DragFloat2("Anchor Min", aAnchorMin, 0.01f, 0.0f, 1.0f, "%.2f"))
    {
        pRectTransform->m_AnchorMin = { std::clamp(aAnchorMin[0], 0.0f, 1.0f), std::clamp(aAnchorMin[1], 0.0f, 1.0f) };
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    float aAnchorMax[2] = { pRectTransform->m_AnchorMax.x, pRectTransform->m_AnchorMax.y };
    if (ImGui::DragFloat2("Anchor Max", aAnchorMax, 0.01f, 0.0f, 1.0f, "%.2f"))
    {
        pRectTransform->m_AnchorMax = { std::clamp(aAnchorMax[0], 0.0f, 1.0f), std::clamp(aAnchorMax[1], 0.0f, 1.0f) };
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    float aPivot[2] = { pRectTransform->m_Pivot.x, pRectTransform->m_Pivot.y };
    if (ImGui::DragFloat2("Pivot", aPivot, 0.01f, 0.0f, 1.0f, "%.2f"))
    {
        pRectTransform->m_Pivot = { std::clamp(aPivot[0], 0.0f, 1.0f), std::clamp(aPivot[1], 0.0f, 1.0f) };
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    float rotation = pRectTransform->m_Rotation2D;
    if (ImGui::DragFloat("Rotation", &rotation, 0.5f, -360.0f, 360.0f, "%.1f deg"))
    {
        pRectTransform->m_Rotation2D = rotation;
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    float aScale[2] = { pRectTransform->m_Scale2D.x, pRectTransform->m_Scale2D.y };
    if (ImGui::DragFloat2("Scale", aScale, 0.01f, -100.0f, 100.0f, "%.2f"))
    {
        pRectTransform->m_Scale2D = { aScale[0], aScale[1] };
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    if (changed)
    {
        editor.m_SceneDirty = true;
    }
}

void CComponentUIHelper::DrawCanvasComponent(
    CEditor& editor,
    CEntity& entity,
    CCanvasComponent* pCanvas)
{
    if (!pCanvas)
    {
        return;
    }

    bool changed = false;

    const char* apRenderModes[] = {
        "Screen Space - Overlay",
        "Screen Space - Camera",
        "World Space"
    };
    int currentMode = static_cast<int>(pCanvas->m_RenderMode);
    if (ImGui::Combo("Render Mode", &currentMode, apRenderModes, IM_ARRAYSIZE(apRenderModes)))
    {
        editor.SaveState();
        pCanvas->m_RenderMode = static_cast<ECanvasRenderMode>(currentMode);
        changed = true;
    }

    auto drawCameraCombo = [&](const char* label, int& cameraId)
    {
        std::string preview = "None (-1)";
        if (cameraId >= 0)
        {
            for (int eIdx = 0; eIdx < static_cast<int>(editor.m_Scene.m_vEntities.size()); ++eIdx)
            {
                const auto& ent = editor.m_Scene.m_vEntities[eIdx];
                if (ent.m_Id == cameraId || eIdx == cameraId)
                {
                    preview = ent.m_Name + " (" + std::to_string(ent.m_Id) + ")";
                    break;
                }
            }
        }

        if (ImGui::BeginCombo(label, preview.c_str()))
        {
            const bool selectedNone = (cameraId == -1);
            if (ImGui::Selectable("None (-1)", selectedNone))
            {
                editor.SaveState();
                cameraId = -1;
                changed = true;
            }
            for (int eIdx = 0; eIdx < static_cast<int>(editor.m_Scene.m_vEntities.size()); ++eIdx)
            {
                const auto& ent = editor.m_Scene.m_vEntities[eIdx];
                if (ent.GetCameraComponent())
                {
                    const bool isSelected = (cameraId == ent.m_Id || cameraId == eIdx);
                    const std::string name = ent.m_Name + " (" + std::to_string(ent.m_Id) + ")";
                    if (ImGui::Selectable(name.c_str(), isSelected))
                    {
                        editor.SaveState();
                        cameraId = ent.m_Id;
                        changed = true;
                    }
                }
            }
            ImGui::EndCombo();
        }
    };

    if (pCanvas->m_RenderMode == ECanvasRenderMode::WORLD_SPACE)
    {
        float aSize[2] = { pCanvas->m_Width, pCanvas->m_Height };
        if (ImGui::DragFloat2("Canvas Size", aSize, 1.0f, 10.0f, 8192.0f, "%.0f"))
        {
            pCanvas->m_Width = std::max(10.0f, aSize[0]);
            pCanvas->m_Height = std::max(10.0f, aSize[1]);
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        int eventCam = pCanvas->m_EventCameraEntityId;
        drawCameraCombo("Event Camera", eventCam);
        if (eventCam != pCanvas->m_EventCameraEntityId)
        {
            pCanvas->m_EventCameraEntityId = eventCam;
            changed = true;
        }

        bool overrideSorting = pCanvas->m_OverrideSorting;
        if (ImGui::Checkbox("Override Sorting", &overrideSorting))
        {
            editor.SaveState();
            pCanvas->m_OverrideSorting = overrideSorting;
            changed = true;
        }

        int sortOrder = pCanvas->m_SortingOrder;
        if (ImGui::InputInt("Sort Order", &sortOrder))
        {
            pCanvas->m_SortingOrder = sortOrder;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        bool pixelPerfect = pCanvas->m_PixelPerfect;
        if (ImGui::Checkbox("Pixel Perfect", &pixelPerfect))
        {
            editor.SaveState();
            pCanvas->m_PixelPerfect = pixelPerfect;
            changed = true;
        }
    }
    else if (pCanvas->m_RenderMode == ECanvasRenderMode::SCREEN_SPACE_CAMERA)
    {
        bool pixelPerfect = pCanvas->m_PixelPerfect;
        if (ImGui::Checkbox("Pixel Perfect", &pixelPerfect))
        {
            editor.SaveState();
            pCanvas->m_PixelPerfect = pixelPerfect;
            changed = true;
        }

        int renderCam = pCanvas->m_RenderCameraEntityId;
        drawCameraCombo("Render Camera", renderCam);
        if (renderCam != pCanvas->m_RenderCameraEntityId)
        {
            pCanvas->m_RenderCameraEntityId = renderCam;
            changed = true;
        }

        float planeDist = pCanvas->m_PlaneDistance;
        if (ImGui::DragFloat("Plane Distance", &planeDist, 1.0f, 0.1f, 10000.0f, "%.1f"))
        {
            pCanvas->m_PlaneDistance = planeDist;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        int sortOrder = pCanvas->m_SortingOrder;
        if (ImGui::InputInt("Sort Order", &sortOrder))
        {
            pCanvas->m_SortingOrder = sortOrder;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    }
    else
    {
        bool pixelPerfect = pCanvas->m_PixelPerfect;
        if (ImGui::Checkbox("Pixel Perfect", &pixelPerfect))
        {
            editor.SaveState();
            pCanvas->m_PixelPerfect = pixelPerfect;
            changed = true;
        }

        int sortOrder = pCanvas->m_SortingOrder;
        if (ImGui::InputInt("Sort Order", &sortOrder))
        {
            pCanvas->m_SortingOrder = sortOrder;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        int targetDisplay = pCanvas->m_TargetDisplay;
        if (ImGui::InputInt("Target Display", &targetDisplay))
        {
            pCanvas->m_TargetDisplay = targetDisplay;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        float scaleFactor = pCanvas->m_ScaleFactor;
        if (ImGui::DragFloat("Scale Factor", &scaleFactor, 0.05f, 0.1f, 10.0f, "%.2f"))
        {
            pCanvas->m_ScaleFactor = scaleFactor;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        float aRefRes[2] = { pCanvas->m_ReferenceResolution.x, pCanvas->m_ReferenceResolution.y };
        if (ImGui::DragFloat2("Reference Resolution", aRefRes, 1.0f, 100.0f, 8192.0f, "%.0f"))
        {
            pCanvas->m_ReferenceResolution = { aRefRes[0], aRefRes[1] };
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    }

    if (changed)
    {
        editor.m_SceneDirty = true;
    }
}

void CComponentUIHelper::DrawImageComponent(
    CEditor& editor,
    CEntity& entity,
    CImageComponent* pImage)
{
    if (!pImage)
    {
        return;
    }

    bool changed = false;

    char aTexturePath[512] = {};
    std::snprintf(aTexturePath, sizeof(aTexturePath), "%s", pImage->m_SourceTexture.c_str());
    if (ImGui::InputText("Source Image", aTexturePath, sizeof(aTexturePath)))
    {
        pImage->m_SourceTexture = aTexturePath;
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }
    ImGui::SameLine();
    if (ImGui::Button("...##BrowseImageTexture"))
    {
        const std::string browsed = BrowseForFileDialog("Image Files (*.png;*.jpg;*.jpeg;*.bmp)", "Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0");
        if (!browsed.empty())
        {
            editor.SaveState();
            pImage->m_SourceTexture = browsed;
            changed = true;
        }
    }

    if (EditGuiColor4(editor, "Color##Img", pImage->m_Color))
    {
        changed = true;
    }

    char aMaterialPath[512] = {};
    std::snprintf(aMaterialPath, sizeof(aMaterialPath), "%s", pImage->m_Material.c_str());
    if (ImGui::InputText("Material", aMaterialPath, sizeof(aMaterialPath)))
    {
        pImage->m_Material = aMaterialPath;
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }
    ImGui::SameLine();
    if (ImGui::Button("...##BrowseImageMaterial"))
    {
        const std::string browsed = BrowseForFileDialog("Material Files (*.mtl)", "Material Files (*.mtl)\0*.mtl\0All Files (*.*)\0*.*\0");
        if (!browsed.empty())
        {
            editor.SaveState();
            pImage->m_Material = browsed;
            changed = true;
        }
    }

    bool raycastTarget = pImage->m_RaycastTarget;
    if (ImGui::Checkbox("Raycast Target##Img", &raycastTarget))
    {
        editor.SaveState();
        pImage->m_RaycastTarget = raycastTarget;
        changed = true;
    }

    const char* apImageTypes[] = { "Simple", "Sliced", "Tiled", "Filled" };
    int currentType = static_cast<int>(pImage->m_ImageType);
    if (ImGui::Combo("Image Type", &currentType, apImageTypes, IM_ARRAYSIZE(apImageTypes)))
    {
        editor.SaveState();
        pImage->m_ImageType = static_cast<EUiImageType>(currentType);
        changed = true;
    }

    bool preserveAspect = pImage->m_PreserveAspect;
    if (ImGui::Checkbox("Preserve Aspect", &preserveAspect))
    {
        editor.SaveState();
        pImage->m_PreserveAspect = preserveAspect;
        changed = true;
    }

    if (pImage->m_ImageType == EUiImageType::SLICED || pImage->m_ImageType == EUiImageType::TILED)
    {
        bool fillCenter = pImage->m_FillCenter;
        if (ImGui::Checkbox("Fill Center", &fillCenter))
        {
            editor.SaveState();
            pImage->m_FillCenter = fillCenter;
            changed = true;
        }
    }
    else if (pImage->m_ImageType == EUiImageType::FILLED)
    {
        float fillAmount = pImage->m_FillAmount;
        if (ImGui::SliderFloat("Fill Amount", &fillAmount, 0.0f, 1.0f, "%.2f"))
        {
            pImage->m_FillAmount = fillAmount;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    }

    if (changed)
    {
        editor.m_SceneDirty = true;
    }
}

void CComponentUIHelper::DrawPanelComponent(
    CEditor& editor,
    CEntity& entity,
    CPanelComponent* pPanel)
{
    if (!pPanel)
    {
        return;
    }

    bool changed = false;

    char aImagePath[512] = {};
    std::snprintf(aImagePath, sizeof(aImagePath), "%s", pPanel->m_BackgroundImage.c_str());
    if (ImGui::InputText("Background Image", aImagePath, sizeof(aImagePath)))
    {
        pPanel->m_BackgroundImage = aImagePath;
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }
    ImGui::SameLine();
    if (ImGui::Button("...##BrowsePanelImage"))
    {
        const std::string browsed = BrowseForFileDialog("Image Files (*.png;*.jpg;*.jpeg;*.bmp)", "Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0");
        if (!browsed.empty())
        {
            editor.SaveState();
            pPanel->m_BackgroundImage = browsed;
            changed = true;
        }
    }

    if (EditGuiColor4(editor, "Background Color##Pnl", pPanel->m_BackgroundColor))
    {
        changed = true;
    }

    bool raycastTarget = pPanel->m_RaycastTarget;
    if (ImGui::Checkbox("Raycast Target##Pnl", &raycastTarget))
    {
        editor.SaveState();
        pPanel->m_RaycastTarget = raycastTarget;
        changed = true;
    }

    if (changed)
    {
        editor.m_SceneDirty = true;
    }
}

void CComponentUIHelper::DrawButtonComponent(
    CEditor& editor,
    CEntity& entity,
    CButtonComponent* pButton)
{
    if (!pButton)
    {
        return;
    }

    bool changed = false;

    bool interactable = pButton->m_Interactable;
    if (ImGui::Checkbox("Interactable", &interactable))
    {
        editor.SaveState();
        pButton->m_Interactable = interactable;
        changed = true;
    }

    const char* apTransitions[] = { "None", "Color Tint", "Sprite Swap", "Animation" };
    int currentTransition = static_cast<int>(pButton->m_Transition);
    if (ImGui::Combo("Transition", &currentTransition, apTransitions, IM_ARRAYSIZE(apTransitions)))
    {
        editor.SaveState();
        pButton->m_Transition = static_cast<EUiTransition>(currentTransition);
        changed = true;
    }

    if (pButton->m_Transition == EUiTransition::COLOR_TINT)
    {
        if (EditGuiColor4(editor, "Normal Color##Btn", pButton->m_NormalColor))
        {
            changed = true;
        }
        if (EditGuiColor4(editor, "Highlighted Color##Btn", pButton->m_HighlightedColor))
        {
            changed = true;
        }
        if (EditGuiColor4(editor, "Pressed Color##Btn", pButton->m_PressedColor))
        {
            changed = true;
        }
        if (EditGuiColor4(editor, "Selected Color##Btn", pButton->m_SelectedColor))
        {
            changed = true;
        }
        if (EditGuiColor4(editor, "Disabled Color##Btn", pButton->m_DisabledColor))
        {
            changed = true;
        }

        float colorMultiplier = pButton->m_ColorMultiplier;
        if (ImGui::SliderFloat("Color Multiplier", &colorMultiplier, 1.0f, 5.0f, "%.1f"))
        {
            pButton->m_ColorMultiplier = colorMultiplier;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        float fadeDuration = pButton->m_FadeDuration;
        if (ImGui::DragFloat("Fade Duration", &fadeDuration, 0.01f, 0.0f, 5.0f, "%.2f s"))
        {
            pButton->m_FadeDuration = fadeDuration;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    }
    else if (pButton->m_Transition == EUiTransition::SPRITE_SWAP)
    {
        char aHighlighted[512] = {};
        std::snprintf(aHighlighted, sizeof(aHighlighted), "%s", pButton->m_SpriteSwapHighlighted.c_str());
        if (ImGui::InputText("Highlighted Sprite", aHighlighted, sizeof(aHighlighted)))
        {
            pButton->m_SpriteSwapHighlighted = aHighlighted;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }

        char aPressed[512] = {};
        std::snprintf(aPressed, sizeof(aPressed), "%s", pButton->m_SpriteSwapPressed.c_str());
        if (ImGui::InputText("Pressed Sprite", aPressed, sizeof(aPressed)))
        {
            pButton->m_SpriteSwapPressed = aPressed;
            changed = true;
        }
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
    }

    const char* apNavigations[] = { "None", "Horizontal", "Vertical", "Automatic", "Explicit" };
    int currentNav = static_cast<int>(pButton->m_Navigation);
    if (ImGui::Combo("Navigation", &currentNav, apNavigations, IM_ARRAYSIZE(apNavigations)))
    {
        editor.SaveState();
        pButton->m_Navigation = static_cast<EUiNavigation>(currentNav);
        changed = true;
    }

    char aAction[512] = {};
    std::snprintf(aAction, sizeof(aAction), "%s", pButton->m_OnClickAction.c_str());
    if (ImGui::InputText("On Click Action", aAction, sizeof(aAction)))
    {
        pButton->m_OnClickAction = aAction;
        changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }

    bool raycastTarget = pButton->m_RaycastTarget;
    if (ImGui::Checkbox("Raycast Target##Btn", &raycastTarget))
    {
        editor.SaveState();
        pButton->m_RaycastTarget = raycastTarget;
        changed = true;
    }

    if (changed)
    {
        editor.m_SceneDirty = true;
    }
}

void CComponentUIHelper::DrawUiTextComponent(CEditor& editor, CTextComponent* pText)
{
    if (!pText)
    {
        return;
    }

    bool changed = false;
    char aText[4096] = {};
    std::snprintf(aText, sizeof(aText), "%s", pText->m_Text.c_str());
    const bool textChanged = ImGui::InputTextMultiline("##Text", aText, sizeof(aText), ImVec2(-1.0f, 82.0f));
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }
    if (textChanged)
    {
        pText->m_Text = aText;
        changed = true;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Character");
    SFontPickerState& fonts = editor.m_Ui.m_FontPicker;
    auto rebuildFontNamePtrs = [&fonts]()
    {
        fonts.m_vFontNames.clear();
        fonts.m_vFontNames.reserve(fonts.m_vFonts.size());
        for (const auto& fontEntry : fonts.m_vFonts)
        {
            fonts.m_vFontNames.push_back(fontEntry.first.c_str());
        }
    };

    if (!fonts.m_Scanned)
    {
        fonts.m_vFonts = CFreetypeTextMesh::GetSystemFonts();
        rebuildFontNamePtrs();
        fonts.m_Scanned = true;
    }

    auto equalsIgnoreCase = [](const std::string& a, const std::string& b) -> bool
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            {
                return false;
            }
        }
        return true;
    };

    if (!pText->m_Font.empty() && !equalsIgnoreCase(pText->m_Font, "Default (Rubik)"))
    {
        const std::string fontStem = std::filesystem::path(pText->m_Font).stem().string();
        bool foundInFonts = false;
        for (size_t i = 0; i < fonts.m_vFonts.size(); ++i)
        {
            if (fonts.m_vFonts[i].second == pText->m_Font ||
                equalsIgnoreCase(fonts.m_vFonts[i].first, pText->m_Font) ||
                (!fontStem.empty() && equalsIgnoreCase(fonts.m_vFonts[i].first, fontStem)))
            {
                foundInFonts = true;
                break;
            }
        }
        if (!foundInFonts)
        {
            fonts.m_vFonts.emplace_back(fontStem.empty() ? pText->m_Font : fontStem, pText->m_Font);
            rebuildFontNamePtrs();
        }
    }

    std::vector<const char*> vUiFontNames;
    vUiFontNames.reserve(fonts.m_vFonts.size() + 1);
    vUiFontNames.push_back("Default (Rubik)");
    for (const auto& fontEntry : fonts.m_vFonts)
    {
        vUiFontNames.push_back(fontEntry.first.c_str());
    }

    int selectedFontIdx = 0;
    if (!pText->m_Font.empty() && !equalsIgnoreCase(pText->m_Font, "Default (Rubik)"))
    {
        const std::string fontStem = std::filesystem::path(pText->m_Font).stem().string();
        for (size_t i = 0; i < fonts.m_vFonts.size(); ++i)
        {
            if (fonts.m_vFonts[i].second == pText->m_Font ||
                equalsIgnoreCase(fonts.m_vFonts[i].first, pText->m_Font) ||
                (!fontStem.empty() && equalsIgnoreCase(fonts.m_vFonts[i].first, fontStem)))
            {
                selectedFontIdx = static_cast<int>(i) + 1;
                break;
            }
        }
    }

    if (ImGui::Combo("Font", &selectedFontIdx, vUiFontNames.data(), static_cast<int>(vUiFontNames.size())))
    {
        editor.SaveState();
        if (selectedFontIdx <= 0 || selectedFontIdx > static_cast<int>(fonts.m_vFonts.size()))
        {
            pText->m_Font = "Default (Rubik)";
        }
        else
        {
            pText->m_Font = fonts.m_vFonts[static_cast<size_t>(selectedFontIdx - 1)].second;
        }
        changed = true;
    }
    if (!pText->m_Font.empty() && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", pText->m_Font.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("...##BrowseUiFont"))
    {
        const std::string browsedFont = BrowseForFileDialog("Font Files (*.ttf;*.otf)", "Font Files (*.ttf;*.otf)\0*.ttf;*.otf\0All Files (*.*)\0*.*\0");
        if (!browsedFont.empty())
        {
            editor.SaveState();
            pText->m_Font = browsedFont;
            const std::string fontStem = std::filesystem::path(browsedFont).stem().string();
            bool existsInFonts = false;
            for (const auto& fontEntry : fonts.m_vFonts)
            {
                if (fontEntry.second == browsedFont)
                {
                    existsInFonts = true;
                    break;
                }
            }
            if (!existsInFonts)
            {
                fonts.m_vFonts.emplace_back(fontStem.empty() ? browsedFont : fontStem, browsedFont);
                rebuildFontNamePtrs();
            }
            changed = true;
        }
    }

    const char* apFontStyles[] = { "Normal", "Bold", "Italic", "Bold Italic" };
    int fontStyle = static_cast<int>(pText->m_FontStyle);
    if (ImGui::Combo("Font Style", &fontStyle, apFontStyles, IM_ARRAYSIZE(apFontStyles)))
    {
        editor.SaveState();
        pText->m_FontStyle = static_cast<EUiTextFontStyle>(fontStyle);
        changed = true;
    }

    float fontSize = pText->m_FontSize;
    const bool fontSizeChanged = ImGui::InputFloat("Font Size", &fontSize, 1.0f, 5.0f, "%.0f");
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }
    if (fontSizeChanged)
    {
        pText->m_FontSize = std::clamp(fontSize, 1.0f, 256.0f);
        changed = true;
    }

    float lineSpacing = pText->m_LineSpacing;
    const bool lineSpacingChanged = ImGui::InputFloat("Line Spacing", &lineSpacing, 0.1f, 1.0f, "%.2f");
    if (ImGui::IsItemActivated())
    {
        editor.SaveState();
    }
    if (lineSpacingChanged)
    {
        pText->m_LineSpacing = std::clamp(lineSpacing, 0.1f, 10.0f);
        changed = true;
    }

    bool richText = pText->m_RichText;
    if (ImGui::Checkbox("Rich Text", &richText))
    {
        editor.SaveState();
        pText->m_RichText = richText;
        changed = true;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Paragraph");
    const char* apHorizontalAlignment[] = { "Left", "Center", "Right" };
    int horizontal = static_cast<int>(pText->m_HorizontalAlignment);
    if (ImGui::Combo("Horizontal Alignment", &horizontal, apHorizontalAlignment, IM_ARRAYSIZE(apHorizontalAlignment)))
    {
        editor.SaveState();
        pText->m_HorizontalAlignment = static_cast<EUiTextAlignment>(horizontal);
        changed = true;
    }

    const char* apVerticalAlignment[] = { "Top", "Middle", "Bottom" };
    int vertical = static_cast<int>(pText->m_VerticalAlignment);
    if (ImGui::Combo("Vertical Alignment", &vertical, apVerticalAlignment, IM_ARRAYSIZE(apVerticalAlignment)))
    {
        editor.SaveState();
        pText->m_VerticalAlignment = static_cast<EUiTextVerticalAlignment>(vertical);
        changed = true;
    }

    const char* apOverflowOptions[] = { "Wrap", "Truncate" };
    int horizontalOverflow = static_cast<int>(pText->m_HorizontalOverflow);
    if (ImGui::Combo("Horizontal Overflow", &horizontalOverflow, apOverflowOptions, IM_ARRAYSIZE(apOverflowOptions)))
    {
        editor.SaveState();
        pText->m_HorizontalOverflow = static_cast<EUiTextOverflow>(horizontalOverflow);
        changed = true;
    }

    int verticalOverflow = static_cast<int>(pText->m_VerticalOverflow);
    if (ImGui::Combo("Vertical Overflow", &verticalOverflow, apOverflowOptions, IM_ARRAYSIZE(apOverflowOptions)))
    {
        editor.SaveState();
        pText->m_VerticalOverflow = static_cast<EUiTextOverflow>(verticalOverflow);
        changed = true;
    }

    bool bestFit = pText->m_BestFit;
    if (ImGui::Checkbox("Best Fit", &bestFit))
    {
        editor.SaveState();
        pText->m_BestFit = bestFit;
        changed = true;
    }
    changed |= EditGuiColor4(editor, "Color", pText->m_Color);

    SMaterialPickerState& matPicker = editor.m_Ui.m_MaterialPicker;
    if (matPicker.m_NeedsUpdate)
    {
        matPicker.m_vMaterialPaths = GetAllMaterialsInProject();
        matPicker.m_vMaterialPaths.insert(matPicker.m_vMaterialPaths.begin(), "None (Material)");
        matPicker.m_vDisplayNames.clear();
        matPicker.m_vMaterialNames.clear();

        for (const auto& matPath : matPicker.m_vMaterialPaths)
        {
            matPicker.m_vDisplayNames.push_back(
                (matPath == "None" || matPath == "None (Material)")
                    ? "None (Material)"
                    : std::filesystem::path(matPath).filename().generic_string());
        }
        for (const auto& displayName : matPicker.m_vDisplayNames)
        {
            matPicker.m_vMaterialNames.push_back(displayName.c_str());
        }
        matPicker.m_NeedsUpdate = false;
    }

    int selectedMaterialIdx = 0;
    if (!pText->m_Material.empty() &&
        pText->m_Material != "None" &&
        pText->m_Material != "None (Material)")
    {
        const std::string matFilename = std::filesystem::path(pText->m_Material).filename().generic_string();
        bool foundInMaterials = false;
        for (size_t i = 1; i < matPicker.m_vMaterialPaths.size(); ++i)
        {
            if (matPicker.m_vMaterialPaths[i] == pText->m_Material ||
                std::filesystem::path(matPicker.m_vMaterialPaths[i]).filename().generic_string() == matFilename)
            {
                selectedMaterialIdx = static_cast<int>(i);
                foundInMaterials = true;
                break;
            }
        }
        if (!foundInMaterials)
        {
            matPicker.m_vMaterialPaths.push_back(pText->m_Material);
            matPicker.m_vDisplayNames.push_back(matFilename.empty() ? pText->m_Material : matFilename);
            matPicker.m_vMaterialNames.clear();
            for (const auto& displayName : matPicker.m_vDisplayNames)
            {
                matPicker.m_vMaterialNames.push_back(displayName.c_str());
            }
            selectedMaterialIdx = static_cast<int>(matPicker.m_vMaterialPaths.size()) - 1;
        }
    }

    if (!matPicker.m_vMaterialNames.empty())
    {
        if (ImGui::Combo("Material", &selectedMaterialIdx, matPicker.m_vMaterialNames.data(),
                         static_cast<int>(matPicker.m_vMaterialNames.size())))
        {
            editor.SaveState();
            if (selectedMaterialIdx <= 0 || selectedMaterialIdx >= static_cast<int>(matPicker.m_vMaterialPaths.size()))
            {
                pText->m_Material = "None (Material)";
            }
            else
            {
                pText->m_Material = matPicker.m_vMaterialPaths[static_cast<size_t>(selectedMaterialIdx)];
            }
            changed = true;
        }
        if (selectedMaterialIdx > 0 &&
            selectedMaterialIdx < static_cast<int>(matPicker.m_vMaterialPaths.size()) &&
            ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", matPicker.m_vMaterialPaths[static_cast<size_t>(selectedMaterialIdx)].c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("...##BrowseUiMaterial"))
        {
            const std::string browsedMaterial = BrowseForFileDialog("Material Files (*.mtl)", "Material Files (*.mtl)\0*.mtl\0All Files (*.*)\0*.*\0");
            if (!browsedMaterial.empty())
            {
                editor.SaveState();
                pText->m_Material = browsedMaterial;
                matPicker.m_NeedsUpdate = true;
                changed = true;
            }
        }
    }

    if (changed)
    {
        editor.m_SceneDirty = true;
    }
}

void CComponentUIHelper::DrawUiComponent(CEditor& editor, CEntity& entity, CUiComponent* pUi)
{
    if (!pUi)
    {
        return;
    }

    bool changed = false;

    const char* apWidgetTypes[] = { "Button", "Inputfield", "Combo" };
    int currentType = static_cast<int>(pUi->m_WidgetType);
    if (ImGui::Combo("Widget Type", &currentType, apWidgetTypes, IM_ARRAYSIZE(apWidgetTypes)))
    {
        editor.SaveState();
        pUi->m_WidgetType = static_cast<EGuiWidgetType>(currentType);
        auto pComponents = entity.GetComponents();
        auto pText = pComponents->GetComponentOfType<CTextComponent>();
        auto pRectTransform = pComponents->GetComponentOfType<CUiRectTransformComponent>();
        if (pUi->m_WidgetType == EGuiWidgetType::BUTTON)
        {
            if (pText)
            {
                pText->m_Text = "Button";
                pText->m_HorizontalAlignment = EUiTextAlignment::CENTER;
            }
            if (pRectTransform)
            {
                pRectTransform->m_Size = { 150.0f, 30.0f };
            }
        }
        else if (pUi->m_WidgetType == EGuiWidgetType::INPUT_FIELD)
        {
            pUi->m_Placeholder = "InputField";
            if (pText)
            {
                pText->m_Text.clear();
                pText->m_HorizontalAlignment = EUiTextAlignment::LEFT;
            }
            if (pRectTransform)
            {
                pRectTransform->m_Size = { 260.0f, 30.0f };
            }
        }
        else if (pUi->m_WidgetType == EGuiWidgetType::COMBO)
        {
            pUi->m_vItems = { "Combo" };
            pUi->m_SelectedIndex = 0;
            if (pText)
            {
                pText->m_Text = "Combo";
                pText->m_HorizontalAlignment = EUiTextAlignment::LEFT;
            }
            if (pRectTransform)
            {
                pRectTransform->m_Size = { 260.0f, 30.0f };
            }
        }
        changed = true;
    }

    if (pUi->m_WidgetType == EGuiWidgetType::BUTTON)
    {
        ImGui::Spacing();
        ImGui::Separator();
        bool interactable = pUi->m_Interactable;
        if (ImGui::Checkbox("Interactable", &interactable))
        {
            editor.SaveState();
            pUi->m_Interactable = interactable;
            changed = true;
        }

        const char* apTransitionOptions[] = { "None", "Color Tint" };
        int transition = static_cast<int>(pUi->m_Transition);
        if (ImGui::Combo("Transition", &transition, apTransitionOptions, IM_ARRAYSIZE(apTransitionOptions)))
        {
            editor.SaveState();
            pUi->m_Transition = static_cast<EGuiButtonTransition>(transition);
            changed = true;
        }

        const char* apTargetGraphics[] = { "Button Background" };
        int targetGraphic = 0;
        ImGui::BeginDisabled();
        ImGui::Combo("Target Graphic", &targetGraphic, apTargetGraphics, IM_ARRAYSIZE(apTargetGraphics));
        ImGui::EndDisabled();

        ImGui::BeginDisabled(pUi->m_Transition == EGuiButtonTransition::NONE);
        changed |= EditGuiColor4(editor, "Normal Color", pUi->m_NormalColor);
        changed |= EditGuiColor4(editor, "Highlighted Color", pUi->m_HighlightedColor);
        changed |= EditGuiColor4(editor, "Pressed Color", pUi->m_PressedColor);
        changed |= EditGuiColor4(editor, "Selected Color", pUi->m_SelectedColor);
        changed |= EditGuiColor4(editor, "Disabled Color", pUi->m_DisabledColor);

        float colorMultiplier = pUi->m_ColorMultiplier;
        const bool colorMultChanged = ImGui::SliderFloat("Color Multiplier", &colorMultiplier, 0.0f, 2.0f, "%.2f");
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
        if (colorMultChanged)
        {
            pUi->m_ColorMultiplier = colorMultiplier;
            changed = true;
        }

        float fadeDuration = pUi->m_FadeDuration;
        const bool fadeDurationChanged = ImGui::SliderFloat("Fade Duration", &fadeDuration, 0.0f, 1.0f, "%.2f s");
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
        if (fadeDurationChanged)
        {
            pUi->m_FadeDuration = fadeDuration;
            changed = true;
        }
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        const char* apNavigationOptions[] = { "Automatic", "None" };
        int navigation = static_cast<int>(pUi->m_Navigation);
        if (ImGui::Combo("Navigation", &navigation, apNavigationOptions, IM_ARRAYSIZE(apNavigationOptions)))
        {
            editor.SaveState();
            pUi->m_Navigation = static_cast<EGuiButtonNavigation>(navigation);
            changed = true;
        }
        if (ImGui::Button("Visualize", ImVec2(-1.0f, 0.0f)))
        {
            editor.SaveState();
            pUi->m_ShowNavigationVisualization = !pUi->m_ShowNavigationVisualization;
            changed = true;
        }
    }

    if (pUi->m_WidgetType == EGuiWidgetType::INPUT_FIELD)
    {
        char aPhBuf[256] = {};
        std::snprintf(aPhBuf, sizeof(aPhBuf), "%s", pUi->m_Placeholder.c_str());
        const bool phChanged = ImGui::InputText("Placeholder", aPhBuf, sizeof(aPhBuf));
        if (ImGui::IsItemActivated())
        {
            editor.SaveState();
        }
        if (phChanged)
        {
            pUi->m_Placeholder = aPhBuf;
            changed = true;
        }
    }
    else if (pUi->m_WidgetType == EGuiWidgetType::COMBO)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Combo Items");
        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(pUi->m_vItems.size()); ++i)
        {
            ImGui::PushID(i);
            char aItemBuf[256] = {};
            std::snprintf(aItemBuf, sizeof(aItemBuf), "%s",
                          pUi->m_vItems[static_cast<size_t>(i)].c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36.0f);
            const bool itemChanged = ImGui::InputText("##item", aItemBuf, sizeof(aItemBuf));
            if (ImGui::IsItemActivated())
            {
                editor.SaveState();
            }
            if (itemChanged)
            {
                pUi->m_vItems[static_cast<size_t>(i)] = aItemBuf;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(28.0f, 0.0f)))
            {
                removeIdx = i;
            }
            ImGui::PopID();
        }
        if (removeIdx >= 0 && removeIdx < static_cast<int>(pUi->m_vItems.size()))
        {
            editor.SaveState();
            pUi->m_vItems.erase(pUi->m_vItems.begin() + removeIdx);
            if (pUi->m_SelectedIndex >= static_cast<int>(pUi->m_vItems.size()))
            {
                pUi->m_SelectedIndex = std::max(0, static_cast<int>(pUi->m_vItems.size()) - 1);
            }
            changed = true;
        }
        if (ImGui::Button("+ Add Item", ImVec2(-1.0f, 0.0f)))
        {
            editor.SaveState();
            pUi->m_vItems.push_back("New Item " + std::to_string(pUi->m_vItems.size() + 1));
            changed = true;
        }
        if (!pUi->m_vItems.empty())
        {
            int sel = std::clamp(pUi->m_SelectedIndex, 0, static_cast<int>(pUi->m_vItems.size()) - 1);
            const bool selChanged = ImGui::SliderInt("Selected Index", &sel, 0, static_cast<int>(pUi->m_vItems.size()) - 1);
            if (ImGui::IsItemActivated())
            {
                editor.SaveState();
            }
            if (selChanged)
            {
                pUi->m_SelectedIndex = sel;
                pUi->m_RuntimeDesc.m_SelectedIndex = sel;
                changed = true;
            }
        }
    }

    if (pUi->m_WidgetType == EGuiWidgetType::BUTTON)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Clicks in Play Mode: %d", pUi->m_RuntimeDesc.m_ClickCount);
    }

    if (changed)
    {
        pUi->m_Name = pUi->GetTypeName();
        auto pComponents = entity.GetComponents();
        pUi->SyncToRuntimeDesc(
            pComponents->GetComponentOfType<CUiRectTransformComponent>().get(),
            pComponents->GetComponentOfType<CTextComponent>().get());
        editor.m_SceneDirty = true;
    }
}
