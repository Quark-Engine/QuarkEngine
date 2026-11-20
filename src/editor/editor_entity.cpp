#include "editor/editor_entity.h"
#include "editor/editor_viewers.h"
#include "engine/scene_document.h"
#include "engine/scene_runtime.h"
#include "models.h"
#include "nlohmann/json.hpp"

#include <fstream>
#include <memory>
#include <sstream>

namespace
{

    std::string SanitizePrefabFileName(const std::string& rawName)
    {
        std::string sanitized;
        sanitized.reserve(rawName.size());
        for (unsigned char ch : rawName)
        {
            if (ch < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
                ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*')
            {
                sanitized.push_back('_');
            }
            else
            {
                sanitized.push_back(static_cast<char>(ch));
            }
        }

        while (!sanitized.empty() && (sanitized.back() == ' ' || sanitized.back() == '.'))
        {
            sanitized.pop_back();
        }

        if (sanitized.empty())
        {
            return "Prefab";
        }
        return sanitized;
    }

} // anonymous

void CEntityFactory::AssignName(CEntity& entity, const char* pNewName)
{
    if (!pNewName || pNewName[0] == '\0')
    {
        return;
    }
    entity.m_Name = pNewName;
}

CEntity CEntityFactory::FromAsset(CScene& scene, CModelAsset& asset)
{
    CEntity entity;
    CMeshComponent* pMesh = entity.GetMeshComponent();
    if (!pMesh)
    {
        return entity;
    }

    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    pMesh->m_Type = asset.m_Type;
    pMesh->m_pAsset = &asset;
    pMesh->m_AssetName = asset.m_Name;
    pMesh->m_Segments = 16;
    const std::string baseName = asset.m_IsProcedural ? asset.m_Name : fs::path(asset.m_Name).stem().string();
    entity.m_Name = scene.MakeUniqueName(baseName.empty() ? "Model" : baseName);

    auto pMatComp = std::make_shared<CMaterialComponent>();
    entity.GetComponents()->AddComponent(pMatComp);
    CMaterialComponent* pMat = pMatComp.get();

    if (asset.m_IsProcedural)
    {
        pMesh->m_Model = asset.pfnGenerator(pMesh->m_Segments);
        pMesh->m_OwnsModelInstance = true;
        CMeshOverrideService::Clear(entity);
        CEntityTextureService::StoreUV(&entity);
        CEntityTextureService::StoreMaterialTextures(&entity);
        pMat->m_TextureSource = TEXTURE_NONE;
        pMat->m_TextureName.clear();

        if (asset.m_Name == "Text" || asset.m_Name == "3D Text")
        {
            auto pTextComp = std::make_shared<CText3DComponent>();
            entity.GetComponents()->AddComponent(pTextComp);
        }
    }
    else
    {
        if (!CModelService::LoadInstance(asset, pMesh->m_Model))
        {
            pMesh->m_pAsset = nullptr;
            pMesh->m_AssetName.clear();
            pMesh->m_Model = {};
            return entity;
        }

        pMesh->m_OwnsModelInstance = true;
        CMeshOverrideService::Clear(entity);
        CEntityTextureService::StoreUV(&entity);
        CEntityTextureService::StoreMaterialTextures(&entity);

        bool hasEmbedded = false;
        for (int i = 0; i < pMesh->m_Model.materialCount; i++)
        {
            if (pMesh->m_Model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0)
            {
                hasEmbedded = true;
                break;
            }
        }

        pMat->m_TextureSource = hasEmbedded ? TEXTURE_MODEL : TEXTURE_NONE;

        if (!asset.m_FilePath.empty())
        {
            std::filesystem::path modelPath(asset.m_FilePath);
            std::filesystem::path mtlPath = modelPath.parent_path() / (modelPath.stem().string() + ".mtl");

            if (std::filesystem::exists(mtlPath))
            {
                LoadMaterialToEntity(&entity, mtlPath);
                return entity;
            }
        }
    }

    pMat->m_Texture = { 0 };
    return entity;
}

CEntity CEntityFactory::Light(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Light");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    for (size_t i = 0; i < pCm->GetComponentCount(); ++i)
    {
        if (pCm->GetComponent(i)->GetType() == COMPONENT_MESH)
        {
            pCm->RemoveComponent(i);
            break;
        }
    }

    auto pLight = std::make_shared<CLightComponent>();
    pLight->m_Light = CreateLighting({ 0, 0, 0 }, WHITE);
    pCm->AddComponent(pLight);

    return entity;
}

CEntity CEntityFactory::Camera(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Camera");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;
    entity.GetTransformComponent()->m_Position = { 0.0f, 1.5f, 6.0f };
    entity.GetTransformComponent()->m_Rotation = { -12.0f, 0.0f, 0.0f };

    CComponentManager* pCm = entity.GetComponents();
    for (size_t componentIndex = 0; componentIndex < pCm->GetComponentCount(); ++componentIndex)
    {
        if (pCm->GetComponent(componentIndex)->GetType() == COMPONENT_MESH)
        {
            pCm->RemoveComponent(componentIndex);
            break;
        }
    }

    auto pCamera = std::make_shared<CCameraComponent>();
    pCm->AddComponent(pCamera);
    return entity;
}

CEntity CEntityFactory::UiWidget(CScene& scene, EGuiWidgetType widgetType, int parentIndex)
{
    const char* baseName = "Ui Button";
    if (widgetType == EGuiWidgetType::INPUT_FIELD)
    {
        baseName = "Ui Inputfield";
    }
    else if (widgetType == EGuiWidgetType::COMBO)
    {
        baseName = "Ui Combo";
    }

    int existingUiCount = 0;
    for (const CEntity& existing : scene.m_vEntities)
    {
        if (existing.GetUiComponent() != nullptr)
        {
            ++existingUiCount;
        }
    }

    CEntity entity;
    entity.m_Name = scene.MakeUniqueName(baseName);
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pUi = std::make_shared<CUiComponent>();
    pUi->m_WidgetType = widgetType;
    pUi->m_Name = pUi->GetTypeName();
    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    auto pText = std::make_shared<CTextComponent>();
    pRectTransform->m_Position = { 32.0f, 32.0f + static_cast<float>(existingUiCount) * 42.0f };

    switch (widgetType)
    {
        case EGuiWidgetType::BUTTON:
            pRectTransform->m_Size = { 150.0f, 30.0f };
            pText->m_Text = "Button";
            pText->m_HorizontalAlignment = EUiTextAlignment::CENTER;
            break;
        case EGuiWidgetType::INPUT_FIELD:
            pRectTransform->m_Size = { 260.0f, 30.0f };
            pText->m_Text.clear();
            pText->m_HorizontalAlignment = EUiTextAlignment::LEFT;
            pUi->m_Placeholder = "InputField";
            break;
        case EGuiWidgetType::COMBO:
            pRectTransform->m_Size = { 260.0f, 30.0f };
            pUi->m_vItems = { "Combo" };
            pUi->m_SelectedIndex = 0;
            pText->m_Text = "Combo";
            pText->m_HorizontalAlignment = EUiTextAlignment::LEFT;
            break;
    }

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pUi);
    pCm->AddComponent(pText);
    pUi->SyncToRuntimeDesc(pRectTransform.get(), pText.get());
    return entity;
}

CEntity CEntityFactory::Canvas(CScene& scene, ECanvasRenderMode mode, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Canvas");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pTransform = std::make_shared<CTransformComponent>();
    pTransform->m_Position = { 0.0f, 1.5f, 0.0f };
    pTransform->m_Rotation = { 0.0f, 0.0f, 0.0f };
    pTransform->m_Scale = { 0.005f, 0.005f, 0.005f };

    auto pCanvas = std::make_shared<CCanvasComponent>();
    pCanvas->m_RenderMode = mode;
    pCanvas->m_Width = 800.0f;
    pCanvas->m_Height = 600.0f;

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_AnchoredPosition = { 0.0f, 0.0f };
    pRectTransform->m_Position = { 0.0f, 0.0f };
    pRectTransform->m_Size = { 800.0f, 600.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    pCm->AddComponent(pTransform);
    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pCanvas);
    return entity;
}

CEntity CEntityFactory::UiPanel(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Panel");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_AnchorMin = { 0.0f, 0.0f };
    pRectTransform->m_AnchorMax = { 1.0f, 1.0f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };
    pRectTransform->m_OffsetMin = { 0.0f, 0.0f };
    pRectTransform->m_OffsetMax = { 0.0f, 0.0f };
    pRectTransform->m_Size = { 800.0f, 600.0f };

    auto pPanel = std::make_shared<CPanelComponent>();
    pPanel->m_BackgroundColor = { 35, 35, 45, 200 };

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pPanel);
    return entity;
}

CEntity CEntityFactory::UiButton(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Button");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 160.0f, 40.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    auto pButton = std::make_shared<CButtonComponent>();
    pButton->m_NormalColor = { 70, 70, 80, 255 };
    pButton->m_HighlightedColor = { 90, 90, 105, 255 };
    pButton->m_PressedColor = { 50, 50, 60, 255 };
    pButton->m_SelectedColor = { 90, 90, 105, 255 };

    auto pText = std::make_shared<CTextComponent>();
    pText->m_Text = "Button";
    pText->m_FontSize = 18.0f;
    pText->m_HorizontalAlignment = EUiTextAlignment::CENTER;
    pText->m_VerticalAlignment = EUiTextVerticalAlignment::MIDDLE;

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pButton);
    pCm->AddComponent(pText);
    return entity;
}

CEntity CEntityFactory::UiText(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Text");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 200.0f, 40.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    auto pText = std::make_shared<CTextComponent>();
    pText->m_Text = "New Text";
    pText->m_FontSize = 20.0f;
    pText->m_HorizontalAlignment = EUiTextAlignment::CENTER;
    pText->m_VerticalAlignment = EUiTextVerticalAlignment::MIDDLE;

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pText);
    return entity;
}

CEntity CEntityFactory::UiImage(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Image");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 100.0f, 100.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    auto pImage = std::make_shared<CImageComponent>();
    pImage->m_Color = WHITE;

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pImage);
    return entity;
}

CEntity CEntityFactory::UiSlider(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Slider");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 180.0f, 24.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    auto pSlider = std::make_shared<CSliderComponent>();
    pSlider->m_MinValue = 0.0f;
    pSlider->m_MaxValue = 1.0f;
    pSlider->m_Value = 0.5f;

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pSlider);
    return entity;
}

CEntity CEntityFactory::UiToggle(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Toggle");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 140.0f, 26.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    auto pToggle = std::make_shared<CToggleComponent>();
    pToggle->m_IsOn = true;
    pToggle->m_Label = "Toggle";

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pToggle);
    return entity;
}

CEntity CEntityFactory::UiProgressBar(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Progress Bar");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 200.0f, 24.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };

    auto pProgressBar = std::make_shared<CProgressBarComponent>();
    pProgressBar->m_MinValue = 0.0f;
    pProgressBar->m_MaxValue = 100.0f;
    pProgressBar->m_Value = 65.0f;

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pProgressBar);
    return entity;
}

CEntity CEntityFactory::UiScrollView(CScene& scene, int parentIndex)
{
    CEntity entity;
    entity.m_Name = scene.MakeUniqueName("Scroll View");
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_ParentId = parentIndex;

    CComponentManager* pCm = entity.GetComponents();
    pCm->RemoveMeshAndMaterialForUi(false);

    auto pRectTransform = std::make_shared<CUiRectTransformComponent>();
    pRectTransform->m_Size = { 260.0f, 220.0f };
    pRectTransform->m_AnchorMin = { 0.5f, 0.5f };
    pRectTransform->m_AnchorMax = { 0.5f, 0.5f };
    pRectTransform->m_Pivot = { 0.5f, 0.5f };
    pRectTransform->m_ClipChildren = true;

    auto pScrollView = std::make_shared<CScrollViewComponent>();
    pScrollView->m_ContentSize = { 260.0f, 500.0f };

    pCm->AddComponent(pRectTransform);
    pCm->AddComponent(pScrollView);
    return entity;
}

bool CEntityFactory::SavePrefabToFile(const CScene& scene, int rootEntityIndex, const fs::path& filePath)
{
    if (rootEntityIndex < 0 || rootEntityIndex >= static_cast<int>(scene.m_vEntities.size()))
    {
        return false;
    }

    const std::string document = quark::CSceneDocument::SerializePrefab(scene, rootEntityIndex, 4);
    if (document.empty())
    {
        return false;
    }

    if (filePath.has_parent_path())
    {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream f(filePath);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open %s", filePath.string().c_str()));
        return false;
    }

    f << document;
    f.close();
    if (!f)
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to write %s", filePath.string().c_str()));
        return false;
    }

    const_cast<CScene&>(scene).m_vEntities[rootEntityIndex].m_PrefabPath = filePath.generic_string();
    TraceLog(LogLevel::Info, "PREFAB", TextFormat("Saved prefab: %s", filePath.string().c_str()));
    return true;
}

bool CEntityFactory::SavePrefab(const CScene& scene, int rootEntityIndex,
                                const fs::path& directoryPath, fs::path* pOutFilePath)
{
    if (rootEntityIndex < 0 || rootEntityIndex >= static_cast<int>(scene.m_vEntities.size()))
    {
        return false;
    }

    const std::string baseFileName = SanitizePrefabFileName(scene.m_vEntities[rootEntityIndex].m_Name);
    const fs::path filePath = directoryPath / (baseFileName + ".prefab");
    if (!SavePrefabToFile(scene, rootEntityIndex, filePath))
    {
        return false;
    }

    if (pOutFilePath)
    {
        *pOutFilePath = filePath;
    }
    return true;
}

void CEntityFactory::SavePrefab(CEntity entity, const fs::path path)
{
    CScene tempScene;
    entity.m_Id = 0;
    entity.m_ParentId = -1;
    tempScene.m_vEntities.push_back(std::move(entity));
    SavePrefab(tempScene, 0, path);
}

std::vector<int> CEntityFactory::InstantiatePrefab(CScene& scene, CAssetLibrary& assets,
                                                   const fs::path& filename,
                                                   const CComponentFactoryRegistry& factories,
                                                   int parentIndex)
{
    std::ifstream f(filename);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open prefab %s", filename.string().c_str()));
        return {};
    }

    std::stringstream buffer;
    buffer << f.rdbuf();

    std::vector<int> vCreatedIndices;
    if (!quark::CSceneDocument::InstantiatePrefab(buffer.str(), scene, factories, parentIndex, &vCreatedIndices))
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to parse prefab %s", filename.string().c_str()));
        return {};
    }

    if (!vCreatedIndices.empty())
    {
        const int rootCreatedIdx = vCreatedIndices.front();
        if (rootCreatedIdx >= 0 && rootCreatedIdx < static_cast<int>(scene.m_vEntities.size()))
        {
            scene.m_vEntities[rootCreatedIdx].m_PrefabPath = filename.generic_string();
        }
    }

    for (int createdIndex : vCreatedIndices)
    {
        if (createdIndex < 0 || createdIndex >= static_cast<int>(scene.m_vEntities.size()))
        {
            continue;
        }
        CEntity& entity = scene.m_vEntities[createdIndex];
        quark::CSceneRuntime::RestoreEntityModel(entity, assets);
        quark::CSceneRuntime::ResetEntityLightRuntime(entity);
    }

    return vCreatedIndices;
}

namespace
{
    void CollectSubtreeChildrenInOrder(const CScene& scene, int parentIndex, std::vector<int>& vOutIndices)
    {
        for (int i = 0; i < static_cast<int>(scene.m_vEntities.size()); ++i)
        {
            if (scene.m_vEntities[i].m_ParentId == parentIndex)
            {
                vOutIndices.push_back(i);
                CollectSubtreeChildrenInOrder(scene, i, vOutIndices);
            }
        }
    }

    bool PathsReferToSamePrefab(const std::string& pathA, const fs::path& pathB)
    {
        if (pathA.empty() || pathB.empty())
        {
            return false;
        }
        const fs::path normA = fs::path(pathA).lexically_normal();
        const fs::path normB = pathB.lexically_normal();
        if (normA == normB || normA.generic_string() == normB.generic_string())
        {
            return true;
        }
        std::error_code ec;
        if (fs::exists(normA, ec) && fs::exists(normB, ec) && fs::equivalent(normA, normB, ec))
        {
            return true;
        }
        return false;
    }
} // anonymous

bool CEntityFactory::RevertPrefabInstance(CScene& scene, CAssetLibrary& assets, int rootEntityIndex,
                                          const CComponentFactoryRegistry& factories)
{
    if (rootEntityIndex < 0 || rootEntityIndex >= static_cast<int>(scene.m_vEntities.size()))
    {
        return false;
    }

    const std::string prefabPathStr = scene.m_vEntities[rootEntityIndex].m_PrefabPath;
    if (prefabPathStr.empty())
    {
        return false;
    }

    std::ifstream f(prefabPathStr);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open prefab %s", prefabPathStr.c_str()));
        return false;
    }

    std::stringstream buffer;
    buffer << f.rdbuf();

    CScene tempPrefabScene;
    std::vector<int> vPrefabIndices;
    if (!quark::CSceneDocument::InstantiatePrefab(buffer.str(), tempPrefabScene, factories, -1, &vPrefabIndices) ||
        tempPrefabScene.m_vEntities.empty())
    {
        return false;
    }

    CEntity& rootTarget = scene.m_vEntities[rootEntityIndex];
    const std::string savedName = rootTarget.m_Name;
    const int savedParentId = rootTarget.m_ParentId;
    const int savedId = rootTarget.m_Id;
    bool hasSavedTransform = false;
    CTransformComponent savedTransform;
    if (const CTransformComponent* pOldTransform = rootTarget.GetTransformComponent())
    {
        savedTransform = *pOldTransform;
        hasSavedTransform = true;
    }
    bool hasSavedRectPos = false;
    Vec2 savedAnchoredPos = { 0.0f, 0.0f };
    if (const CUiRectTransformComponent* pOldRect = rootTarget.GetRectTransformComponent())
    {
        savedAnchoredPos = pOldRect->m_AnchoredPosition;
        hasSavedRectPos = true;
    }

    rootTarget = tempPrefabScene.m_vEntities[0];
    rootTarget.m_Id = savedId;
    rootTarget.m_Name = savedName;
    rootTarget.m_ParentId = savedParentId;
    rootTarget.m_PrefabPath = prefabPathStr;
    if (hasSavedTransform && rootTarget.GetTransformComponent())
    {
        rootTarget.GetTransformComponent()->m_Position = savedTransform.m_Position;
        rootTarget.GetTransformComponent()->m_Rotation = savedTransform.m_Rotation;
        rootTarget.GetTransformComponent()->m_Scale = savedTransform.m_Scale;
    }
    if (hasSavedRectPos && rootTarget.GetRectTransformComponent())
    {
        rootTarget.GetRectTransformComponent()->SetAnchoredPosition(savedAnchoredPos);
    }
    quark::CSceneRuntime::RestoreEntityModel(rootTarget, assets);
    quark::CSceneRuntime::ResetEntityLightRuntime(rootTarget);

    std::vector<int> vExistingChildren;
    CollectSubtreeChildrenInOrder(scene, rootEntityIndex, vExistingChildren);

    const size_t prefabChildCount = tempPrefabScene.m_vEntities.size() - 1;
    std::vector<int> vMappedSceneIndices(tempPrefabScene.m_vEntities.size(), rootEntityIndex);

    for (size_t childIdx = 0; childIdx < prefabChildCount; ++childIdx)
    {
        const size_t prefabEntityIndex = childIdx + 1;
        CEntity prefabChild = tempPrefabScene.m_vEntities[prefabEntityIndex];
        const int prefabParentIdx = (prefabChild.m_ParentId >= 0 &&
                                     prefabChild.m_ParentId < static_cast<int>(vMappedSceneIndices.size()))
            ? prefabChild.m_ParentId
            : 0;
        const int mappedParentSceneIdx = vMappedSceneIndices[prefabParentIdx];

        if (childIdx < vExistingChildren.size())
        {
            const int targetSceneIdx = vExistingChildren[childIdx];
            vMappedSceneIndices[prefabEntityIndex] = targetSceneIdx;
            prefabChild.m_Id = scene.m_vEntities[targetSceneIdx].m_Id;
            prefabChild.m_ParentId = mappedParentSceneIdx;
            scene.m_vEntities[targetSceneIdx] = std::move(prefabChild);
            quark::CSceneRuntime::RestoreEntityModel(scene.m_vEntities[targetSceneIdx], assets);
            quark::CSceneRuntime::ResetEntityLightRuntime(scene.m_vEntities[targetSceneIdx]);
        }
        else
        {
            const int newSceneIdx = static_cast<int>(scene.m_vEntities.size());
            vMappedSceneIndices[prefabEntityIndex] = newSceneIdx;
            prefabChild.m_Id = newSceneIdx;
            prefabChild.m_Name = scene.MakeUniqueName(prefabChild.m_Name);
            prefabChild.m_ParentId = mappedParentSceneIdx;
            scene.m_vEntities.push_back(std::move(prefabChild));
            quark::CSceneRuntime::RestoreEntityModel(scene.m_vEntities[newSceneIdx], assets);
            quark::CSceneRuntime::ResetEntityLightRuntime(scene.m_vEntities[newSceneIdx]);
        }
    }

    return true;
}

int CEntityFactory::SyncPrefabInstances(CScene& scene, CAssetLibrary& assets, const fs::path& prefabPath,
                                        const CComponentFactoryRegistry& factories,
                                        int skipRootEntityIndex)
{
    int syncedCount = 0;
    const int initialCount = static_cast<int>(scene.m_vEntities.size());
    for (int i = 0; i < initialCount; ++i)
    {
        if (i == skipRootEntityIndex)
        {
            continue;
        }
        if (PathsReferToSamePrefab(scene.m_vEntities[i].m_PrefabPath, prefabPath))
        {
            if (RevertPrefabInstance(scene, assets, i, factories))
            {
                ++syncedCount;
            }
        }
    }
    return syncedCount;
}

CEntity CEntityFactory::FromPrefab(CScene& scene, const CAssetLibrary& assets, const fs::path filename,
                                   const CComponentFactoryRegistry& factories)
{
    std::ifstream f(filename);
    if (!f.is_open())
    {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open prefab %s", filename.string().c_str()));
        return {};
    }

    std::stringstream buffer;
    buffer << f.rdbuf();

    CScene tempScene;
    std::vector<int> vCreatedIndices;
    if (!quark::CSceneDocument::InstantiatePrefab(buffer.str(), tempScene, factories, -1, &vCreatedIndices) ||
        vCreatedIndices.empty())
    {
        return {};
    }

    CEntity& entity = tempScene.m_vEntities.front();
    entity.m_Id = static_cast<int>(scene.m_vEntities.size());
    entity.m_Name = scene.MakeUniqueName(entity.m_Name);
    quark::CSceneRuntime::RestoreEntityModel(entity, const_cast<CAssetLibrary&>(assets));
    quark::CSceneRuntime::ResetEntityLightRuntime(entity);
    return std::move(entity);
}
