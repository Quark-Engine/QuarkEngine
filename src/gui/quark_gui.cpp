#include "QuarkGUI/QuarkGUI.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace quark
{
    namespace gui
    {

        namespace
        {

            bool PointInRect(Vec2 p, Rectangle r)
            {
                return p.x >= r.x && p.x <= (r.x + r.width) &&
                    p.y >= r.y && p.y <= (r.y + r.height);
            }

            Rectangle ShrinkRect(Rectangle r, float amount)
            {
                Rectangle out;
                out.x = r.x + amount;
                out.y = r.y + amount;
                out.width = std::max(0.0f, r.width - amount * 2.0f);
                out.height = std::max(0.0f, r.height - amount * 2.0f);
                return out;
            }

            const SGuiTheme& DefaultTheme()
            {
                static const SGuiTheme theme;
                return theme;
            }

            float AlignTextX(Rectangle rect, Vec2 textSize, int alignment, float padding)
            {
                if (alignment == 1)
                {
                    return rect.x + (rect.width - textSize.x) * 0.5f;
                }
                if (alignment == 2)
                {
                    return rect.x + rect.width - textSize.x - padding;
                }
                return rect.x + padding;
            }

            float AlignTextY(Rectangle rect, Vec2 textSize, int alignment)
            {
                if (alignment == 0)
                {
                    return rect.y;
                }
                if (alignment == 2)
                {
                    return rect.y + rect.height - textSize.y;
                }
                return rect.y + (rect.height - textSize.y) * 0.5f;
            }

            Color MultiplyColor(Color color, float multiplier)
            {
                const auto scaleChannel = [multiplier](unsigned char channel)
                {
                    return static_cast<unsigned char>(std::clamp(
                        static_cast<int>(std::lround(channel * multiplier)), 0, 255));
                };
                return {
                    scaleChannel(color.r),
                    scaleChannel(color.g),
                    scaleChannel(color.b),
                    scaleChannel(color.a)
                };
            }

            bool IsUtf8ContinuationByte(unsigned char c)
            {
                return (c & 0xC0) == 0x80;
            }

            int PrevUtf8CharPos(const std::string& str, int pos)
            {
                int i = std::clamp(pos, 0, static_cast<int>(str.size()));
                if (i <= 0)
                {
                    return 0;
                }
                --i;
                while (i > 0 && IsUtf8ContinuationByte(static_cast<unsigned char>(str[i])))
                {
                    --i;
                }
                return i;
            }

            int NextUtf8CharPos(const std::string& str, int pos)
            {
                const int n = static_cast<int>(str.size());
                int i = std::clamp(pos, 0, n);
                if (i >= n)
                {
                    return n;
                }
                ++i;
                while (i < n && IsUtf8ContinuationByte(static_cast<unsigned char>(str[i])))
                {
                    ++i;
                }
                return i;
            }

            std::string EncodeUtf8Codepoint(uint32_t cp)
            {
                std::string out;
                if (cp < 0x80)
                {
                    out.push_back(static_cast<char>(cp));
                }
                else if (cp < 0x800)
                {
                    out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
                else if (cp < 0x10000)
                {
                    out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
                else if (cp <= 0x10FFFF)
                {
                    out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
                return out;
            }

        } // anonymous

        CGuiSystem& CGuiSystem::Get()
        {
            static CGuiSystem s_Instance;
            return s_Instance;
        }

        CGuiSystem::CGuiSystem() = default;
        CGuiSystem::~CGuiSystem() = default;

        void CGuiSystem::SetBackend(const SGuiBackend& backend)
        {
            m_Backend = backend;
        }

        const SGuiBackend& CGuiSystem::GetBackend() const
        {
            return m_Backend;
        }

        void CGuiSystem::DrawStyledFrame(Rectangle rect, Color bgColor, Color innerBorderColor, const SGuiTheme& theme) const
        {
            if (rect.width <= 0.0f || rect.height <= 0.0f)
            {
                return;
            }

            const Rectangle innerBorderRect = ShrinkRect(rect, 1.0f);
            const Rectangle fillRect = ShrinkRect(rect, 2.0f);

            if (m_Backend.pfnDrawRectangle)
            {
                m_Backend.pfnDrawRectangle(
                    static_cast<int>(std::round(rect.x)),
                    static_cast<int>(std::round(rect.y)),
                    static_cast<int>(std::round(rect.width)),
                    static_cast<int>(std::round(rect.height)),
                    theme.m_OuterBorderColor);
                if (innerBorderRect.width > 0.0f && innerBorderRect.height > 0.0f)
                {
                    m_Backend.pfnDrawRectangle(
                        static_cast<int>(std::round(innerBorderRect.x)),
                        static_cast<int>(std::round(innerBorderRect.y)),
                        static_cast<int>(std::round(innerBorderRect.width)),
                        static_cast<int>(std::round(innerBorderRect.height)),
                        innerBorderColor);
                }
                if (fillRect.width > 0.0f && fillRect.height > 0.0f)
                {
                    const int gradientSteps = std::max(1, static_cast<int>(std::ceil(fillRect.height)));
                    const auto adjustColor = [](unsigned char channel, int offset)
                    {
                        return static_cast<unsigned char>(std::clamp(static_cast<int>(channel) + offset, 0, 255));
                    };
                    const Color topColor = {
                        adjustColor(bgColor.r, 8),
                        adjustColor(bgColor.g, 8),
                        adjustColor(bgColor.b, 8),
                        bgColor.a
                    };
                    const Color bottomColor = {
                        adjustColor(bgColor.r, -8),
                        adjustColor(bgColor.g, -8),
                        adjustColor(bgColor.b, -8),
                        bgColor.a
                    };

                    for (int step = 0; step < gradientSteps; ++step)
                    {
                        const float t = (static_cast<float>(step) + 0.5f) / gradientSteps;
                        const Color color = {
                            static_cast<unsigned char>(std::lround(topColor.r + (bottomColor.r - topColor.r) * t)),
                            static_cast<unsigned char>(std::lround(topColor.g + (bottomColor.g - topColor.g) * t)),
                            static_cast<unsigned char>(std::lround(topColor.b + (bottomColor.b - topColor.b) * t)),
                            bgColor.a
                        };
                        const float stripTop = fillRect.y + fillRect.height * step / gradientSteps;
                        const float stripBottom = fillRect.y + fillRect.height * (step + 1) / gradientSteps;
                        m_Backend.pfnDrawRectangle(
                            static_cast<int>(std::round(fillRect.x)),
                            static_cast<int>(std::floor(stripTop)),
                            static_cast<int>(std::round(fillRect.width)),
                            std::max(1, static_cast<int>(std::ceil(stripBottom) - std::floor(stripTop))),
                            color);
                    }
                }
            }
        }

        void CGuiSystem::DrawDownArrow(Vec2 center, float halfSize, Color color) const
        {
            if (!m_Backend.pfnDrawTriangle)
            {
                return;
            }
            const Vec2 topLeft = { center.x - halfSize, center.y - halfSize * 0.55f };
            const Vec2 topRight = { center.x + halfSize, center.y - halfSize * 0.55f };
            const Vec2 bottom = { center.x, center.y + halfSize * 0.65f };
            m_Backend.pfnDrawTriangle(topLeft, bottom, topRight, color);
            m_Backend.pfnDrawTriangle(topLeft, topRight, bottom, color);
        }

        void CGuiSystem::DrawUpArrow(Vec2 center, float halfSize, Color color) const
        {
            if (!m_Backend.pfnDrawTriangle)
            {
                return;
            }
            const Vec2 bottomLeft = { center.x - halfSize, center.y + halfSize * 0.55f };
            const Vec2 bottomRight = { center.x + halfSize, center.y + halfSize * 0.55f };
            const Vec2 top = { center.x, center.y - halfSize * 0.65f };
            m_Backend.pfnDrawTriangle(top, bottomLeft, bottomRight, color);
            m_Backend.pfnDrawTriangle(top, bottomRight, bottomLeft, color);
        }

        Rectangle CGuiSystem::GetComboPopupRect(const SGuiWidgetDesc& widget, const SGuiTheme& theme) const
        {
            const int itemCount = std::max(1, static_cast<int>(widget.m_vItems.size()));
            const float itemH = std::max(20.0f, theme.m_ComboItemHeight);
            const float popupHeight = static_cast<float>(itemCount) * itemH + 4.0f;
            return Rectangle{
                widget.m_Position.x,
                widget.m_Position.y + widget.m_Size.y + 2.0f,
                widget.m_Size.x,
                popupHeight
            };
        }

        void CGuiSystem::HandleTextInput(SGuiWidgetDesc& widget)
        {
            widget.m_CaretPos = std::clamp(widget.m_CaretPos, 0, static_cast<int>(widget.m_Text.size()));

            for (uint32_t cp : m_Input.m_vTypedCodepoints)
            {
                if (cp < 32 || cp == 127)
                {
                    continue;
                }
                const std::string encoded = EncodeUtf8Codepoint(cp);
                widget.m_Text.insert(static_cast<size_t>(widget.m_CaretPos), encoded);
                widget.m_CaretPos += static_cast<int>(encoded.size());
                widget.m_ValueChanged = true;
            }

            if (m_Input.m_KeyBackspacePressed && widget.m_CaretPos > 0)
            {
                const int prevPos = PrevUtf8CharPos(widget.m_Text, widget.m_CaretPos);
                widget.m_Text.erase(static_cast<size_t>(prevPos), static_cast<size_t>(widget.m_CaretPos - prevPos));
                widget.m_CaretPos = prevPos;
                widget.m_ValueChanged = true;
            }

            if (m_Input.m_KeyDeletePressed && widget.m_CaretPos < static_cast<int>(widget.m_Text.size()))
            {
                const int nextPos = NextUtf8CharPos(widget.m_Text, widget.m_CaretPos);
                widget.m_Text.erase(static_cast<size_t>(widget.m_CaretPos), static_cast<size_t>(nextPos - widget.m_CaretPos));
                widget.m_ValueChanged = true;
            }

            if (m_Input.m_KeyLeftPressed)
            {
                widget.m_CaretPos = PrevUtf8CharPos(widget.m_Text, widget.m_CaretPos);
            }
            if (m_Input.m_KeyRightPressed)
            {
                widget.m_CaretPos = NextUtf8CharPos(widget.m_Text, widget.m_CaretPos);
            }
            if (m_Input.m_KeyHomePressed)
            {
                widget.m_CaretPos = 0;
            }
            if (m_Input.m_KeyEndPressed)
            {
                widget.m_CaretPos = static_cast<int>(widget.m_Text.size());
            }
            if (m_Input.m_KeyEnterPressed || m_Input.m_KeyEscapePressed)
            {
                widget.m_Focused = false;
                if (m_FocusedWidgetId == widget.m_WidgetId)
                {
                    m_FocusedWidgetId = 0;
                }
            }
        }

        void CGuiSystem::BeginFrame(const SGuiInputState& input, bool interactive)
        {
            m_Input = input;
            m_Interactive = interactive;
            m_PopupConsumedClick = false;
            m_WantsMouseCapture = false;
            m_WantsKeyboardCapture = false;
            m_vFrameWidgets.clear();

            if (!m_Interactive)
            {
                m_ActiveWidgetId = 0;
                m_FocusedWidgetId = 0;
                m_OpenComboWidgetId = 0;
            }
        }

        void CGuiSystem::ProcessWidget(SGuiWidgetDesc& widget)
        {
            m_vFrameWidgets.push_back(&widget);

            const SGuiTheme& theme = DefaultTheme();
            const Rectangle rect = {
                widget.m_Position.x,
                widget.m_Position.y,
                std::max(16.0f, widget.m_Size.x),
                std::max(16.0f, widget.m_Size.y)
            };

            widget.m_Clicked = false;
            widget.m_ValueChanged = false;

            const bool mouseOverRect = m_Interactive &&
                (widget.m_WidgetType != EGuiWidgetType::BUTTON || widget.m_ButtonInteractable) &&
                !m_PopupConsumedClick &&
                PointInRect(m_Input.m_MousePosition, rect);
            widget.m_Hovered = mouseOverRect;
            if (mouseOverRect)
            {
                m_WantsMouseCapture = true;
            }

            if (!m_Interactive)
            {
                widget.m_Pressed = false;
                widget.m_Focused = false;
                widget.m_ComboOpen = false;
                widget.m_HoveredItemIndex = -1;
            }

            const float fontSize = std::clamp(widget.m_TextFontSize, 1.0f, 256.0f);
            const float lineSpacing = std::clamp(widget.m_TextLineSpacing, 0.1f, 10.0f);

            auto measureText = [&](const std::string& s) -> Vec2
            {
                if (m_Backend.pfnMeasureText && !s.empty())
                {
                    return m_Backend.pfnMeasureText(s.c_str(), fontSize);
                }
                return { static_cast<float>(s.size()) * (fontSize * 0.52f), fontSize };
            };

            auto drawClippedText = [&](const std::string& s, float x, float y, Color color, Rectangle clipRect)
            {
                if (s.empty() || !m_Backend.pfnDrawText)
                {
                    return;
                }
                if (m_Backend.pfnBeginScissorMode && m_Backend.pfnEndScissorMode && clipRect.width > 1.0f && clipRect.height > 1.0f)
                {
                    m_Backend.pfnBeginScissorMode(
                        static_cast<int>(std::floor(clipRect.x)),
                        static_cast<int>(std::floor(clipRect.y)),
                        static_cast<int>(std::ceil(clipRect.width)),
                        static_cast<int>(std::ceil(clipRect.height)));
                    m_Backend.pfnDrawText(s.c_str(), std::round(x), std::round(y), fontSize, lineSpacing, color);
                    m_Backend.pfnEndScissorMode();
                }
                else
                {
                    m_Backend.pfnDrawText(s.c_str(), std::round(x), std::round(y), fontSize, lineSpacing, color);
                }
            };

            switch (widget.m_WidgetType)
            {
                case EGuiWidgetType::BUTTON:
                {
                    if (m_Interactive && widget.m_ButtonInteractable)
                    {
                        if (mouseOverRect && m_Input.m_MousePressed)
                        {
                            m_ActiveWidgetId = widget.m_WidgetId;
                            m_FocusedWidgetId = 0;
                            m_OpenComboWidgetId = 0;
                        }
                        widget.m_Pressed = (m_ActiveWidgetId == widget.m_WidgetId) && m_Input.m_MouseDown && mouseOverRect;
                        if (m_ActiveWidgetId == widget.m_WidgetId && m_Input.m_MouseReleased)
                        {
                            if (mouseOverRect)
                            {
                                widget.m_Clicked = true;
                                widget.m_ValueChanged = true;
                                ++widget.m_ClickCount;
                            }
                            m_ActiveWidgetId = 0;
                            widget.m_Pressed = false;
                        }
                    }
                    else if (m_ActiveWidgetId == widget.m_WidgetId)
                    {
                        m_ActiveWidgetId = 0;
                        widget.m_Pressed = false;
                    }

                    Color bg = theme.m_ButtonColor;
                    Color border = theme.m_BorderColor;
                    if (widget.m_ButtonTransition == EGuiButtonTransition::COLOR_TINT)
                    {
                        if (!widget.m_ButtonInteractable)
                        {
                            bg = widget.m_ButtonDisabledColor;
                        }
                        else if (widget.m_Pressed)
                        {
                            bg = widget.m_ButtonPressedColor;
                            border = theme.m_BorderFocusedColor;
                        }
                        else if (widget.m_Hovered)
                        {
                            bg = widget.m_ButtonHighlightedColor;
                            border = theme.m_BorderHoveredColor;
                        }
                        else
                        {
                            bg = widget.m_ButtonNormalColor;
                        }
                        bg = MultiplyColor(bg, std::clamp(widget.m_ButtonColorMultiplier, 0.0f, 10.0f));
                    }
                    else if (widget.m_Pressed)
                    {
                        border = theme.m_BorderFocusedColor;
                    }
                    else if (widget.m_Hovered)
                    {
                        border = theme.m_BorderHoveredColor;
                    }

                    DrawStyledFrame(rect, bg, border, theme);
                    if (widget.m_ShowNavigationVisualization && m_Backend.pfnDrawLineEx)
                    {
                        const Color visualizationColor = { 80, 170, 255, 255 };
                        const Vec2 topLeft = { rect.x, rect.y };
                        const Vec2 topRight = { rect.x + rect.width, rect.y };
                        const Vec2 bottomRight = { rect.x + rect.width, rect.y + rect.height };
                        const Vec2 bottomLeft = { rect.x, rect.y + rect.height };
                        m_Backend.pfnDrawLineEx(topLeft, topRight, 2.0f, visualizationColor);
                        m_Backend.pfnDrawLineEx(topRight, bottomRight, 2.0f, visualizationColor);
                        m_Backend.pfnDrawLineEx(bottomRight, bottomLeft, 2.0f, visualizationColor);
                        m_Backend.pfnDrawLineEx(bottomLeft, topLeft, 2.0f, visualizationColor);
                    }

                    const std::string& label = widget.m_Text.empty() ? std::string("Button") : widget.m_Text;
                    const Vec2 textSize = measureText(label);
                    const float textX = AlignTextX(rect, textSize, widget.m_TextHorizontalAlignment, theme.m_PaddingX);
                    const float textY = AlignTextY(rect, textSize, widget.m_TextVerticalAlignment);
                    drawClippedText(label, textX, textY, widget.m_TextColor, ShrinkRect(rect, 3.0f));
                    break;
                }

                case EGuiWidgetType::INPUT_FIELD:
                {
                    if (m_Interactive)
                    {
                        if (m_Input.m_MousePressed && !m_PopupConsumedClick)
                        {
                            if (mouseOverRect)
                            {
                                m_FocusedWidgetId = widget.m_WidgetId;
                                m_OpenComboWidgetId = 0;
                                widget.m_Focused = true;
                                widget.m_Clicked = true;
                                widget.m_CaretPos = static_cast<int>(widget.m_Text.size());
                            }
                            else if (m_FocusedWidgetId == widget.m_WidgetId)
                            {
                                m_FocusedWidgetId = 0;
                                widget.m_Focused = false;
                            }
                        }
                        widget.m_Focused = (m_FocusedWidgetId == widget.m_WidgetId);
                        if (widget.m_Focused)
                        {
                            m_WantsKeyboardCapture = true;
                            m_WantsMouseCapture = true;
                            HandleTextInput(widget);
                        }
                    }

                    Color bg = theme.m_InputBgColor;
                    Color border = theme.m_BorderColor;
                    if (widget.m_Focused)
                    {
                        bg = theme.m_InputActiveColor;
                        border = theme.m_BorderFocusedColor;
                    }
                    else if (widget.m_Hovered)
                    {
                        bg = theme.m_InputHoveredColor;
                        border = theme.m_BorderHoveredColor;
                    }

                    DrawStyledFrame(rect, bg, border, theme);

                    const Rectangle contentRect = {
                        rect.x + theme.m_PaddingX,
                        rect.y + 3.0f,
                        std::max(1.0f, rect.width - theme.m_PaddingX * 2.0f),
                        std::max(1.0f, rect.height - 6.0f)
                    };

                    const bool showPlaceholder = widget.m_Text.empty() && !widget.m_Focused;
                    const std::string& displayStr = showPlaceholder
                        ? (widget.m_Placeholder.empty() ? std::string("InputField") : widget.m_Placeholder)
                        : widget.m_Text;
                    const Color textColor = showPlaceholder ? theme.m_PlaceholderColor : widget.m_TextColor;

                    const Vec2 fullSize = measureText(displayStr.empty() ? std::string("Ag") : displayStr);
                    const float textY = AlignTextY(rect, fullSize, widget.m_TextVerticalAlignment);

                    float scrollOffsetX = 0.0f;
                    const int safeCaret = std::clamp(widget.m_CaretPos, 0, static_cast<int>(widget.m_Text.size()));
                    const std::string textBeforeCaret = widget.m_Text.substr(0, static_cast<size_t>(safeCaret));
                    const float caretOffsetPx = widget.m_Text.empty() ? 0.0f : measureText(textBeforeCaret).x;
                    if (caretOffsetPx > contentRect.width - 6.0f)
                    {
                        scrollOffsetX = caretOffsetPx - (contentRect.width - 6.0f);
                    }

                    const float alignedTextX = AlignTextX(
                        contentRect,
                        fullSize,
                        widget.m_TextHorizontalAlignment,
                        0.0f);
                    const float textX = alignedTextX - scrollOffsetX;
                    drawClippedText(displayStr, textX, textY, textColor, contentRect);

                    if (widget.m_Focused && m_Backend.pfnDrawLineEx)
                    {
                        const double now = m_Backend.pfnGetTime ? m_Backend.pfnGetTime() : 0.0;
                        const bool caretVisible = (std::fmod(now, 1.0) < 0.55);
                        if (caretVisible)
                        {
                            const float caretX = std::clamp(textX + caretOffsetPx + 1.0f, contentRect.x, contentRect.x + contentRect.width);
                            const float caretTop = rect.y + 6.0f;
                            const float caretBottom = rect.y + rect.height - 6.0f;
                            m_Backend.pfnDrawLineEx({ caretX, caretTop }, { caretX, caretBottom }, 1.5f, widget.m_TextColor);
                        }
                    }
                    break;
                }

                case EGuiWidgetType::COMBO:
                {
                    if (m_Interactive)
                    {
                        if (mouseOverRect && m_Input.m_MousePressed)
                        {
                            widget.m_Clicked = true;
                            m_FocusedWidgetId = 0;
                            if (m_OpenComboWidgetId == widget.m_WidgetId)
                            {
                                m_OpenComboWidgetId = 0;
                                widget.m_ComboOpen = false;
                            }
                            else
                            {
                                m_OpenComboWidgetId = widget.m_WidgetId;
                                widget.m_ComboOpen = true;
                            }
                        }
                        widget.m_ComboOpen = (m_OpenComboWidgetId == widget.m_WidgetId);
                        if (widget.m_ComboOpen)
                        {
                            m_WantsMouseCapture = true;
                        }
                    }

                    Color bg = theme.m_ComboBgColor;
                    Color border = theme.m_BorderColor;
                    if (widget.m_ComboOpen)
                    {
                        bg = theme.m_ComboActiveColor;
                        border = theme.m_BorderFocusedColor;
                    }
                    else if (widget.m_Hovered)
                    {
                        bg = theme.m_ComboHoveredColor;
                        border = theme.m_BorderHoveredColor;
                    }

                    DrawStyledFrame(rect, bg, border, theme);

                    std::string selectedText = widget.m_Text.empty() ? "Combo" : widget.m_Text;
                    if (!widget.m_vItems.empty())
                    {
                        const int clampedIdx = std::clamp(widget.m_SelectedIndex, 0, static_cast<int>(widget.m_vItems.size()) - 1);
                        selectedText = widget.m_vItems[static_cast<size_t>(clampedIdx)];
                    }

                    const float arrowZoneWidth = 26.0f;
                    const Rectangle labelClip = {
                        rect.x + theme.m_PaddingX,
                        rect.y + 2.0f,
                        std::max(1.0f, rect.width - theme.m_PaddingX - arrowZoneWidth),
                        std::max(1.0f, rect.height - 4.0f)
                    };

                    const Vec2 textSize = measureText(selectedText.empty() ? std::string("Ag") : selectedText);
                    const float textX = AlignTextX(
                        labelClip,
                        textSize,
                        widget.m_TextHorizontalAlignment,
                        0.0f);
                    const float textY = AlignTextY(rect, textSize, widget.m_TextVerticalAlignment);
                    drawClippedText(selectedText, textX, textY, widget.m_TextColor, labelClip);

                    const Vec2 arrowCenter = {
                        rect.x + rect.width - arrowZoneWidth * 0.5f,
                        rect.y + rect.height * 0.5f
                    };
                    if (widget.m_ComboOpen)
                    {
                        DrawUpArrow(arrowCenter, 4.5f, theme.m_ArrowColor);
                    }
                    else
                    {
                        DrawDownArrow(arrowCenter, 4.5f, theme.m_ArrowColor);
                    }
                    break;
                }
            }
        }

        void CGuiSystem::EndFrame()
        {
            for (SGuiWidgetDesc* pWidget : m_vFrameWidgets)
            {
                if (!pWidget || pWidget->m_WidgetType != EGuiWidgetType::COMBO)
                {
                    continue;
                }
                pWidget->m_ComboOpen = (m_Interactive && m_OpenComboWidgetId == pWidget->m_WidgetId);
                if (!pWidget->m_ComboOpen)
                {
                    pWidget->m_HoveredItemIndex = -1;
                    continue;
                }

                const SGuiTheme& theme = DefaultTheme();
                const Rectangle popupRect = GetComboPopupRect(*pWidget, theme);
                const Rectangle mainRect = {
                    pWidget->m_Position.x,
                    pWidget->m_Position.y,
                    std::max(16.0f, pWidget->m_Size.x),
                    std::max(16.0f, pWidget->m_Size.y)
                };

                DrawStyledFrame(popupRect, theme.m_ComboPopupBgColor, theme.m_BorderFocusedColor, theme);

                const int itemCount = static_cast<int>(pWidget->m_vItems.size());
                const float itemH = std::max(20.0f, theme.m_ComboItemHeight);
                const float fontSize = std::clamp(pWidget->m_TextFontSize, 1.0f, 256.0f);
                const float lineSpacing = std::clamp(pWidget->m_TextLineSpacing, 0.1f, 10.0f);

                pWidget->m_HoveredItemIndex = -1;

                for (int i = 0; i < itemCount; ++i)
                {
                    const Rectangle itemRect = {
                        popupRect.x + 2.0f,
                        popupRect.y + 2.0f + static_cast<float>(i) * itemH,
                        std::max(1.0f, popupRect.width - 4.0f),
                        itemH
                    };

                    const bool itemHovered = m_Interactive && PointInRect(m_Input.m_MousePosition, itemRect);
                    if (itemHovered)
                    {
                        pWidget->m_HoveredItemIndex = i;
                        m_WantsMouseCapture = true;
                    }

                    if (itemHovered || i == pWidget->m_SelectedIndex)
                    {
                        const Color rowColor = itemHovered ? theme.m_ComboItemHoverColor : theme.m_ComboItemSelectedColor;
                        if (m_Backend.pfnDrawRectangle)
                        {
                            m_Backend.pfnDrawRectangle(
                                static_cast<int>(std::round(itemRect.x)),
                                static_cast<int>(std::round(itemRect.y)),
                                static_cast<int>(std::round(itemRect.width)),
                                static_cast<int>(std::round(itemRect.height)),
                                rowColor);
                        }
                    }

                    const std::string& itemText = pWidget->m_vItems[static_cast<size_t>(i)];
                    Vec2 tSize = { fontSize, fontSize };
                    if (m_Backend.pfnMeasureText && !itemText.empty())
                    {
                        tSize = m_Backend.pfnMeasureText(itemText.c_str(), fontSize);
                    }
                    const float textX = itemRect.x + theme.m_PaddingX;
                    const float textY = itemRect.y + (itemRect.height - tSize.y) * 0.5f;

                    if (m_Backend.pfnDrawText && !itemText.empty())
                    {
                        if (m_Backend.pfnBeginScissorMode && m_Backend.pfnEndScissorMode)
                        {
                            m_Backend.pfnBeginScissorMode(
                                static_cast<int>(std::floor(itemRect.x)),
                                static_cast<int>(std::floor(itemRect.y)),
                                static_cast<int>(std::ceil(itemRect.width)),
                                static_cast<int>(std::ceil(itemRect.height)));
                            m_Backend.pfnDrawText(itemText.c_str(), std::round(textX), std::round(textY), fontSize, lineSpacing, pWidget->m_TextColor);
                            m_Backend.pfnEndScissorMode();
                        }
                        else
                        {
                            m_Backend.pfnDrawText(itemText.c_str(), std::round(textX), std::round(textY), fontSize, lineSpacing, pWidget->m_TextColor);
                        }
                    }

                    if (itemHovered && m_Input.m_MousePressed)
                    {
                        if (pWidget->m_SelectedIndex != i)
                        {
                            pWidget->m_SelectedIndex = i;
                            pWidget->m_ValueChanged = true;
                        }
                        pWidget->m_Text = itemText;
                        pWidget->m_Clicked = true;
                        pWidget->m_ComboOpen = false;
                        m_OpenComboWidgetId = 0;
                    }
                }

                if (m_Interactive)
                {
                    const bool clickOutside = m_Input.m_MousePressed &&
                        !PointInRect(m_Input.m_MousePosition, mainRect) &&
                        !PointInRect(m_Input.m_MousePosition, popupRect);
                    if (clickOutside || m_Input.m_KeyEscapePressed)
                    {
                        pWidget->m_ComboOpen = false;
                        m_OpenComboWidgetId = 0;
                    }
                }
            }
        }

        void CGuiSystem::UpdateAndRender(std::vector<SGuiWidgetDesc*>& vWidgets, const SGuiInputState& input, bool interactive)
        {
            BeginFrame(input, interactive);

            if (interactive && m_OpenComboWidgetId != 0)
            {
                for (const SGuiWidgetDesc* pWidget : vWidgets)
                {
                    if (pWidget && pWidget->m_WidgetId == m_OpenComboWidgetId && pWidget->m_WidgetType == EGuiWidgetType::COMBO)
                    {
                        const SGuiTheme& theme = DefaultTheme();
                        const Rectangle popupRect = GetComboPopupRect(*pWidget, theme);
                        if (PointInRect(input.m_MousePosition, popupRect))
                        {
                            m_PopupConsumedClick = true;
                            m_WantsMouseCapture = true;
                        }
                        break;
                    }
                }
            }

            for (SGuiWidgetDesc* pWidget : vWidgets)
            {
                if (pWidget)
                {
                    ProcessWidget(*pWidget);
                }
            }

            EndFrame();
        }

        void CGuiSystem::RenderOnly(std::vector<SGuiWidgetDesc*>& vWidgets)
        {
            const SGuiInputState savedInput = m_Input;
            const bool savedInteractive = m_Interactive;
            const bool savedPopupConsumedClick = m_PopupConsumedClick;
            const bool savedWantsMouseCapture = m_WantsMouseCapture;
            const bool savedWantsKeyboardCapture = m_WantsKeyboardCapture;
            const uint64_t savedActiveWidgetId = m_ActiveWidgetId;
            const uint64_t savedFocusedWidgetId = m_FocusedWidgetId;
            const uint64_t savedOpenComboWidgetId = m_OpenComboWidgetId;
            std::vector<SGuiWidgetDesc*> vSavedFrameWidgets = std::move(m_vFrameWidgets);

            UpdateAndRender(vWidgets, SGuiInputState{}, false);

            m_Input = savedInput;
            m_Interactive = savedInteractive;
            m_PopupConsumedClick = savedPopupConsumedClick;
            m_WantsMouseCapture = savedWantsMouseCapture;
            m_WantsKeyboardCapture = savedWantsKeyboardCapture;
            m_ActiveWidgetId = savedActiveWidgetId;
            m_FocusedWidgetId = savedFocusedWidgetId;
            m_OpenComboWidgetId = savedOpenComboWidgetId;
            m_vFrameWidgets = std::move(vSavedFrameWidgets);
        }

        bool CGuiSystem::IsPointOverAnyWidget(const std::vector<SGuiWidgetDesc*>& vWidgets, Vec2 point) const
        {
            for (const SGuiWidgetDesc* pWidget : vWidgets)
            {
                if (!pWidget)
                {
                    continue;
                }
                const Rectangle rect = {
                    pWidget->m_Position.x,
                    pWidget->m_Position.y,
                    std::max(16.0f, pWidget->m_Size.x),
                    std::max(16.0f, pWidget->m_Size.y)
                };
                if (PointInRect(point, rect))
                {
                    return true;
                }
                if (pWidget->m_WidgetType == EGuiWidgetType::COMBO && pWidget->m_ComboOpen)
                {
                    const SGuiTheme& theme = DefaultTheme();
                    if (PointInRect(point, GetComboPopupRect(*pWidget, theme)))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool CGuiSystem::WantsMouseCapture() const
        {
            return m_WantsMouseCapture;
        }

        bool CGuiSystem::WantsKeyboardCapture() const
        {
            return m_WantsKeyboardCapture;
        }

    } // gui
} // quark
