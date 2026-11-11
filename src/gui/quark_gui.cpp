#include "QuarkGUI/QuarkGUI.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace quark {
namespace gui {

namespace {

bool PointInRect(Vec2 p, Rectangle r) {
    return p.x >= r.x && p.x <= (r.x + r.width) &&
           p.y >= r.y && p.y <= (r.y + r.height);
}

Rectangle ShrinkRect(Rectangle r, float amount) {
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

bool IsUtf8ContinuationByte(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

int PrevUtf8CharPos(const std::string& str, int pos) {
    int i = std::clamp(pos, 0, static_cast<int>(str.size()));
    if (i <= 0) {
        return 0;
    }
    --i;
    while (i > 0 && IsUtf8ContinuationByte(static_cast<unsigned char>(str[i]))) {
        --i;
    }
    return i;
}

int NextUtf8CharPos(const std::string& str, int pos) {
    const int n = static_cast<int>(str.size());
    int i = std::clamp(pos, 0, n);
    if (i >= n) {
        return n;
    }
    ++i;
    while (i < n && IsUtf8ContinuationByte(static_cast<unsigned char>(str[i]))) {
        ++i;
    }
    return i;
}

std::string EncodeUtf8Codepoint(uint32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

} // anonymous

CGuiSystem& CGuiSystem::Get() {
    static CGuiSystem s_Instance;
    return s_Instance;
}

CGuiSystem::CGuiSystem() = default;
CGuiSystem::~CGuiSystem() = default;

void CGuiSystem::SetBackend(const SGuiBackend& backend) {
    m_Backend = backend;
}

const SGuiBackend& CGuiSystem::GetBackend() const {
    return m_Backend;
}

void CGuiSystem::DrawStyledFrame(Rectangle rect, Color bgColor, Color innerBorderColor, const SGuiTheme& theme) const {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }

    const Rectangle innerBorderRect = ShrinkRect(rect, 1.0f);
    const Rectangle fillRect = ShrinkRect(rect, 2.0f);

    if (m_Backend.pfnDrawRectangle) {
        m_Backend.pfnDrawRectangle(
            static_cast<int>(std::round(rect.x)),
            static_cast<int>(std::round(rect.y)),
            static_cast<int>(std::round(rect.width)),
            static_cast<int>(std::round(rect.height)),
            theme.outerBorderColor);
        if (innerBorderRect.width > 0.0f && innerBorderRect.height > 0.0f) {
            m_Backend.pfnDrawRectangle(
                static_cast<int>(std::round(innerBorderRect.x)),
                static_cast<int>(std::round(innerBorderRect.y)),
                static_cast<int>(std::round(innerBorderRect.width)),
                static_cast<int>(std::round(innerBorderRect.height)),
                innerBorderColor);
        }
        if (fillRect.width > 0.0f && fillRect.height > 0.0f) {
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

void CGuiSystem::DrawDownArrow(Vec2 center, float halfSize, Color color) const {
    if (!m_Backend.pfnDrawTriangle) {
        return;
    }
    const Vec2 topLeft  = { center.x - halfSize, center.y - halfSize * 0.55f };
    const Vec2 topRight = { center.x + halfSize, center.y - halfSize * 0.55f };
    const Vec2 bottom   = { center.x,            center.y + halfSize * 0.65f };
    m_Backend.pfnDrawTriangle(topLeft, bottom, topRight, color);
    m_Backend.pfnDrawTriangle(topLeft, topRight, bottom, color);
}

void CGuiSystem::DrawUpArrow(Vec2 center, float halfSize, Color color) const {
    if (!m_Backend.pfnDrawTriangle) {
        return;
    }
    const Vec2 bottomLeft  = { center.x - halfSize, center.y + halfSize * 0.55f };
    const Vec2 bottomRight = { center.x + halfSize, center.y + halfSize * 0.55f };
    const Vec2 top         = { center.x,            center.y - halfSize * 0.65f };
    m_Backend.pfnDrawTriangle(top, bottomLeft, bottomRight, color);
    m_Backend.pfnDrawTriangle(top, bottomRight, bottomLeft, color);
}

Rectangle CGuiSystem::GetComboPopupRect(const SGuiWidgetDesc& widget, const SGuiTheme& theme) const {
    const int itemCount = std::max(1, static_cast<int>(widget.vItems.size()));
    const float itemH = std::max(20.0f, theme.comboItemHeight);
    const float popupHeight = static_cast<float>(itemCount) * itemH + 4.0f;
    return Rectangle{
        widget.position.x,
        widget.position.y + widget.size.y + 2.0f,
        widget.size.x,
        popupHeight
    };
}

void CGuiSystem::HandleTextInput(SGuiWidgetDesc& widget) {
    widget.caretPos = std::clamp(widget.caretPos, 0, static_cast<int>(widget.text.size()));

    for (uint32_t cp : m_Input.vTypedCodepoints) {
        if (cp < 32 || cp == 127) {
            continue;
        }
        const std::string encoded = EncodeUtf8Codepoint(cp);
        widget.text.insert(static_cast<size_t>(widget.caretPos), encoded);
        widget.caretPos += static_cast<int>(encoded.size());
        widget.valueChanged = true;
    }

    if (m_Input.keyBackspacePressed && widget.caretPos > 0) {
        const int prevPos = PrevUtf8CharPos(widget.text, widget.caretPos);
        widget.text.erase(static_cast<size_t>(prevPos), static_cast<size_t>(widget.caretPos - prevPos));
        widget.caretPos = prevPos;
        widget.valueChanged = true;
    }

    if (m_Input.keyDeletePressed && widget.caretPos < static_cast<int>(widget.text.size())) {
        const int nextPos = NextUtf8CharPos(widget.text, widget.caretPos);
        widget.text.erase(static_cast<size_t>(widget.caretPos), static_cast<size_t>(nextPos - widget.caretPos));
        widget.valueChanged = true;
    }

    if (m_Input.keyLeftPressed) {
        widget.caretPos = PrevUtf8CharPos(widget.text, widget.caretPos);
    }
    if (m_Input.keyRightPressed) {
        widget.caretPos = NextUtf8CharPos(widget.text, widget.caretPos);
    }
    if (m_Input.keyHomePressed) {
        widget.caretPos = 0;
    }
    if (m_Input.keyEndPressed) {
        widget.caretPos = static_cast<int>(widget.text.size());
    }
    if (m_Input.keyEnterPressed || m_Input.keyEscapePressed) {
        widget.focused = false;
        if (m_FocusedWidgetId == widget.widgetId) {
            m_FocusedWidgetId = 0;
        }
    }
}

void CGuiSystem::BeginFrame(const SGuiInputState& input, bool interactive) {
    m_Input = input;
    m_Interactive = interactive;
    m_PopupConsumedClick = false;
    m_WantsMouseCapture = false;
    m_WantsKeyboardCapture = false;
    m_vFrameWidgets.clear();

    if (!m_Interactive) {
        m_ActiveWidgetId = 0;
        m_FocusedWidgetId = 0;
        m_OpenComboWidgetId = 0;
    }
}

void CGuiSystem::ProcessWidget(SGuiWidgetDesc& widget) {
    m_vFrameWidgets.push_back(&widget);

    const SGuiTheme& theme = DefaultTheme();
    const Rectangle rect = {
        widget.position.x,
        widget.position.y,
        std::max(16.0f, widget.size.x),
        std::max(16.0f, widget.size.y)
    };

    widget.clicked = false;
    widget.valueChanged = false;

    const bool mouseOverRect = m_Interactive &&
        (widget.widgetType != EGuiWidgetType::Button || widget.buttonInteractable) &&
        !m_PopupConsumedClick &&
        PointInRect(m_Input.mousePosition, rect);
    widget.hovered = mouseOverRect;
    if (mouseOverRect) {
        m_WantsMouseCapture = true;
    }

    if (!m_Interactive) {
        widget.pressed = false;
        widget.focused = false;
        widget.comboOpen = false;
        widget.hoveredItemIndex = -1;
    }

    const float fontSize = std::clamp(widget.textFontSize, 1.0f, 256.0f);
    const float lineSpacing = std::clamp(widget.textLineSpacing, 0.1f, 10.0f);

    auto measureText = [&](const std::string& s) -> Vec2 {
        if (m_Backend.pfnMeasureText && !s.empty()) {
            return m_Backend.pfnMeasureText(s.c_str(), fontSize);
        }
        return { static_cast<float>(s.size()) * (fontSize * 0.52f), fontSize };
    };

    auto drawClippedText = [&](const std::string& s, float x, float y, Color color, Rectangle clipRect) {
        if (s.empty() || !m_Backend.pfnDrawText) {
            return;
        }
        if (m_Backend.pfnBeginScissorMode && m_Backend.pfnEndScissorMode && clipRect.width > 1.0f && clipRect.height > 1.0f) {
            m_Backend.pfnBeginScissorMode(
                static_cast<int>(std::floor(clipRect.x)),
                static_cast<int>(std::floor(clipRect.y)),
                static_cast<int>(std::ceil(clipRect.width)),
                static_cast<int>(std::ceil(clipRect.height)));
            m_Backend.pfnDrawText(s.c_str(), std::round(x), std::round(y), fontSize, lineSpacing, color);
            m_Backend.pfnEndScissorMode();
        } else {
            m_Backend.pfnDrawText(s.c_str(), std::round(x), std::round(y), fontSize, lineSpacing, color);
        }
    };

    switch (widget.widgetType) {
        case EGuiWidgetType::Button: {
            if (m_Interactive && widget.buttonInteractable) {
                if (mouseOverRect && m_Input.mousePressed) {
                    m_ActiveWidgetId = widget.widgetId;
                    m_FocusedWidgetId = 0;
                    m_OpenComboWidgetId = 0;
                }
                widget.pressed = (m_ActiveWidgetId == widget.widgetId) && m_Input.mouseDown && mouseOverRect;
                if (m_ActiveWidgetId == widget.widgetId && m_Input.mouseReleased) {
                    if (mouseOverRect) {
                        widget.clicked = true;
                        widget.valueChanged = true;
                        ++widget.clickCount;
                    }
                    m_ActiveWidgetId = 0;
                    widget.pressed = false;
                }
            }
            else if (m_ActiveWidgetId == widget.widgetId)
            {
                m_ActiveWidgetId = 0;
                widget.pressed = false;
            }

            Color bg = theme.buttonColor;
            Color border = theme.borderColor;
            if (widget.buttonTransition == EGuiButtonTransition::COLOR_TINT)
            {
                if (!widget.buttonInteractable)
                {
                    bg = widget.buttonDisabledColor;
                }
                else if (widget.pressed)
                {
                    bg = widget.buttonPressedColor;
                    border = theme.borderFocusedColor;
                }
                else if (widget.hovered)
                {
                    bg = widget.buttonHighlightedColor;
                    border = theme.borderHoveredColor;
                }
                else
                {
                    bg = widget.buttonNormalColor;
                }
                bg = MultiplyColor(bg, std::clamp(widget.buttonColorMultiplier, 0.0f, 10.0f));
            }
            else if (widget.pressed)
            {
                border = theme.borderFocusedColor;
            }
            else if (widget.hovered)
            {
                border = theme.borderHoveredColor;
            }

            DrawStyledFrame(rect, bg, border, theme);
            if (widget.showNavigationVisualization && m_Backend.pfnDrawLineEx)
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

            const std::string& label = widget.text.empty() ? std::string("Button") : widget.text;
            const Vec2 textSize = measureText(label);
            const float textX = AlignTextX(rect, textSize, widget.textHorizontalAlignment, theme.paddingX);
            const float textY = AlignTextY(rect, textSize, widget.textVerticalAlignment);
            drawClippedText(label, textX, textY, widget.textColor, ShrinkRect(rect, 3.0f));
            break;
        }

        case EGuiWidgetType::InputField: {
            if (m_Interactive) {
                if (m_Input.mousePressed && !m_PopupConsumedClick) {
                    if (mouseOverRect) {
                        m_FocusedWidgetId = widget.widgetId;
                        m_OpenComboWidgetId = 0;
                        widget.focused = true;
                        widget.clicked = true;
                        widget.caretPos = static_cast<int>(widget.text.size());
                    } else if (m_FocusedWidgetId == widget.widgetId) {
                        m_FocusedWidgetId = 0;
                        widget.focused = false;
                    }
                }
                widget.focused = (m_FocusedWidgetId == widget.widgetId);
                if (widget.focused) {
                    m_WantsKeyboardCapture = true;
                    m_WantsMouseCapture = true;
                    HandleTextInput(widget);
                }
            }

            Color bg = theme.inputBgColor;
            Color border = theme.borderColor;
            if (widget.focused) {
                bg = theme.inputActiveColor;
                border = theme.borderFocusedColor;
            } else if (widget.hovered) {
                bg = theme.inputHoveredColor;
                border = theme.borderHoveredColor;
            }

            DrawStyledFrame(rect, bg, border, theme);

            const Rectangle contentRect = {
                rect.x + theme.paddingX,
                rect.y + 3.0f,
                std::max(1.0f, rect.width - theme.paddingX * 2.0f),
                std::max(1.0f, rect.height - 6.0f)
            };

            const bool showPlaceholder = widget.text.empty() && !widget.focused;
            const std::string& displayStr = showPlaceholder
                ? (widget.placeholder.empty() ? std::string("InputField") : widget.placeholder)
                : widget.text;
            const Color textColor = showPlaceholder ? theme.placeholderColor : widget.textColor;

            const Vec2 fullSize = measureText(displayStr.empty() ? std::string("Ag") : displayStr);
            const float textY = AlignTextY(rect, fullSize, widget.textVerticalAlignment);

            float scrollOffsetX = 0.0f;
            const int safeCaret = std::clamp(widget.caretPos, 0, static_cast<int>(widget.text.size()));
            const std::string textBeforeCaret = widget.text.substr(0, static_cast<size_t>(safeCaret));
            const float caretOffsetPx = widget.text.empty() ? 0.0f : measureText(textBeforeCaret).x;
            if (caretOffsetPx > contentRect.width - 6.0f) {
                scrollOffsetX = caretOffsetPx - (contentRect.width - 6.0f);
            }

            const float alignedTextX = AlignTextX(
                contentRect,
                fullSize,
                widget.textHorizontalAlignment,
                0.0f);
            const float textX = alignedTextX - scrollOffsetX;
            drawClippedText(displayStr, textX, textY, textColor, contentRect);

            if (widget.focused && m_Backend.pfnDrawLineEx) {
                const double now = m_Backend.pfnGetTime ? m_Backend.pfnGetTime() : 0.0;
                const bool caretVisible = (std::fmod(now, 1.0) < 0.55);
                if (caretVisible) {
                    const float caretX = std::clamp(textX + caretOffsetPx + 1.0f, contentRect.x, contentRect.x + contentRect.width);
                    const float caretTop = rect.y + 6.0f;
                    const float caretBottom = rect.y + rect.height - 6.0f;
                    m_Backend.pfnDrawLineEx({ caretX, caretTop }, { caretX, caretBottom }, 1.5f, widget.textColor);
                }
            }
            break;
        }

        case EGuiWidgetType::Combo: {
            if (m_Interactive) {
                if (mouseOverRect && m_Input.mousePressed) {
                    widget.clicked = true;
                    m_FocusedWidgetId = 0;
                    if (m_OpenComboWidgetId == widget.widgetId) {
                        m_OpenComboWidgetId = 0;
                        widget.comboOpen = false;
                    } else {
                        m_OpenComboWidgetId = widget.widgetId;
                        widget.comboOpen = true;
                    }
                }
                widget.comboOpen = (m_OpenComboWidgetId == widget.widgetId);
                if (widget.comboOpen) {
                    m_WantsMouseCapture = true;
                }
            }

            Color bg = theme.comboBgColor;
            Color border = theme.borderColor;
            if (widget.comboOpen) {
                bg = theme.comboActiveColor;
                border = theme.borderFocusedColor;
            } else if (widget.hovered) {
                bg = theme.comboHoveredColor;
                border = theme.borderHoveredColor;
            }

            DrawStyledFrame(rect, bg, border, theme);

            std::string selectedText = widget.text.empty() ? "Combo" : widget.text;
            if (!widget.vItems.empty()) {
                const int clampedIdx = std::clamp(widget.selectedIndex, 0, static_cast<int>(widget.vItems.size()) - 1);
                selectedText = widget.vItems[static_cast<size_t>(clampedIdx)];
            }

            const float arrowZoneWidth = 26.0f;
            const Rectangle labelClip = {
                rect.x + theme.paddingX,
                rect.y + 2.0f,
                std::max(1.0f, rect.width - theme.paddingX - arrowZoneWidth),
                std::max(1.0f, rect.height - 4.0f)
            };

            const Vec2 textSize = measureText(selectedText.empty() ? std::string("Ag") : selectedText);
            const float textX = AlignTextX(
                labelClip,
                textSize,
                widget.textHorizontalAlignment,
                0.0f);
            const float textY = AlignTextY(rect, textSize, widget.textVerticalAlignment);
            drawClippedText(selectedText, textX, textY, widget.textColor, labelClip);

            const Vec2 arrowCenter = {
                rect.x + rect.width - arrowZoneWidth * 0.5f,
                rect.y + rect.height * 0.5f
            };
            if (widget.comboOpen) {
                DrawUpArrow(arrowCenter, 4.5f, theme.arrowColor);
            } else {
                DrawDownArrow(arrowCenter, 4.5f, theme.arrowColor);
            }
            break;
        }
    }
}

void CGuiSystem::EndFrame() {
    for (SGuiWidgetDesc* pWidget : m_vFrameWidgets) {
        if (!pWidget || pWidget->widgetType != EGuiWidgetType::Combo) {
            continue;
        }
        pWidget->comboOpen = (m_Interactive && m_OpenComboWidgetId == pWidget->widgetId);
        if (!pWidget->comboOpen) {
            pWidget->hoveredItemIndex = -1;
            continue;
        }

        const SGuiTheme& theme = DefaultTheme();
        const Rectangle popupRect = GetComboPopupRect(*pWidget, theme);
        const Rectangle mainRect = {
            pWidget->position.x,
            pWidget->position.y,
            std::max(16.0f, pWidget->size.x),
            std::max(16.0f, pWidget->size.y)
        };

        DrawStyledFrame(popupRect, theme.comboPopupBgColor, theme.borderFocusedColor, theme);

        const int itemCount = static_cast<int>(pWidget->vItems.size());
        const float itemH = std::max(20.0f, theme.comboItemHeight);
        const float fontSize = std::clamp(pWidget->textFontSize, 1.0f, 256.0f);
        const float lineSpacing = std::clamp(pWidget->textLineSpacing, 0.1f, 10.0f);

        pWidget->hoveredItemIndex = -1;

        for (int i = 0; i < itemCount; ++i) {
            const Rectangle itemRect = {
                popupRect.x + 2.0f,
                popupRect.y + 2.0f + static_cast<float>(i) * itemH,
                std::max(1.0f, popupRect.width - 4.0f),
                itemH
            };

            const bool itemHovered = m_Interactive && PointInRect(m_Input.mousePosition, itemRect);
            if (itemHovered) {
                pWidget->hoveredItemIndex = i;
                m_WantsMouseCapture = true;
            }

            if (itemHovered || i == pWidget->selectedIndex) {
                const Color rowColor = itemHovered ? theme.comboItemHoverColor : theme.comboItemSelectedColor;
                if (m_Backend.pfnDrawRectangle) {
                    m_Backend.pfnDrawRectangle(
                        static_cast<int>(std::round(itemRect.x)),
                        static_cast<int>(std::round(itemRect.y)),
                        static_cast<int>(std::round(itemRect.width)),
                        static_cast<int>(std::round(itemRect.height)),
                        rowColor);
                }
            }

            const std::string& itemText = pWidget->vItems[static_cast<size_t>(i)];
            Vec2 tSize = { fontSize, fontSize };
            if (m_Backend.pfnMeasureText && !itemText.empty()) {
                tSize = m_Backend.pfnMeasureText(itemText.c_str(), fontSize);
            }
            const float textX = itemRect.x + theme.paddingX;
            const float textY = itemRect.y + (itemRect.height - tSize.y) * 0.5f;

            if (m_Backend.pfnDrawText && !itemText.empty()) {
                if (m_Backend.pfnBeginScissorMode && m_Backend.pfnEndScissorMode) {
                    m_Backend.pfnBeginScissorMode(
                        static_cast<int>(std::floor(itemRect.x)),
                        static_cast<int>(std::floor(itemRect.y)),
                        static_cast<int>(std::ceil(itemRect.width)),
                        static_cast<int>(std::ceil(itemRect.height)));
                    m_Backend.pfnDrawText(itemText.c_str(), std::round(textX), std::round(textY), fontSize, lineSpacing, pWidget->textColor);
                    m_Backend.pfnEndScissorMode();
                } else {
                    m_Backend.pfnDrawText(itemText.c_str(), std::round(textX), std::round(textY), fontSize, lineSpacing, pWidget->textColor);
                }
            }

            if (itemHovered && m_Input.mousePressed) {
                if (pWidget->selectedIndex != i) {
                    pWidget->selectedIndex = i;
                    pWidget->valueChanged = true;
                }
                pWidget->text = itemText;
                pWidget->clicked = true;
                pWidget->comboOpen = false;
                m_OpenComboWidgetId = 0;
            }
        }

        if (m_Interactive) {
            const bool clickOutside = m_Input.mousePressed &&
                                      !PointInRect(m_Input.mousePosition, mainRect) &&
                                      !PointInRect(m_Input.mousePosition, popupRect);
            if (clickOutside || m_Input.keyEscapePressed) {
                pWidget->comboOpen = false;
                m_OpenComboWidgetId = 0;
            }
        }
    }
}

void CGuiSystem::UpdateAndRender(std::vector<SGuiWidgetDesc*>& vWidgets, const SGuiInputState& input, bool interactive) {
    BeginFrame(input, interactive);

    if (interactive && m_OpenComboWidgetId != 0) {
        for (const SGuiWidgetDesc* pWidget : vWidgets) {
            if (pWidget && pWidget->widgetId == m_OpenComboWidgetId && pWidget->widgetType == EGuiWidgetType::Combo) {
                const SGuiTheme& theme = DefaultTheme();
                const Rectangle popupRect = GetComboPopupRect(*pWidget, theme);
                if (PointInRect(input.mousePosition, popupRect)) {
                    m_PopupConsumedClick = true;
                    m_WantsMouseCapture = true;
                }
                break;
            }
        }
    }

    for (SGuiWidgetDesc* pWidget : vWidgets) {
        if (pWidget) {
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

bool CGuiSystem::IsPointOverAnyWidget(const std::vector<SGuiWidgetDesc*>& vWidgets, Vec2 point) const {
    for (const SGuiWidgetDesc* pWidget : vWidgets) {
        if (!pWidget) {
            continue;
        }
        const Rectangle rect = {
            pWidget->position.x,
            pWidget->position.y,
            std::max(16.0f, pWidget->size.x),
            std::max(16.0f, pWidget->size.y)
        };
        if (PointInRect(point, rect)) {
            return true;
        }
        if (pWidget->widgetType == EGuiWidgetType::Combo && pWidget->comboOpen) {
            const SGuiTheme& theme = DefaultTheme();
            if (PointInRect(point, GetComboPopupRect(*pWidget, theme))) {
                return true;
            }
        }
    }
    return false;
}

bool CGuiSystem::WantsMouseCapture() const {
    return m_WantsMouseCapture;
}

bool CGuiSystem::WantsKeyboardCapture() const {
    return m_WantsKeyboardCapture;
}

} // gui
} // quark
