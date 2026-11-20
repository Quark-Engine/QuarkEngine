#ifndef __ENTITY_H__
#define __ENTITY_H__

#include "lighting.h"
#include "component.h"

#include <memory>
#include <string>
#include <vector>

class CComponentManager;

const char* ObjectTypeName(EObjectType type);

class CModelAsset
{
public:
    CModelAsset();
    ~CModelAsset();

    CModelAsset(const CModelAsset&) = delete;
    CModelAsset& operator=(const CModelAsset&) = delete;

    CModelAsset(CModelAsset&& other) noexcept;
    CModelAsset& operator=(CModelAsset&& other) noexcept;

    std::string m_Name;
    std::string m_FilePath;

    EObjectType m_Type = OBJECT_CUBE;
    bool m_IsProcedural = false;
    std::function<Model(int)> pfnGenerator;
    Model m_LoadedModel;

    Model TakeLoadedModel();
    void Unload();
};

class CEntity
{
public:
    CEntity();
    explicit CEntity(int entityId);
    ~CEntity();

    CEntity(const CEntity& other);
    CEntity& operator=(const CEntity& other);

    CEntity(CEntity&& other) noexcept = default;
    CEntity& operator=(CEntity&& other) noexcept = default;

    CComponentManager* GetComponents();
    const CComponentManager* GetComponents() const;

    CTransformComponent* GetTransformComponent();
    const CTransformComponent* GetTransformComponent() const;
    CMeshComponent* GetMeshComponent();
    const CMeshComponent* GetMeshComponent() const;
    CLightComponent* GetLightComponent();
    const CLightComponent* GetLightComponent() const;
    CMaterialComponent* GetMaterialComponent();
    const CMaterialComponent* GetMaterialComponent() const;
    CCollisionComponent* GetCollisionComponent();
    const CCollisionComponent* GetCollisionComponent() const;
    CRigidbodyComponent* GetRigidbodyComponent();
    const CRigidbodyComponent* GetRigidbodyComponent() const;
    CCameraComponent* GetCameraComponent();
    CScriptComponent* GetScriptComponent();
    const CScriptComponent* GetScriptComponent() const;
    const CCameraComponent* GetCameraComponent() const;
    CUiComponent* GetUiComponent();
    const CUiComponent* GetUiComponent() const;
    CCanvasComponent* GetCanvasComponent();
    const CCanvasComponent* GetCanvasComponent() const;
    CUiRectTransformComponent* GetRectTransformComponent();
    const CUiRectTransformComponent* GetRectTransformComponent() const;
    CImageComponent* GetImageComponent();
    const CImageComponent* GetImageComponent() const;
    CPanelComponent* GetPanelComponent();
    const CPanelComponent* GetPanelComponent() const;
    CButtonComponent* GetButtonComponent();
    const CButtonComponent* GetButtonComponent() const;
    CTextComponent* GetTextComponent();
    const CTextComponent* GetTextComponent() const;
    CSliderComponent* GetSliderComponent();
    const CSliderComponent* GetSliderComponent() const;
    CToggleComponent* GetToggleComponent();
    const CToggleComponent* GetToggleComponent() const;
    CProgressBarComponent* GetProgressBarComponent();
    const CProgressBarComponent* GetProgressBarComponent() const;
    CScrollViewComponent* GetScrollViewComponent();
    const CScrollViewComponent* GetScrollViewComponent() const;
    CHorizontalLayoutComponent* GetHorizontalLayoutComponent();
    const CHorizontalLayoutComponent* GetHorizontalLayoutComponent() const;
    CVerticalLayoutComponent* GetVerticalLayoutComponent();
    const CVerticalLayoutComponent* GetVerticalLayoutComponent() const;
    CGridLayoutComponent* GetGridLayoutComponent();
    const CGridLayoutComponent* GetGridLayoutComponent() const;
    bool HasUiComponent() const;

    int m_Id = 0;
    std::string m_Name;
    std::vector<std::string> m_vTags;
    int m_ParentId = -1;
    bool m_IsGroup = false;
    std::string m_PrefabPath;

    std::unique_ptr<CComponentManager> m_pComponents;
};

#endif // __ENTITY_H__
