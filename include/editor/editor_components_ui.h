#ifndef __EDITOR_COMPONENTS_UI_H__
#define __EDITOR_COMPONENTS_UI_H__
#include "component.h"
#include "entity.h"
#include "imgui.h"
#include <string>

class CEditor;

class CComponentUIHelper
{
public:
    static void DrawEntityInspector(CEditor& editor, CEntity& entity, Shader shader);

    static void DrawTransformComponent(CEditor& editor, CEntity& entity, CTransformComponent* pTransform);
    static void DrawMeshComponent(CEditor& editor, CEntity& entity, CMeshComponent* pMesh);
    static void DrawLightComponent(CEditor& editor, CEntity& entity, CLightComponent* pLight, Shader shader);
    static void DrawMaterialComponent(CEditor& editor, CEntity& entity, CMaterialComponent* pMaterial);
    static void DrawCollisionComponent(CEditor& editor, CEntity& entity, CCollisionComponent* pCollision);
    static void DrawRigidbodyComponent(CEditor& editor, CEntity& entity, CRigidbodyComponent* pBody);
    static void Draw3dTextComponent(CEditor& editor, CEntity& entity, CText3DComponent* pText);
    static void DrawUiRectTransformComponent(CEditor& editor, CUiRectTransformComponent* pRectTransform);
    static void DrawCanvasComponent(CEditor& editor, CEntity& entity, CCanvasComponent* pCanvas);
    static void DrawImageComponent(CEditor& editor, CEntity& entity, CImageComponent* pImage);
    static void DrawPanelComponent(CEditor& editor, CEntity& entity, CPanelComponent* pPanel);
    static void DrawButtonComponent(CEditor& editor, CEntity& entity, CButtonComponent* pButton);
    static void DrawUiTextComponent(CEditor& editor, CTextComponent* pText);
    static void DrawSliderComponent(CEditor& editor, CSliderComponent* pSlider);
    static void DrawToggleComponent(CEditor& editor, CToggleComponent* pToggle);
    static void DrawProgressBarComponent(CEditor& editor, CProgressBarComponent* pProgressBar);
    static void DrawScrollViewComponent(CEditor& editor, CScrollViewComponent* pScrollView);
    static void DrawHorizontalLayoutComponent(CEditor& editor, CHorizontalLayoutComponent* pLayout);
    static void DrawVerticalLayoutComponent(CEditor& editor, CVerticalLayoutComponent* pLayout);
    static void DrawGridLayoutComponent(CEditor& editor, CGridLayoutComponent* pLayout);
    static void DrawScriptComponent(CEditor& editor, CEntity& entity, CScriptComponent* pScript);
    static void DrawUiComponent(CEditor& editor, CEntity& entity, CUiComponent* pUi);
};

#endif // __EDITOR_COMPONENTS_UI_H__
