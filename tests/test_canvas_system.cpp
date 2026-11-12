#include "test_harness.h"

#include "engine/canvas_system.h"
#include "engine/component_factory_registry.h"
#include "engine/scene_document.h"

#include "QuarkCore/QuarkCore.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace
{

    int AddEntity(CScene& target, const std::string& name, int parentId = -1)
    {
        CEntity added(static_cast<int>(target.m_vEntities.size()));
        added.m_Name = name;
        added.m_ParentId = parentId;
        target.m_vEntities.push_back(added);
        return static_cast<int>(target.m_vEntities.size()) - 1;
    }

} // anonymous

TEST(CanvasComponent, default_properties_and_serialization)
{
    CCanvasComponent canvas;
    CHECK(canvas.m_RenderMode == ECanvasRenderMode::WORLD_SPACE);
    CHECK_NEAR(canvas.m_Width, 800.0f, 0.001);
    CHECK_NEAR(canvas.m_Height, 600.0f, 0.001);
    CHECK(canvas.m_SortingOrder == 0);
    CHECK(canvas.m_PixelPerfect == false);

    canvas.m_RenderMode = ECanvasRenderMode::SCREEN_SPACE_CAMERA;
    canvas.m_Width = 1920.0f;
    canvas.m_Height = 1080.0f;
    canvas.m_SortingOrder = 5;
    canvas.m_PixelPerfect = true;
    canvas.m_ScaleFactor = 2.0f;
    canvas.m_PlaneDistance = 100.0f;
    canvas.m_EventCameraEntityId = 2;
    canvas.m_OverrideSorting = true;

    nlohmann::json j;
    canvas.Serialize(j);

    CCanvasComponent deserialized;
    deserialized.Deserialize(j);

    CHECK(deserialized.m_RenderMode == ECanvasRenderMode::SCREEN_SPACE_CAMERA);
    CHECK_NEAR(deserialized.m_Width, 1920.0f, 0.001);
    CHECK_NEAR(deserialized.m_Height, 1080.0f, 0.001);
    CHECK(deserialized.m_SortingOrder == 5);
    CHECK(deserialized.m_PixelPerfect == true);
    CHECK_NEAR(deserialized.m_ScaleFactor, 2.0f, 0.001);
    CHECK_NEAR(deserialized.m_PlaneDistance, 100.0f, 0.001);
    CHECK(deserialized.m_EventCameraEntityId == 2);
    CHECK(deserialized.m_OverrideSorting == true);
}

TEST(RectTransform, stretch_and_center_calculations)
{
    CUiRectTransformComponent centerRect;
    centerRect.m_AnchorMin = { 0.5f, 0.5f };
    centerRect.m_AnchorMax = { 0.5f, 0.5f };
    centerRect.m_Pivot = { 0.5f, 0.5f };
    centerRect.m_AnchoredPosition = { 0.0f, 0.0f };
    centerRect.m_Size = { 200.0f, 100.0f };

    Rectangle rect = centerRect.CalculateLocalRect({ 800.0f, 600.0f });
    CHECK_NEAR(rect.x, 300.0f, 0.001);
    CHECK_NEAR(rect.y, 250.0f, 0.001);
    CHECK_NEAR(rect.width, 200.0f, 0.001);
    CHECK_NEAR(rect.height, 100.0f, 0.001);

    CUiRectTransformComponent stretchRect;
    stretchRect.m_AnchorMin = { 0.0f, 0.0f };
    stretchRect.m_AnchorMax = { 1.0f, 1.0f };
    stretchRect.m_OffsetMin = { 10.0f, 20.0f };
    stretchRect.m_OffsetMax = { -10.0f, -20.0f };

    rect = stretchRect.CalculateLocalRect({ 800.0f, 600.0f });
    CHECK_NEAR(rect.x, 10.0f, 0.001);
    CHECK_NEAR(rect.y, 20.0f, 0.001);
    CHECK_NEAR(rect.width, 780.0f, 0.001);
    CHECK_NEAR(rect.height, 560.0f, 0.001);
}

TEST(ButtonComponent, state_transitions_and_color_multiplier)
{
    CButtonComponent button;
    button.m_NormalColor = { 200, 200, 200, 255 };
    button.m_HighlightedColor = { 240, 240, 240, 255 };
    button.m_PressedColor = { 150, 150, 150, 255 };
    button.m_DisabledColor = { 80, 80, 80, 255 };
    button.m_ColorMultiplier = 1.0f;

    Color c = button.GetCurrentColor();
    CHECK(c.r == 200 && c.g == 200 && c.b == 200);

    button.m_IsHovered = true;
    c = button.GetCurrentColor();
    CHECK(c.r == 240 && c.g == 240 && c.b == 240);

    button.m_IsPressed = true;
    c = button.GetCurrentColor();
    CHECK(c.r == 150 && c.g == 150 && c.b == 150);

    button.m_Interactable = false;
    c = button.GetCurrentColor();
    CHECK(c.r == 80 && c.g == 80 && c.b == 80);

    button.m_Interactable = true;
    button.m_IsHovered = false;
    button.m_IsPressed = false;
    button.m_ColorMultiplier = 1.5f;
    c = button.GetCurrentColor();
    CHECK(c.r == 255 && c.g == 255 && c.b == 255);

    bool clicked = false;
    button.m_fnOnClick = [&clicked]() { clicked = true; };
    button.m_fnOnClick();
    CHECK(clicked == true);
}

TEST(ImageAndPanel, serialization_round_trip)
{
    CImageComponent image;
    image.m_SourceTexture = "assets/textures/ui_frame.png";
    image.m_Color = { 255, 128, 64, 255 };
    image.m_Material = "assets/materials/ui.mat";
    image.m_RaycastTarget = false;
    image.m_ImageType = EUiImageType::SLICED;
    image.m_PreserveAspect = true;
    image.m_FillCenter = false;

    nlohmann::json jImg;
    image.Serialize(jImg);
    CImageComponent dImg;
    dImg.Deserialize(jImg);
    CHECK(dImg.m_SourceTexture == "assets/textures/ui_frame.png");
    CHECK(dImg.m_Color.r == 255 && dImg.m_Color.g == 128 && dImg.m_Color.b == 64);
    CHECK(dImg.m_Material == "assets/materials/ui.mat");
    CHECK(dImg.m_RaycastTarget == false);
    CHECK(dImg.m_ImageType == EUiImageType::SLICED);
    CHECK(dImg.m_PreserveAspect == true);
    CHECK(dImg.m_FillCenter == false);

    CPanelComponent panel;
    panel.m_BackgroundImage = "assets/textures/panel_bg.png";
    panel.m_BackgroundColor = { 30, 30, 40, 200 };
    panel.m_RaycastTarget = true;

    nlohmann::json jPnl;
    panel.Serialize(jPnl);
    CPanelComponent dPnl;
    dPnl.Deserialize(jPnl);
    CHECK(dPnl.m_BackgroundImage == "assets/textures/panel_bg.png");
    CHECK(dPnl.m_BackgroundColor.r == 30 && dPnl.m_BackgroundColor.a == 200);
    CHECK(dPnl.m_RaycastTarget == true);
}

TEST(CanvasSystem, coordinate_conversions_and_world_raycast)
{
    quark::ui::CCanvasSystem& system = quark::ui::CCanvasSystem::Get();

    CCanvasComponent canvas;
    canvas.m_RenderMode = ECanvasRenderMode::WORLD_SPACE;
    canvas.m_Width = 800.0f;
    canvas.m_Height = 600.0f;

    Mat4 canvasWorld = Mat4::scale(0.01f, 0.01f, 0.01f);

    Vec3 worldPos = system.CanvasToWorld(canvas, canvasWorld, { 400.0f, 300.0f });
    CHECK_NEAR(worldPos.x, 0.0f, 0.001);
    CHECK_NEAR(worldPos.y, 0.0f, 0.001);
    CHECK_NEAR(worldPos.z, 0.0f, 0.001);

    Vec2 canvasPos = system.WorldToCanvas(canvas, canvasWorld, worldPos);
    CHECK_NEAR(canvasPos.x, 400.0f, 0.001);
    CHECK_NEAR(canvasPos.y, 300.0f, 0.001);

    Vec3 cornerWorld = system.CanvasToWorld(canvas, canvasWorld, { 0.0f, 0.0f });
    CHECK_NEAR(cornerWorld.x, -4.0f, 0.001);
    CHECK_NEAR(cornerWorld.y, -3.0f, 0.001);
    CHECK_NEAR(cornerWorld.z, 0.0f, 0.001);

    CScene scene;
    Ray ray;
    ray.position = { 0.0f, 0.0f, 5.0f };
    ray.direction = { 0.0f, 0.0f, -1.0f };

    Vec2 hitPoint = { 0.0f, 0.0f };
    float hitDist = 0.0f;
    bool hit = system.RaycastWorldCanvas(scene, canvas, canvasWorld, ray, hitPoint, hitDist);
    CHECK(hit == true);
    CHECK_NEAR(hitDist, 5.0f, 0.001);
    CHECK_NEAR(hitPoint.x, 400.0f, 0.001);
    CHECK_NEAR(hitPoint.y, 300.0f, 0.001);

    ray.direction = { 0.0f, 0.0f, 1.0f };
    hit = system.RaycastWorldCanvas(scene, canvas, canvasWorld, ray, hitPoint, hitDist);
    CHECK(hit == false);
}

TEST(CanvasSystem, scene_document_full_ui_round_trip)
{
    CComponentFactoryRegistry registry;
    CScene scene;

    int canvasIdx = AddEntity(scene, "UICanvas", -1);
    auto pCanvas = std::make_shared<CCanvasComponent>();
    pCanvas->m_RenderMode = ECanvasRenderMode::WORLD_SPACE;
    pCanvas->m_Width = 800.0f;
    pCanvas->m_Height = 600.0f;
    scene.m_vEntities[canvasIdx].GetComponents()->AddComponent(pCanvas);
    auto* pCanvasTrans = scene.m_vEntities[canvasIdx].GetTransformComponent();
    pCanvasTrans->m_Position = { 0.0f, 1.5f, 0.0f };
    pCanvasTrans->m_Scale = { 0.005f, 0.005f, 0.005f };

    int panelIdx = AddEntity(scene, "BackgroundPanel", scene.m_vEntities[canvasIdx].m_Id);
    auto pPanel = std::make_shared<CPanelComponent>();
    pPanel->m_BackgroundColor = { 20, 24, 30, 220 };
    scene.m_vEntities[panelIdx].GetComponents()->AddComponent(pPanel);
    auto pPanelRect = std::make_shared<CUiRectTransformComponent>();
    pPanelRect->m_AnchorMin = { 0.0f, 0.0f };
    pPanelRect->m_AnchorMax = { 1.0f, 1.0f };
    scene.m_vEntities[panelIdx].GetComponents()->AddComponent(pPanelRect);

    int btnIdx = AddEntity(scene, "StartButton", scene.m_vEntities[panelIdx].m_Id);
    auto pButton = std::make_shared<CButtonComponent>();
    pButton->m_NormalColor = { 50, 150, 250, 255 };
    scene.m_vEntities[btnIdx].GetComponents()->AddComponent(pButton);
    auto pBtnRect = std::make_shared<CUiRectTransformComponent>();
    pBtnRect->m_AnchoredPosition = { 0.0f, -50.0f };
    pBtnRect->m_Size = { 200.0f, 50.0f };
    scene.m_vEntities[btnIdx].GetComponents()->AddComponent(pBtnRect);

    int textIdx = AddEntity(scene, "BtnText", scene.m_vEntities[btnIdx].m_Id);
    auto pText = std::make_shared<CTextComponent>();
    pText->m_Text = "Play Game";
    pText->m_FontSize = 20.0f;
    pText->m_Color = WHITE;
    scene.m_vEntities[textIdx].GetComponents()->AddComponent(pText);

    const std::string doc = quark::CSceneDocument::Serialize(scene);
    CHECK(!doc.empty());

    CScene restored;
    CHECK(quark::CSceneDocument::Deserialize(doc, restored, registry));
    CHECK(restored.m_vEntities.size() == 4);

    CEntity& rCanvasEnt = restored.m_vEntities[0];
    CHECK(rCanvasEnt.GetCanvasComponent() != nullptr);
    CHECK(rCanvasEnt.GetCanvasComponent()->m_RenderMode == ECanvasRenderMode::WORLD_SPACE);
    CHECK_NEAR(rCanvasEnt.GetCanvasComponent()->m_Width, 800.0f, 0.001);

    CEntity& rPanelEnt = restored.m_vEntities[1];
    CHECK(rPanelEnt.GetPanelComponent() != nullptr);
    CHECK(rPanelEnt.GetPanelComponent()->m_BackgroundColor.r == 20);

    CEntity& rBtnEnt = restored.m_vEntities[2];
    CHECK(rBtnEnt.GetButtonComponent() != nullptr);
    CHECK(rBtnEnt.GetButtonComponent()->m_NormalColor.r == 50);

    CEntity& rTextEnt = restored.m_vEntities[3];
    CHECK(rTextEnt.GetTextComponent() != nullptr);
    CHECK(rTextEnt.GetTextComponent()->m_Text == "Play Game");
}

TEST(CanvasSystem, example_scene_creation)
{
    CComponentFactoryRegistry registry;
    CScene scene;

    int camIdx = AddEntity(scene, "Main Camera", -1);
    auto* pCamTrans = scene.m_vEntities[camIdx].GetTransformComponent();
    pCamTrans->m_Position = { 0.0f, 1.5f, -5.0f };
    pCamTrans->m_Rotation = { 0.0f, 0.0f, 0.0f };
    auto pCamera = std::make_shared<CCameraComponent>();
    pCamera->m_FieldOfView = 60.0f;
    scene.m_vEntities[camIdx].GetComponents()->AddComponent(pCamera);

    int lightIdx = AddEntity(scene, "Directional Light", -1);
    auto* pLightTrans = scene.m_vEntities[lightIdx].GetTransformComponent();
    pLightTrans->m_Position = { 5.0f, 10.0f, -5.0f };
    pLightTrans->m_Rotation = { 45.0f, -30.0f, 0.0f };
    auto pLight = std::make_shared<CLightComponent>();
    pLight->m_Light.m_Light.type = LIGHT_DIRECTIONAL;
    pLight->m_Light.m_Color = WHITE;
    pLight->m_Light.m_Intensity = 1.0f;
    scene.m_vEntities[lightIdx].GetComponents()->AddComponent(pLight);

    int canvasIdx = AddEntity(scene, "UI Canvas", -1);
    scene.m_vEntities[canvasIdx].GetComponents()->RemoveMeshAndMaterialForUi(true);
    auto* pCanvasTrans = scene.m_vEntities[canvasIdx].GetTransformComponent();
    pCanvasTrans->m_Position = { 0.0f, 1.5f, 0.0f };
    pCanvasTrans->m_Rotation = { 0.0f, 0.0f, 0.0f };
    pCanvasTrans->m_Scale = { 0.005f, 0.005f, 0.005f };
    auto pCanvas = std::make_shared<CCanvasComponent>();
    pCanvas->m_RenderMode = ECanvasRenderMode::WORLD_SPACE;
    pCanvas->m_Width = 800.0f;
    pCanvas->m_Height = 600.0f;
    scene.m_vEntities[canvasIdx].GetComponents()->AddComponent(pCanvas);

    int panelIdx = AddEntity(scene, "Background Panel", scene.m_vEntities[canvasIdx].m_Id);
    scene.m_vEntities[panelIdx].GetComponents()->RemoveMeshAndMaterialForUi(false);
    auto pPanel = std::make_shared<CPanelComponent>();
    pPanel->m_BackgroundColor = { 25, 30, 45, 230 };
    scene.m_vEntities[panelIdx].GetComponents()->AddComponent(pPanel);
    auto pPanelRect = std::make_shared<CUiRectTransformComponent>();
    pPanelRect->m_AnchorMin = { 0.0f, 0.0f };
    pPanelRect->m_AnchorMax = { 1.0f, 1.0f };
    pPanelRect->m_Pivot = { 0.5f, 0.5f };
    pPanelRect->m_OffsetMin = { 0.0f, 0.0f };
    pPanelRect->m_OffsetMax = { 0.0f, 0.0f };
    scene.m_vEntities[panelIdx].GetComponents()->AddComponent(pPanelRect);

    int titleIdx = AddEntity(scene, "Title Text", scene.m_vEntities[panelIdx].m_Id);
    scene.m_vEntities[titleIdx].GetComponents()->RemoveMeshAndMaterialForUi(false);
    auto pTitleText = std::make_shared<CTextComponent>();
    pTitleText->m_Text = "QUARK ENGINE CANVAS";
    pTitleText->m_FontSize = 40.0f;
    pTitleText->m_Color = { 255, 215, 0, 255 };
    pTitleText->m_HorizontalAlignment = EUiTextAlignment::CENTER;
    pTitleText->m_VerticalAlignment = EUiTextVerticalAlignment::MIDDLE;
    scene.m_vEntities[titleIdx].GetComponents()->AddComponent(pTitleText);
    auto pTitleRect = std::make_shared<CUiRectTransformComponent>();
    pTitleRect->m_AnchorMin = { 0.5f, 0.8f };
    pTitleRect->m_AnchorMax = { 0.5f, 0.8f };
    pTitleRect->m_Pivot = { 0.5f, 0.5f };
    pTitleRect->m_AnchoredPosition = { 0.0f, 0.0f };
    pTitleRect->m_Size = { 600.0f, 60.0f };
    scene.m_vEntities[titleIdx].GetComponents()->AddComponent(pTitleRect);

    auto createButton = [&](const std::string& name, const std::string& label, float posY, Color normColor)
    {
        int btnId = AddEntity(scene, name, scene.m_vEntities[panelIdx].m_Id);
        scene.m_vEntities[btnId].GetComponents()->RemoveMeshAndMaterialForUi(false);
        auto pBtn = std::make_shared<CButtonComponent>();
        pBtn->m_NormalColor = normColor;
        pBtn->m_HighlightedColor = { static_cast<unsigned char>(std::min(255, normColor.r + 35)),
                                     static_cast<unsigned char>(std::min(255, normColor.g + 35)),
                                     static_cast<unsigned char>(std::min(255, normColor.b + 35)), 255 };
        pBtn->m_PressedColor = { static_cast<unsigned char>(normColor.r / 2),
                                 static_cast<unsigned char>(normColor.g / 2),
                                 static_cast<unsigned char>(normColor.b / 2), 255 };
        scene.m_vEntities[btnId].GetComponents()->AddComponent(pBtn);
        auto pBtnRect = std::make_shared<CUiRectTransformComponent>();
        pBtnRect->m_AnchorMin = { 0.5f, 0.5f };
        pBtnRect->m_AnchorMax = { 0.5f, 0.5f };
        pBtnRect->m_Pivot = { 0.5f, 0.5f };
        pBtnRect->m_AnchoredPosition = { 0.0f, posY };
        pBtnRect->m_Size = { 300.0f, 50.0f };
        scene.m_vEntities[btnId].GetComponents()->AddComponent(pBtnRect);

        int textId = AddEntity(scene, name + " Text", scene.m_vEntities[btnId].m_Id);
        scene.m_vEntities[textId].GetComponents()->RemoveMeshAndMaterialForUi(false);
        auto pBtnText = std::make_shared<CTextComponent>();
        pBtnText->m_Text = label;
        pBtnText->m_FontSize = 24.0f;
        pBtnText->m_Color = WHITE;
        pBtnText->m_HorizontalAlignment = EUiTextAlignment::CENTER;
        pBtnText->m_VerticalAlignment = EUiTextVerticalAlignment::MIDDLE;
        scene.m_vEntities[textId].GetComponents()->AddComponent(pBtnText);
        auto pTextRect = std::make_shared<CUiRectTransformComponent>();
        pTextRect->m_AnchorMin = { 0.0f, 0.0f };
        pTextRect->m_AnchorMax = { 1.0f, 1.0f };
        pTextRect->m_Pivot = { 0.5f, 0.5f };
        pTextRect->m_OffsetMin = { 0.0f, 0.0f };
        pTextRect->m_OffsetMax = { 0.0f, 0.0f };
        scene.m_vEntities[textId].GetComponents()->AddComponent(pTextRect);
    };

    createButton("Start Button", "Start Game", 40.0f, Color{ 40, 140, 240, 255 });
    createButton("Settings Button", "Settings", -30.0f, Color{ 60, 160, 120, 255 });
    createButton("Exit Button", "Exit", -100.0f, Color{ 200, 60, 60, 255 });

    const std::string serialized = quark::CSceneDocument::Serialize(scene);
    CHECK(!serialized.empty());

    std::filesystem::create_directories("assets/scenes");
    std::ofstream outScene("assets/scenes/canvas_world_space.json");
    CHECK(outScene.is_open());
    outScene << serialized;
    outScene.close();

    std::filesystem::create_directories("projects/canvas_world_space");
    std::ofstream outProjScene("projects/canvas_world_space/scene.json");
    CHECK(outProjScene.is_open());
    outProjScene << serialized;
    outProjScene.close();

    nlohmann::json projManifest;
    projManifest["name"] = "Canvas World Space Example";
    projManifest["version"] = "1.0.0";
    projManifest["scene"] = "scene.json";
    std::ofstream outProj("projects/canvas_world_space/canvas_world_space.quarkproj");
    CHECK(outProj.is_open());
    outProj << projManifest.dump(4);
    outProj.close();

    std::ifstream inScene("assets/scenes/canvas_world_space.json");
    CHECK(inScene.is_open());
    std::stringstream buffer;
    buffer << inScene.rdbuf();

    CScene loadedScene;
    CHECK(quark::CSceneDocument::Deserialize(buffer.str(), loadedScene, registry));
    CHECK(loadedScene.m_vEntities.size() == 11);
}
