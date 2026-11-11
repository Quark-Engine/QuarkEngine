#include "tex.h"
#include "editor/editor_components_ui.h"
#include "scripting/script_component.h"
#include "scripting/script_engine.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
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
#define lang CLanguageManager::Get()

void CComponentUIHelper::DrawEntityInspector(CEditor& editor, CEntity& entity, Shader shader)
{
    ImGui::Spacing();

    if (entity.GetComponents())
    {
        auto pComponentsManager = entity.GetComponents();
        size_t componentCount = pComponentsManager->GetComponentCount();
        editor.m_Ui.m_Inspector.ComponentToRemove = -1;

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
                ImGui::Spacing();

                bool isEnabled = pComp->m_Enabled;
                if (ImGui::Checkbox(lang.Word("enabled"), &isEnabled))
                {
                    editor.SaveState();
                    pComp->m_Enabled = isEnabled;
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
                else if (auto pScript = std::dynamic_pointer_cast<CScriptComponent>(pComp))
                {
                    DrawScriptComponent(editor, entity, pScript.get());
                }
                else if (auto pUi = std::dynamic_pointer_cast<CUiComponent>(pComp))
                {
                    DrawUiComponent(editor, entity, pUi.get());
                }

                ImGui::Spacing();

                bool canRemove = (pComp->GetType() != COMPONENT_TRANSFORM && pComp->GetType() != COMPONENT_MESH);
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
                        editor.m_Ui.m_Inspector.ComponentToRemove = static_cast<int>(i);
                    }
                    ImGui::PopStyleColor(2);
                }

                if (!canRemove)
                {
                    ImGui::EndDisabled();
                }
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
                    pComponentsManager->AddComponent(pUi);
                    pComponentsManager->AddComponent(pRectTransform);
                    pComponentsManager->AddComponent(pText);
                }
            }
            ImGui::EndPopup();
        }

    if (editor.m_Ui.m_Inspector.ComponentToRemove != -1)
    {
        const int componentToRemove = editor.m_Ui.m_Inspector.ComponentToRemove;
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
        editor.m_Ui.m_Inspector.ComponentToRemove = -1;
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

    float aPosition[3] = {pTransform->m_Position.x, pTransform->m_Position.y, pTransform->m_Position.z};
    float aRotation[3] = {pTransform->m_Rotation.x, pTransform->m_Rotation.y, pTransform->m_Rotation.z};
    float aScale[3] = {pTransform->m_Scale.x, pTransform->m_Scale.y, pTransform->m_Scale.z};

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
                    m.vertices[i*3+0],
                    m.vertices[i*3+1],
                    m.vertices[i*3+2]
                };

                vRemap[i] = (int)pMesh->m_EditableMesh.m_vVertices.size();
                SEditableVertex ev;
                ev.Position = pos;
                if (m.texcoords)
                {
                    ev.U = m.texcoords[i * 2 + 0];
                    ev.V = m.texcoords[i * 2 + 1];
                }
                pMesh->m_EditableMesh.m_vVertices.push_back(ev);
            }

            for (int t = 0; t < m.triangleCount; t++)
            {
                int ia, ib, ic;
                if (m.indices)
                {
                    ia = m.indices[t*3+0];
                    ib = m.indices[t*3+1];
                    ic = m.indices[t*3+2];
                }
                else
                {
                    ia = t*3+0;
                    ib = t*3+1;
                    ic = t*3+2;
                }

                if (ia >= m.vertexCount || ib >= m.vertexCount || ic >= m.vertexCount)
                {
                    continue;
                }

                SEditableTriangle tri;
                tri.A = vRemap[ia];
                tri.B = vRemap[ib];
                tri.C = vRemap[ic];

                if (tri.A == tri.B || tri.B == tri.C || tri.A == tri.C)
                {
                    continue;
                }

                pMesh->m_EditableMesh.m_vTriangles.push_back(tri);
            }

            if (edit.vSelectedVertices.empty() && !pMesh->m_EditableMesh.m_vVertices.empty())
            {
                edit.vSelectedVertices.push_back(0);
            }
        }
    }

    if (pMesh->m_VertexGizmo)
    {
        ImGui::Text(lang.Word("vertices"), (int)pMesh->m_EditableMesh.m_vVertices.size());
        ImGui::Text(lang.Word("triangles"), (int)pMesh->m_EditableMesh.m_vTriangles.size());

        if (!edit.vSelectedVertices.empty())
        {
            SEditableVertex& selectedVertex = pMesh->m_EditableMesh.m_vVertices[edit.vSelectedVertices[0]];
            float aVertexPos[3] = { selectedVertex.Position.x, selectedVertex.Position.y, selectedVertex.Position.z };

            ImGui::Separator();

            if (ImGui::DragFloat3("##vertex_position", aVertexPos, 0.1f))
            {
                editor.SaveState();

                selectedVertex.Position = Vec3(aVertexPos[0],
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

    if (picker.NeedsUpdate)
    {
        picker.vMaterialPaths = GetAllMaterialsInProject();
        picker.vMaterialPaths.insert(picker.vMaterialPaths.begin(), "None");
        picker.vDisplayNames.clear();
        picker.vMaterialNames.clear();

        for (const auto& matPath : picker.vMaterialPaths)
        {
            picker.vDisplayNames.push_back(
                matPath == "None" ? "None" : std::filesystem::path(matPath).filename().generic_string()
            );
        }
        for (const auto& displayName : picker.vDisplayNames)
        {
            picker.vMaterialNames.push_back(displayName.c_str());
        }

        picker.NeedsUpdate = false;
    }

    if (pMesh->m_Model.materialCount > 0)
    {
        pMat->m_vMaterialSlotSources.resize(pMesh->m_Model.materialCount);
        ImGui::Separator();
        ImGui::Text("Material assignment by slot");
        for (int slot = 0; slot < pMesh->m_Model.materialCount; ++slot)
        {
            int slotMaterialIndex = 0;
            for (int i = 1; i < static_cast<int>(picker.vMaterialPaths.size()); ++i)
            {
                if (picker.vMaterialPaths[i] == pMat->m_vMaterialSlotSources[slot])
                {
                    slotMaterialIndex = i;
                    break;
                }
            }

            ImGui::PushID(slot);
            ImGui::Text("Slot %d", slot);
            ImGui::SameLine();
            if (ImGui::Combo("##slot_material", &slotMaterialIndex, picker.vMaterialNames.data(),
                             static_cast<int>(picker.vMaterialNames.size())))
                             {
                editor.SaveState();
                if (slotMaterialIndex == 0)
                {
                    if (pMesh->m_Model.materials && pMesh->m_Model.materials[slot].maps)
                    {
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_ALBEDO].texture = {0};
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_NORMAL].texture = {0};
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_ROUGHNESS].texture = {0};
                        pMesh->m_Model.materials[slot].maps[MATERIAL_MAP_METALNESS].texture = {0};
                    }
                    pMat->m_vMaterialSlotSources[slot].clear();
                }
                else if (slotMaterialIndex < static_cast<int>(picker.vMaterialPaths.size()))
                {
                    LoadMaterialToEntity(&entity, picker.vMaterialPaths[slotMaterialIndex], slot);
                    pMat->m_vMaterialSlotSources[slot] = picker.vMaterialPaths[slotMaterialIndex];
                }
            }
            ImGui::PopID();
        }
    }

    picker.SelectedIndex = 0;
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
        for (int i = 1; i < static_cast<int>(picker.vMaterialPaths.size()); ++i)
        {
            if (picker.vMaterialPaths[i] == displayPath)
            {
                picker.SelectedIndex = i;
                break;
            }
        }
    }

    ImGui::Text("%s", lang.Word("material_file"));
    if (!picker.vMaterialNames.empty())
    {
        if (ImGui::Combo("##material_combo", &picker.SelectedIndex, picker.vMaterialNames.data(),
                         static_cast<int>(picker.vMaterialNames.size())))
                         {
            editor.SaveState();
            if (picker.SelectedIndex == 0)
            {
                CEntityTextureService::ClearMaterialTextures(&entity);
                pMat->m_TextureName.clear();
                pMat->m_Texture = {0};
                pMat->m_TextureSource = TEXTURE_NONE;
            }
            else if (picker.SelectedIndex > 0 && picker.SelectedIndex < static_cast<int>(picker.vMaterialPaths.size()))
            {
                const std::string& selectedPath = picker.vMaterialPaths[picker.SelectedIndex];
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
    editor.m_Ui.m_Inspector.SelectedTextureIndex = 0;
    for (size_t i = 0; i < editor.m_Assets.TextureCount(); ++i)
    {
        vTextureNames.push_back(editor.m_Assets.TextureByIndex(i).Name.c_str());
        if (editor.m_Assets.TextureByIndex(i).Name == pMat->m_AlbedoTextureName)
        {
            editor.m_Ui.m_Inspector.SelectedTextureIndex = static_cast<int>(i);
        }
    }

    ImGui::Text("Direct texture");
    if (!vTextureNames.empty() && ImGui::Combo("##direct_texture", &editor.m_Ui.m_Inspector.SelectedTextureIndex,
        vTextureNames.data(), static_cast<int>(vTextureNames.size())))
        {
        editor.SaveState();
        const int selectedTextureIndex = editor.m_Ui.m_Inspector.SelectedTextureIndex;
        if (selectedTextureIndex == 0)
        {
            pMat->m_AlbedoTextureName.clear();
            pMat->m_Texture = {0};
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
            pMat->m_AlbedoTextureName = editor.m_Assets.TextureByIndex(selectedTextureIndex).Name;
            pMat->m_TextureName.clear();
            pMat->m_Texture = editor.m_Assets.TextureByIndex(selectedTextureIndex).Texture;
            pMat->m_TextureSource = TEXTURE_EXTERNAL;
        }
        CEntityTextureService::MarkEntityUVDirty(&entity);
    }

    editor.m_Ui.m_Inspector.SelectedNormalIndex = 0;
    for (size_t i = 0; i < editor.m_Assets.TextureCount(); ++i)
    {
        if (editor.m_Assets.TextureByIndex(i).Name == pMat->m_NormalTextureName)
        {
            editor.m_Ui.m_Inspector.SelectedNormalIndex = static_cast<int>(i);
            break;
        }
    }
    ImGui::Text("Normal map");
    if (!vTextureNames.empty() && ImGui::Combo("##normal_map", &editor.m_Ui.m_Inspector.SelectedNormalIndex,
        vTextureNames.data(), static_cast<int>(vTextureNames.size())))
        {
        editor.SaveState();
        const int selectedNormalIndex = editor.m_Ui.m_Inspector.SelectedNormalIndex;
        if (selectedNormalIndex == 0)
        {
            pMat->m_NormalTextureName.clear();
            for (int m = 0; m < pMesh->m_Model.materialCount; ++m)
            {
                if (pMesh->m_Model.materials[m].maps)
                {
                    pMesh->m_Model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = {0};
                }
            }
        }
        else
        {
            pMat->m_NormalTextureName = editor.m_Assets.TextureByIndex(selectedNormalIndex).Name;
            const Texture2D normalTexture = editor.m_Assets.TextureByIndex(selectedNormalIndex).Texture;
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

        float aUvScale[2] = {pMat->m_UvScale.x, pMat->m_UvScale.y};
        if (ImGui::DragFloat2(lang.Word("uv_scale_x"), aUvScale, 0.1f, 0.1f, 5.0f))
        {
            trackMaterialEdit();
            pMat->m_UvScale = {aUvScale[0], aUvScale[1]};
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

    drag("Size",           pText->m_Size,           0.01f, 0.05f, 10.f);
    drag("Thickness",      pText->m_Thickness,      0.01f, 0.01f,  5.f);
    drag("Letter Spacing", pText->m_LetterSpacing, 0.005f, 0.f, 100);

    ImGui::Separator();

    SFontPickerState& fonts = editor.m_Ui.m_FontPicker;

    if (!fonts.Scanned)
    {
        fonts.vFonts = CFreetypeTextMesh::GetSystemFonts();
        fonts.vFontNames.clear();
        for (auto& f : fonts.vFonts)
        {
            fonts.vFontNames.push_back(f.first.c_str());
        }

        for (int i = 0; i < (int)fonts.vFonts.size(); i++)
        {
            if (fonts.vFonts[i].second == pText->m_FontPath)
            {
                fonts.SelectedIndex = i;
                break;
            }
        }
        fonts.Scanned = true;
    }

    ImGui::Text("Font");
    if (!fonts.vFontNames.empty())
    {
        if (ImGui::Combo("##font", &fonts.SelectedIndex, fonts.vFontNames.data(), (int)fonts.vFontNames.size()))
        {
            editor.SaveState();
            pText->m_FontPath = fonts.vFonts[fonts.SelectedIndex].second;
            CModelService::UpdateModel(&entity, editor.m_Text);
            CEntityTextureService::MarkEntityBoundsDirty(&entity);
        }
    }
    else
    {
        ImGui::TextDisabled("No fonts found");
    }

    if (!pText->m_FontPath.empty() && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", pText->m_FontPath.c_str());
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
        vScriptNames.push_back(vAvailable[i].className);
        if (vAvailable[i].className == pScript->m_ScriptName)
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
                    pScript->SyncWithHeaderFields(headerInfo.vFields);
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
            pScript->m_ScriptName = selectedInfo.className;
            pScript->m_ScriptPath = selectedInfo.headerPath;
            pScript->SyncWithHeaderFields(selectedInfo.vFields);
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
            if (info.className == pScript->m_ScriptName)
            {
                if (!info.vFields.empty())
                {
                    pScript->SyncWithHeaderFields(info.vFields);
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

        switch (field.type)
        {
        case quark::script::EScriptFieldType::FLOAT:
        {
            float val = field.floatValue;
            if (ImGui::DragFloat(field.name.c_str(), &val, 0.1f))
            {
                field.floatValue = val;
                valueChanged = true;
            }
            break;
        }
        case quark::script::EScriptFieldType::INT:
        {
            int val = field.intValue;
            if (ImGui::DragInt(field.name.c_str(), &val, 1))
            {
                field.intValue = val;
                valueChanged = true;
            }
            break;
        }
        case quark::script::EScriptFieldType::BOOL:
        {
            bool val = field.boolValue;
            if (ImGui::Checkbox(field.name.c_str(), &val))
            {
                field.boolValue = val;
                valueChanged = true;
            }
            break;
        }
        case quark::script::EScriptFieldType::STRING:
        {
            char aBuf[256];
            size_t copied = field.stringValue.copy(aBuf, sizeof(aBuf) - 1);
            aBuf[copied] = '\0';
            if (ImGui::InputText(field.name.c_str(), aBuf, sizeof(aBuf)))
            {
                field.stringValue = aBuf;
                valueChanged = true;
            }
            break;
        }
        case quark::script::EScriptFieldType::VEC3:
        {
            float aVec[3] = {field.vec3Value.x, field.vec3Value.y, field.vec3Value.z};
            if (ImGui::DragFloat3(field.name.c_str(), aVec, 0.1f))
            {
                field.vec3Value = {aVec[0], aVec[1], aVec[2]};
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
                pScript->SyncWithHeaderFields(headerInfo.vFields);
                editor.m_SceneDirty = true;
            }
        }
    }
}

namespace {

bool EditGuiColor4(const char* label, Color& c)
{
    float rgba[4] = {
        c.r / 255.0f,
        c.g / 255.0f,
        c.b / 255.0f,
        c.a / 255.0f
    };
    if (ImGui::ColorEdit4(label, rgba, ImGuiColorEditFlags_NoInputs))
    {
        c.r = static_cast<unsigned char>(std::clamp(rgba[0] * 255.0f, 0.0f, 255.0f));
        c.g = static_cast<unsigned char>(std::clamp(rgba[1] * 255.0f, 0.0f, 255.0f));
        c.b = static_cast<unsigned char>(std::clamp(rgba[2] * 255.0f, 0.0f, 255.0f));
        c.a = static_cast<unsigned char>(std::clamp(rgba[3] * 255.0f, 0.0f, 255.0f));
        return true;
    }
    return false;
}

} // namespace

void CComponentUIHelper::DrawUiRectTransformComponent(
    CEditor& editor,
    CUiRectTransformComponent* pRectTransform)
{
    if (!pRectTransform)
    {
        return;
    }

    bool changed = false;
    float position[2] = { pRectTransform->m_Position.x, pRectTransform->m_Position.y };
    if (ImGui::DragFloat2("Position", position, 1.0f, -4096.0f, 4096.0f, "%.0f px"))
    {
        pRectTransform->m_Position = { position[0], position[1] };
        changed = true;
    }

    float size[2] = { pRectTransform->m_Size.x, pRectTransform->m_Size.y };
    if (ImGui::DragFloat2("Size", size, 1.0f, 16.0f, 2048.0f, "%.0f px"))
    {
        pRectTransform->m_Size = { std::max(16.0f, size[0]), std::max(16.0f, size[1]) };
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
    char text[4096] = {};
    std::snprintf(text, sizeof(text), "%s", pText->m_Text.c_str());
    if (ImGui::InputTextMultiline("Text", text, sizeof(text), ImVec2(-1.0f, 82.0f)))
    {
        pText->m_Text = text;
        changed = true;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Character");
    char font[256] = {};
    std::snprintf(font, sizeof(font), "%s", pText->m_Font.c_str());
    if (ImGui::InputText("Font", font, sizeof(font)))
    {
        pText->m_Font = font;
        changed = true;
    }

    const char* fontStyles[] = { "Normal", "Bold", "Italic", "Bold Italic" };
    int fontStyle = static_cast<int>(pText->m_FontStyle);
    if (ImGui::Combo("Font Style", &fontStyle, fontStyles, IM_ARRAYSIZE(fontStyles)))
    {
        pText->m_FontStyle = static_cast<EUiTextFontStyle>(fontStyle);
        changed = true;
    }

    float fontSize = pText->m_FontSize;
    if (ImGui::InputFloat("Font Size", &fontSize, 1.0f, 5.0f, "%.0f"))
    {
        pText->m_FontSize = std::clamp(fontSize, 1.0f, 256.0f);
        changed = true;
    }

    float lineSpacing = pText->m_LineSpacing;
    if (ImGui::InputFloat("Line Spacing", &lineSpacing, 0.1f, 1.0f, "%.2f"))
    {
        pText->m_LineSpacing = std::clamp(lineSpacing, 0.1f, 10.0f);
        changed = true;
    }
    changed |= ImGui::Checkbox("Rich Text", &pText->m_RichText);

    ImGui::Spacing();
    ImGui::TextUnformatted("Paragraph");
    const char* horizontalAlignment[] = { "Left", "Center", "Right" };
    int horizontal = static_cast<int>(pText->m_HorizontalAlignment);
    if (ImGui::Combo("Horizontal Alignment", &horizontal, horizontalAlignment, IM_ARRAYSIZE(horizontalAlignment)))
    {
        pText->m_HorizontalAlignment = static_cast<EUiTextAlignment>(horizontal);
        changed = true;
    }

    const char* verticalAlignment[] = { "Top", "Middle", "Bottom" };
    int vertical = static_cast<int>(pText->m_VerticalAlignment);
    if (ImGui::Combo("Vertical Alignment", &vertical, verticalAlignment, IM_ARRAYSIZE(verticalAlignment)))
    {
        pText->m_VerticalAlignment = static_cast<EUiTextVerticalAlignment>(vertical);
        changed = true;
    }

    const char* overflowOptions[] = { "Wrap", "Truncate" };
    int horizontalOverflow = static_cast<int>(pText->m_HorizontalOverflow);
    if (ImGui::Combo("Horizontal Overflow", &horizontalOverflow, overflowOptions, IM_ARRAYSIZE(overflowOptions)))
    {
        pText->m_HorizontalOverflow = static_cast<EUiTextOverflow>(horizontalOverflow);
        changed = true;
    }

    int verticalOverflow = static_cast<int>(pText->m_VerticalOverflow);
    if (ImGui::Combo("Vertical Overflow", &verticalOverflow, overflowOptions, IM_ARRAYSIZE(overflowOptions)))
    {
        pText->m_VerticalOverflow = static_cast<EUiTextOverflow>(verticalOverflow);
        changed = true;
    }

    changed |= ImGui::Checkbox("Best Fit", &pText->m_BestFit);
    changed |= EditGuiColor4("Color", pText->m_Color);

    char material[256] = {};
    std::snprintf(material, sizeof(material), "%s", pText->m_Material.c_str());
    if (ImGui::InputText("Material", material, sizeof(material)))
    {
        pText->m_Material = material;
        changed = true;
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

    const char* widgetTypes[] = { "Button", "Inputfield", "Combo" };
    int currentType = static_cast<int>(pUi->m_WidgetType);
    if (ImGui::Combo("Widget Type", &currentType, widgetTypes, IM_ARRAYSIZE(widgetTypes)))
    {
        editor.SaveState();
        pUi->m_WidgetType = static_cast<EGuiWidgetType>(currentType);
        auto pComponents = entity.GetComponents();
        auto pText = pComponents->GetComponentOfType<CTextComponent>();
        auto pRectTransform = pComponents->GetComponentOfType<CUiRectTransformComponent>();
        if (pUi->m_WidgetType == EGuiWidgetType::Button)
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
        else if (pUi->m_WidgetType == EGuiWidgetType::InputField)
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
        else if (pUi->m_WidgetType == EGuiWidgetType::Combo)
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

    if (pUi->m_WidgetType == EGuiWidgetType::Button)
    {
        ImGui::Spacing();
        ImGui::Separator();
        changed |= ImGui::Checkbox("Interactable", &pUi->m_Interactable);

        const char* transitionOptions[] = { "None", "Color Tint" };
        int transition = static_cast<int>(pUi->m_Transition);
        if (ImGui::Combo("Transition", &transition, transitionOptions, IM_ARRAYSIZE(transitionOptions)))
        {
            pUi->m_Transition = static_cast<EGuiButtonTransition>(transition);
            changed = true;
        }

        const char* targetGraphics[] = { "Button Background" };
        int targetGraphic = 0;
        ImGui::BeginDisabled();
        ImGui::Combo("Target Graphic", &targetGraphic, targetGraphics, IM_ARRAYSIZE(targetGraphics));
        ImGui::EndDisabled();

        ImGui::BeginDisabled(pUi->m_Transition == EGuiButtonTransition::NONE);
        changed |= EditGuiColor4("Normal Color", pUi->m_NormalColor);
        changed |= EditGuiColor4("Highlighted Color", pUi->m_HighlightedColor);
        changed |= EditGuiColor4("Pressed Color", pUi->m_PressedColor);
        changed |= EditGuiColor4("Selected Color", pUi->m_SelectedColor);
        changed |= EditGuiColor4("Disabled Color", pUi->m_DisabledColor);
        changed |= ImGui::SliderFloat("Color Multiplier", &pUi->m_ColorMultiplier, 0.0f, 2.0f, "%.2f");
        changed |= ImGui::SliderFloat("Fade Duration", &pUi->m_FadeDuration, 0.0f, 1.0f, "%.2f s");
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        const char* navigationOptions[] = { "Automatic", "None" };
        int navigation = static_cast<int>(pUi->m_Navigation);
        if (ImGui::Combo("Navigation", &navigation, navigationOptions, IM_ARRAYSIZE(navigationOptions)))
        {
            pUi->m_Navigation = static_cast<EGuiButtonNavigation>(navigation);
            changed = true;
        }
        if (ImGui::Button("Visualize", ImVec2(-1.0f, 0.0f)))
        {
            pUi->m_ShowNavigationVisualization = !pUi->m_ShowNavigationVisualization;
            changed = true;
        }
    }

    if (pUi->m_WidgetType == EGuiWidgetType::InputField)
    {
        char phBuf[256] = {};
        std::snprintf(phBuf, sizeof(phBuf), "%s", pUi->m_Placeholder.c_str());
        if (ImGui::InputText("Placeholder", phBuf, sizeof(phBuf)))
        {
            pUi->m_Placeholder = phBuf;
            changed = true;
        }
    }
    else if (pUi->m_WidgetType == EGuiWidgetType::Combo)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Combo Items");
        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(pUi->m_vItems.size()); ++i)
        {
            ImGui::PushID(i);
            char itemBuf[256] = {};
            std::snprintf(itemBuf, sizeof(itemBuf), "%s",
                          pUi->m_vItems[static_cast<size_t>(i)].c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36.0f);
            if (ImGui::InputText("##item", itemBuf, sizeof(itemBuf)))
            {
                pUi->m_vItems[static_cast<size_t>(i)] = itemBuf;
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
            pUi->m_vItems.erase(pUi->m_vItems.begin() + removeIdx);
            if (pUi->m_SelectedIndex >= static_cast<int>(pUi->m_vItems.size()))
            {
                pUi->m_SelectedIndex = std::max(0, static_cast<int>(pUi->m_vItems.size()) - 1);
            }
            changed = true;
        }
        if (ImGui::Button("+ Add Item", ImVec2(-1.0f, 0.0f)))
        {
            pUi->m_vItems.push_back("New Item " + std::to_string(pUi->m_vItems.size() + 1));
            changed = true;
        }
        if (!pUi->m_vItems.empty())
        {
            int sel = std::clamp(pUi->m_SelectedIndex, 0, static_cast<int>(pUi->m_vItems.size()) - 1);
            if (ImGui::SliderInt("Selected Index", &sel, 0, static_cast<int>(pUi->m_vItems.size()) - 1))
            {
                pUi->m_SelectedIndex = sel;
                pUi->m_RuntimeDesc.selectedIndex = sel;
                changed = true;
            }
        }
    }

    if (pUi->m_WidgetType == EGuiWidgetType::Button)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Clicks in Play Mode: %d", pUi->m_RuntimeDesc.clickCount);
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
