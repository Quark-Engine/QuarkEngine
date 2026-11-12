#include "gui/gui_renderer_bridge.h"

#include "component.h"
#include "entity.h"
#include "scene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace quark
{
    namespace gui
    {
        namespace
        {
            struct SCachedFontEntry
            {
                Font m_Font = {};
                bool m_Valid = false;
            };

            struct SResolvedFontInfo
            {
                std::string m_StyledPath;
                bool m_FoundBold = false;
                bool m_FoundItalic = false;
            };

            static Font s_GuiFont = {};
            static bool s_GuiFontLoaded = false;
            static bool s_GuiFontAttempted = false;
            static std::unordered_map<std::string, SResolvedFontInfo> s_FontResolutionCache;
            static std::string s_DefaultFontPath;
            static std::unordered_map<std::string, SCachedFontEntry> s_FontCache;
            static std::unordered_map<std::string, Color> s_MaterialTintCache;
            static const Font* s_pActiveFont = nullptr;
            static bool s_ActiveSyntheticBold = false;
            static Color s_ActiveMaterialTint = { 255, 255, 255, 255 };
            static bool s_BackendInstalled = false;
            static const Camera2D* s_pGuiScissorCamera2D = nullptr;

            const std::vector<int>& GetGuiCodepoints()
            {
                static std::vector<int> s_vCodepoints;
                if (s_vCodepoints.empty())
                {
                    s_vCodepoints.reserve(450);
                    for (int cp = 32; cp <= 126; ++cp)
                    {
                        s_vCodepoints.push_back(cp);
                    }
                    for (int cp = 160; cp <= 255; ++cp)
                    {
                        s_vCodepoints.push_back(cp);
                    }
                    for (int cp = 0x0400; cp <= 0x04FF; ++cp)
                    {
                        s_vCodepoints.push_back(cp);
                    }
                }
                return s_vCodepoints;
            }

            std::string ToLowerCopy(const std::string& input)
            {
                std::string out = input;
                for (char& ch : out)
                {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                return out;
            }

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

                s_DefaultFontPath = foundPath;
                const std::vector<int>& vCodepoints = GetGuiCodepoints();
                s_GuiFont = LoadFontEx(foundPath.c_str(), 48, vCodepoints.data(), static_cast<int>(vCodepoints.size()));
                if (s_GuiFont.texture.id != 0 && s_GuiFont.glyphCount > 0)
                {
                    SetTextureFilter(s_GuiFont.texture, TEXTURE_FILTER_BILINEAR);
                    s_GuiFontLoaded = true;
                }
            }

            std::string ResolveBaseFontPath(const std::string& requestedFont)
            {
                EnsureGuiFontLoaded();
                const std::string lowerReq = ToLowerCopy(requestedFont);
                if (requestedFont.empty() || lowerReq == "default" || lowerReq == "default (rubik)" || lowerReq == "rubik-regular")
                {
                    return s_DefaultFontPath;
                }

                std::error_code ec;
                if (std::filesystem::exists(requestedFont, ec) && !ec)
                {
                    return requestedFont;
                }

                const char* aSearchPrefixes[] = {
                    "assets/font/",
                    "assets/fonts/",
                    "resources/",
                    "font/",
                    "../assets/font/",
                    "../../assets/font/"
                };
                for (const char* pPrefix : aSearchPrefixes)
                {
                    const std::string candidate = std::string(pPrefix) + requestedFont;
                    if (std::filesystem::exists(candidate, ec) && !ec)
                    {
                        return candidate;
                    }
                    const std::string ttfCandidate = candidate + ".ttf";
                    if (std::filesystem::exists(ttfCandidate, ec) && !ec)
                    {
                        return ttfCandidate;
                    }
                    const std::string otfCandidate = candidate + ".otf";
                    if (std::filesystem::exists(otfCandidate, ec) && !ec)
                    {
                        return otfCandidate;
                    }
                }

#ifdef _WIN32
                char* pWinDir = nullptr;
                size_t winDirLen = 0;
                _dupenv_s(&pWinDir, &winDirLen, "WINDIR");
                const std::string winFontsDir = (pWinDir && winDirLen > 0) ? (std::string(pWinDir) + "\\Fonts\\") : "C:\\Windows\\Fonts\\";
                if (pWinDir)
                {
                    free(pWinDir);
                }
                const std::string winCandidate = winFontsDir + requestedFont;
                if (std::filesystem::exists(winCandidate, ec) && !ec)
                {
                    return winCandidate;
                }
                if (std::filesystem::exists(winCandidate + ".ttf", ec) && !ec)
                {
                    return winCandidate + ".ttf";
                }
                if (std::filesystem::exists(winCandidate + ".otf", ec) && !ec)
                {
                    return winCandidate + ".otf";
                }
#endif

                return s_DefaultFontPath;
            }

            std::string FindStyledSiblingFontPath(const std::string& basePath, int fontStyle, bool& outFoundBold, bool& outFoundItalic)
            {
                outFoundBold = false;
                outFoundItalic = false;
                if (basePath.empty() || fontStyle <= 0)
                {
                    return basePath;
                }

                const std::filesystem::path fsBase(basePath);
                const std::filesystem::path parentDir = fsBase.parent_path();
                const std::string ext = fsBase.extension().string();
                std::string stem = fsBase.stem().string();
                const std::string lowerStem = ToLowerCopy(stem);
                if (lowerStem.size() > 8 && lowerStem.substr(lowerStem.size() - 8) == "-regular")
                {
                    stem = stem.substr(0, stem.size() - 8);
                }

                auto tryCandidates = [&](const std::vector<std::string>& vSuffixes) -> std::string
                {
                    std::error_code ec;
                    for (const std::string& suffix : vSuffixes)
                    {
                        const std::filesystem::path candidate = parentDir / (stem + suffix + ext);
                        if (std::filesystem::exists(candidate, ec) && !ec)
                        {
                            return candidate.string();
                        }
                    }
                    return {};
                };

                if (fontStyle == 3)
                {
                    const std::string bi = tryCandidates({ "-BoldItalic", "bi", "z", "_bi" });
                    if (!bi.empty())
                    {
                        outFoundBold = true;
                        outFoundItalic = true;
                        return bi;
                    }
                    const std::string italicOnly = tryCandidates({ "-Italic", "-Oblique", "i", "_i" });
                    if (!italicOnly.empty())
                    {
                        outFoundItalic = true;
                        return italicOnly;
                    }
                    const std::string boldOnly = tryCandidates({ "-Bold", "bd", "b", "_b" });
                    if (!boldOnly.empty())
                    {
                        outFoundBold = true;
                        return boldOnly;
                    }
                }
                else if (fontStyle == 1)
                {
                    const std::string boldOnly = tryCandidates({ "-Bold", "bd", "b", "_b" });
                    if (!boldOnly.empty())
                    {
                        outFoundBold = true;
                        return boldOnly;
                    }
                }
                else if (fontStyle == 2)
                {
                    const std::string italicOnly = tryCandidates({ "-Italic", "-Oblique", "i", "_i" });
                    if (!italicOnly.empty())
                    {
                        outFoundItalic = true;
                        return italicOnly;
                    }
                }
                return basePath;
            }

            const Font* GetOrLoadCachedFont(const std::string& cacheKey)
            {
                if (cacheKey.empty() || !IsWindowReady())
                {
                    return s_GuiFontLoaded ? &s_GuiFont : nullptr;
                }
                if (cacheKey == s_DefaultFontPath && s_GuiFontLoaded)
                {
                    return &s_GuiFont;
                }

                auto it = s_FontCache.find(cacheKey);
                if (it != s_FontCache.end())
                {
                    return it->second.m_Valid ? &it->second.m_Font : (s_GuiFontLoaded ? &s_GuiFont : nullptr);
                }

                const std::vector<int>& vCodepoints = GetGuiCodepoints();
                SCachedFontEntry entry;
                entry.m_Font = LoadFontEx(cacheKey.c_str(), 48, vCodepoints.data(), static_cast<int>(vCodepoints.size()));
                if (entry.m_Font.texture.id != 0 && entry.m_Font.glyphCount > 0)
                {
                    SetTextureFilter(entry.m_Font.texture, TEXTURE_FILTER_BILINEAR);
                    entry.m_Valid = true;
                }
                auto inserted = s_FontCache.emplace(cacheKey, entry);
                return inserted.first->second.m_Valid ? &inserted.first->second.m_Font : (s_GuiFontLoaded ? &s_GuiFont : nullptr);
            }

            Color ResolveMaterialTint(const std::string& materialPath)
            {
                if (materialPath.empty() || materialPath == "None" || materialPath == "None (Material)")
                {
                    return { 255, 255, 255, 255 };
                }

                auto it = s_MaterialTintCache.find(materialPath);
                if (it != s_MaterialTintCache.end())
                {
                    return it->second;
                }

                Color tint = { 255, 255, 255, 255 };
                std::error_code ec;
                if (std::filesystem::exists(materialPath, ec) && !ec)
                {
                    std::ifstream file(materialPath);
                    std::string line;
                    float kdR = 1.0f;
                    float kdG = 1.0f;
                    float kdB = 1.0f;
                    float alpha = 1.0f;
                    while (std::getline(file, line))
                    {
                        std::istringstream iss(line);
                        std::string token;
                        if (!(iss >> token))
                        {
                            continue;
                        }
                        if (token == "Kd")
                        {
                            iss >> kdR >> kdG >> kdB;
                        }
                        else if (token == "d")
                        {
                            iss >> alpha;
                        }
                    }
                    tint.r = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(kdR * 255.0f)), 0, 255));
                    tint.g = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(kdG * 255.0f)), 0, 255));
                    tint.b = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(kdB * 255.0f)), 0, 255));
                    tint.a = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(alpha * 255.0f)), 0, 255));
                }

                s_MaterialTintCache.emplace(materialPath, tint);
                return tint;
            }

            void BridgeSetTextContext(const char* pFontPath, int fontStyle, const char* pMaterialPath)
            {
                EnsureGuiFontLoaded();
                const std::string fontKey = (pFontPath ? pFontPath : "") + std::string("#") + std::to_string(fontStyle);
                auto resIt = s_FontResolutionCache.find(fontKey);
                if (resIt == s_FontResolutionCache.end())
                {
                    const std::string basePath = ResolveBaseFontPath(pFontPath ? pFontPath : "");
                    bool foundBold = false;
                    bool foundItalic = false;
                    std::string styledPath = FindStyledSiblingFontPath(basePath, fontStyle, foundBold, foundItalic);
                    resIt = s_FontResolutionCache.emplace(fontKey, SResolvedFontInfo{ std::move(styledPath), foundBold, foundItalic }).first;
                }

                const std::string& styledPath = resIt->second.m_StyledPath;
                const bool foundBold = resIt->second.m_FoundBold;
                const bool foundItalic = resIt->second.m_FoundItalic;

                const bool wantsBold = (fontStyle == 1 || fontStyle == 3);
                const bool wantsItalic = (fontStyle == 2 || fontStyle == 3);
                s_ActiveSyntheticBold = wantsBold && !foundBold;

                if (wantsItalic && !foundItalic && !styledPath.empty())
                {
                    const Font* pItalicFont = GetOrLoadCachedFont(styledPath + "|italic");
                    if (pItalicFont != nullptr && pItalicFont != &s_GuiFont)
                    {
                        s_pActiveFont = pItalicFont;
                    }
                    else
                    {
                        s_pActiveFont = GetOrLoadCachedFont(styledPath);
                    }
                }
                else
                {
                    s_pActiveFont = GetOrLoadCachedFont(styledPath);
                }

                s_ActiveMaterialTint = ResolveMaterialTint(pMaterialPath ? pMaterialPath : "");
            }

            void BridgeDrawText(const char* text, float x, float y, float fontSize, float lineSpacing, Color color)
            {
                (void)lineSpacing;
                if (!text || !text[0])
                {
                    return;
                }
                EnsureGuiFontLoaded();
                const Color finalColor = {
                    static_cast<unsigned char>((static_cast<int>(color.r) * static_cast<int>(s_ActiveMaterialTint.r)) / 255),
                    static_cast<unsigned char>((static_cast<int>(color.g) * static_cast<int>(s_ActiveMaterialTint.g)) / 255),
                    static_cast<unsigned char>((static_cast<int>(color.b) * static_cast<int>(s_ActiveMaterialTint.b)) / 255),
                    static_cast<unsigned char>((static_cast<int>(color.a) * static_cast<int>(s_ActiveMaterialTint.a)) / 255)
                };

                const Font* pFontToUse = (s_pActiveFont && s_pActiveFont->texture.id != 0)
                    ? s_pActiveFont
                    : ((s_GuiFontLoaded && s_GuiFont.texture.id != 0) ? &s_GuiFont : nullptr);

                if (pFontToUse != nullptr)
                {
                    DrawTextEx(*pFontToUse, text, { x, y }, fontSize, 1.0f, finalColor);
                    if (s_ActiveSyntheticBold)
                    {
                        const float boldOffset = std::max(1.0f, std::round(fontSize * 0.045f));
                        DrawTextEx(*pFontToUse, text, { x + boldOffset, y }, fontSize, 1.0f, finalColor);
                    }
                }
                else
                {
                    DrawText(text, static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), static_cast<int>(std::round(fontSize)), finalColor);
                    if (s_ActiveSyntheticBold)
                    {
                        DrawText(text, static_cast<int>(std::round(x)) + 1, static_cast<int>(std::round(y)), static_cast<int>(std::round(fontSize)), finalColor);
                    }
                }
            }

            Vec2 BridgeMeasureText(const char* text, float fontSize)
            {
                if (!text || !text[0])
                {
                    return { 0.0f, fontSize };
                }
                EnsureGuiFontLoaded();
                const Font* pFontToUse = (s_pActiveFont && s_pActiveFont->texture.id != 0)
                    ? s_pActiveFont
                    : ((s_GuiFontLoaded && s_GuiFont.texture.id != 0) ? &s_GuiFont : nullptr);

                if (pFontToUse != nullptr)
                {
                    Vec2 measured = MeasureTextEx(*pFontToUse, text, fontSize, 1.0f);
                    if (s_ActiveSyntheticBold)
                    {
                        measured.x += std::max(1.0f, std::round(fontSize * 0.045f));
                    }
                    return measured;
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
            backend.pfnSetTextContext = &BridgeSetTextContext;
            backend.pfnGetTime = &BridgeGetTime;

            CGuiSystem::Get().SetBackend(backend);
            s_BackendInstalled = true;
        }

        void ShutdownGuiBackend()
        {
            if (IsWindowReady())
            {
                for (auto& pair : s_FontCache)
                {
                    if (pair.second.m_Valid && pair.second.m_Font.texture.id != 0)
                    {
                        UnloadFont(pair.second.m_Font);
                    }
                }
                if (s_GuiFontLoaded)
                {
                    UnloadFont(s_GuiFont);
                }
            }
            s_FontCache.clear();
            s_FontResolutionCache.clear();
            s_MaterialTintCache.clear();
            s_pActiveFont = nullptr;
            s_ActiveSyntheticBold = false;
            s_ActiveMaterialTint = { 255, 255, 255, 255 };
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
                const CCanvasComponent* pCanvas = entity.GetCanvasComponent();
                if (pCanvas && pCanvas->m_Enabled)
                {
                    return true;
                }
                const CButtonComponent* pBtn = entity.GetButtonComponent();
                if (pBtn && pBtn->m_Enabled)
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

        void DrawUiText(const char* text, float x, float y, float fontSize,
                        const char* pFontPath, int fontStyle, Color color)
        {
            BridgeSetTextContext(pFontPath, fontStyle, nullptr);
            BridgeDrawText(text, x, y, fontSize, 1.0f, color);
        }

        Vec2 MeasureUiText(const char* text, float fontSize,
                           const char* pFontPath, int fontStyle)
        {
            BridgeSetTextContext(pFontPath, fontStyle, nullptr);
            return BridgeMeasureText(text, fontSize);
        }
    } // gui
} // quark
