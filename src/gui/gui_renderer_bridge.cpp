#include "gui/gui_renderer_bridge.h"

#include "component.h"
#include "entity.h"
#include "scene.h"

#include <cmath>
#include <filesystem>
#include <vector>

namespace quark
{
    namespace gui
    {

        namespace
        {

            Font s_GuiFont = {};
            bool s_GuiFontLoaded = false;
            bool s_GuiFontAttempted = false;
            bool s_BackendInstalled = false;
            const Camera2D* s_pGuiScissorCamera2D = nullptr;

            void EnsureGuiFontLoaded()
            {
                if (s_GuiFontAttempted || !IsWindowReady())
                {
                    return;
                }
                s_GuiFontAttempted = true;

                const char* aCandidatePaths[] = {
                    "assets/font/Rubik-Regular.ttf",
                    "resources/Rubik-Regular.ttf",
                    "font/Rubik-Regular.ttf",
                    "../assets/font/Rubik-Regular.ttf",
                    "../../assets/font/Rubik-Regular.ttf",
                    "../../../assets/font/Rubik-Regular.ttf"
                };

                std::string foundPath;
                std::error_code ec;
                for (const char* pRel : aCandidatePaths)
                {
                    if (std::filesystem::exists(pRel, ec) && !ec)
                    {
                        foundPath = pRel;
                        break;
                    }
                }

                if (foundPath.empty())
                {
                    return;
                }

                std::vector<int> vCodepoints;
                vCodepoints.reserve(450);
                for (int cp = 32; cp <= 126; ++cp)
                {
                    vCodepoints.push_back(cp);
                }
                for (int cp = 160; cp <= 255; ++cp)
                {
                    vCodepoints.push_back(cp);
                }
                for (int cp = 0x0400; cp <= 0x04FF; ++cp)
                {
                    vCodepoints.push_back(cp);
                }

                s_GuiFont = LoadFontEx(foundPath.c_str(), 32, vCodepoints.data(), static_cast<int>(vCodepoints.size()));
                if (s_GuiFont.texture.id != 0 && s_GuiFont.glyphCount > 0)
                {
                    SetTextureFilter(s_GuiFont.texture, TEXTURE_FILTER_BILINEAR);
                    s_GuiFontLoaded = true;
                }
            }

            void BridgeDrawText(const char* text, float x, float y, float fontSize, float lineSpacing, Color color)
            {
                if (!text || !text[0])
                {
                    return;
                }
                EnsureGuiFontLoaded();
                if (s_GuiFontLoaded && s_GuiFont.texture.id != 0)
                {
                    DrawTextEx(s_GuiFont, text, { x, y }, fontSize, lineSpacing, color);
                }
                else
                {
                    DrawText(text, static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), static_cast<int>(std::round(fontSize)), color);
                }
            }

            Vec2 BridgeMeasureText(const char* text, float fontSize)
            {
                if (!text || !text[0])
                {
                    return { 0.0f, fontSize };
                }
                EnsureGuiFontLoaded();
                if (s_GuiFontLoaded && s_GuiFont.texture.id != 0)
                {
                    return MeasureTextEx(s_GuiFont, text, fontSize, 1.0f);
                }
                const int width = MeasureText(text, static_cast<int>(std::round(fontSize)));
                return { static_cast<float>(width), fontSize };
            }

            void BridgeDrawRectangle(int posX, int posY, int width, int height, Color color)
            {
                DrawRectangle(static_cast<float>(posX), static_cast<float>(posY), static_cast<float>(width), static_cast<float>(height), color);
            }

            void BridgeDrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
            {
                DrawRectangleRounded(rec, roundness, segments, color);
            }

            void BridgeDrawTriangle(Vec2 v1, Vec2 v2, Vec2 v3, Color color)
            {
                DrawTriangle(v1, v2, v3, color);
            }

            void BridgeDrawLineEx(Vec2 startPos, Vec2 endPos, float thick, Color color)
            {
                DrawLineEx(startPos, endPos, thick, color);
            }

            void BridgeBeginScissorMode(int x, int y, int width, int height)
            {
                if (s_pGuiScissorCamera2D != nullptr)
                {
                    const Vec2 topLeft = GetWorldToScreen2D(
                        { static_cast<float>(x), static_cast<float>(y) },
                        *s_pGuiScissorCamera2D);
                    const Vec2 bottomRight = GetWorldToScreen2D(
                        { static_cast<float>(x + width), static_cast<float>(y + height) },
                        *s_pGuiScissorCamera2D);
                    x = static_cast<int>(std::floor(topLeft.x));
                    y = static_cast<int>(std::floor(topLeft.y));
                    width = static_cast<int>(std::ceil(bottomRight.x) - std::floor(topLeft.x));
                    height = static_cast<int>(std::ceil(bottomRight.y) - std::floor(topLeft.y));
                }
                BeginScissorMode(x, y, width, height);
            }

            void BridgeEndScissorMode()
            {
                EndScissorMode();
            }

            double BridgeGetTime()
            {
                return GetTime();
            }

        } // anonymous

        void EnsureGuiBackendInstalled()
        {
            if (s_BackendInstalled)
            {
                return;
            }
            SGuiBackend backend;
            backend.pfnDrawRectangle = &BridgeDrawRectangle;
            backend.pfnDrawRectangleRounded = &BridgeDrawRectangleRounded;
            backend.pfnDrawTriangle = &BridgeDrawTriangle;
            backend.pfnDrawLineEx = &BridgeDrawLineEx;
            backend.pfnBeginScissorMode = &BridgeBeginScissorMode;
            backend.pfnEndScissorMode = &BridgeEndScissorMode;
            backend.pfnDrawText = &BridgeDrawText;
            backend.pfnMeasureText = &BridgeMeasureText;
            backend.pfnGetTime = &BridgeGetTime;

            CGuiSystem::Get().SetBackend(backend);
            s_BackendInstalled = true;
        }

        void ShutdownGuiBackend()
        {
            if (s_GuiFontLoaded && IsWindowReady())
            {
                UnloadFont(s_GuiFont);
            }
            s_GuiFont = {};
            s_GuiFontLoaded = false;
            s_GuiFontAttempted = false;
            s_pGuiScissorCamera2D = nullptr;
        }

        void SetGuiScissorCamera2D(const Camera2D* pCamera)
        {
            s_pGuiScissorCamera2D = pCamera;
        }

        SGuiInputState GatherGuiInputState(Vec2 mousePosInViewport)
        {
            SGuiInputState input;
            input.m_MousePosition = mousePosInViewport;
            input.m_MouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            input.m_MousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            input.m_MouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
            input.m_MouseWheelMove = GetMouseWheelMove();

            input.m_KeyBackspacePressed = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE);
            input.m_KeyDeletePressed = IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE);
            input.m_KeyLeftPressed = IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT);
            input.m_KeyRightPressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT);
            input.m_KeyHomePressed = IsKeyPressed(KEY_HOME);
            input.m_KeyEndPressed = IsKeyPressed(KEY_END);
            input.m_KeyEnterPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
            input.m_KeyEscapePressed = IsKeyPressed(KEY_ESCAPE);

            int cp = GetCharPressed();
            while (cp > 0)
            {
                input.m_vTypedCodepoints.push_back(static_cast<uint32_t>(cp));
                cp = GetCharPressed();
            }

            return input;
        }

        void RenderSceneGui(CScene& scene, Vec2 mousePosInViewport, bool interactive)
        {
            EnsureGuiBackendInstalled();

            std::vector<CUiComponent*> vUiComponents;
            std::vector<CTextComponent*> vTextComponents;
            std::vector<SGuiWidgetDesc*> vWidgetDescs;

            for (CEntity& entity : scene.m_vEntities)
            {
                CUiComponent* pUi = entity.GetUiComponent();
                if (!pUi || !pUi->m_Enabled)
                {
                    continue;
                }
                CComponentManager* pComponents = entity.GetComponents();
                auto pRectTransform = pComponents->GetComponentOfType<CUiRectTransformComponent>();
                auto pText = pComponents->GetComponentOfType<CTextComponent>();
                pUi->SyncToRuntimeDesc(pRectTransform.get(), pText.get());
                vUiComponents.push_back(pUi);
                vTextComponents.push_back(pText.get());
                vWidgetDescs.push_back(&pUi->m_RuntimeDesc);
            }

            if (vWidgetDescs.empty())
            {
                return;
            }

            SGuiInputState input;
            if (interactive)
            {
                input = GatherGuiInputState(mousePosInViewport);
            }
            else
            {
                input.m_MousePosition = mousePosInViewport;
            }

            if (interactive)
            {
                CGuiSystem::Get().UpdateAndRender(vWidgetDescs, input, true);

                for (size_t componentIndex = 0; componentIndex < vUiComponents.size(); ++componentIndex)
                {
                    vUiComponents[componentIndex]->SyncFromRuntimeDesc(vTextComponents[componentIndex]);
                }
            }
            else
            {
                std::vector<SGuiWidgetDesc> vPassiveWidgets;
                vPassiveWidgets.reserve(vWidgetDescs.size());
                for (const SGuiWidgetDesc* pWidget : vWidgetDescs)
                {
                    vPassiveWidgets.push_back(*pWidget);
                }

                vWidgetDescs.clear();
                vWidgetDescs.reserve(vPassiveWidgets.size());
                for (SGuiWidgetDesc& widget : vPassiveWidgets)
                {
                    vWidgetDescs.push_back(&widget);
                }
                CGuiSystem::Get().RenderOnly(vWidgetDescs);
            }
        }

        bool SceneHasActiveUiWidgets(const CScene& scene)
        {
            for (const CEntity& entity : scene.m_vEntities)
            {
                const CUiComponent* pUi = entity.GetUiComponent();
                if (pUi && pUi->m_Enabled)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsPointOverSceneGui(CScene& scene, Vec2 mousePosInViewport)
        {
            std::vector<SGuiWidgetDesc*> vWidgetDescs;
            for (CEntity& entity : scene.m_vEntities)
            {
                CUiComponent* pUi = entity.GetUiComponent();
                if (!pUi || !pUi->m_Enabled)
                {
                    continue;
                }
                CComponentManager* pComponents = entity.GetComponents();
                pUi->SyncToRuntimeDesc(
                    pComponents->GetComponentOfType<CUiRectTransformComponent>().get(),
                    pComponents->GetComponentOfType<CTextComponent>().get());
                vWidgetDescs.push_back(&pUi->m_RuntimeDesc);
            }
            if (vWidgetDescs.empty())
            {
                return false;
            }
            return CGuiSystem::Get().IsPointOverAnyWidget(vWidgetDescs, mousePosInViewport);
        }

    } // gui
} // quark
