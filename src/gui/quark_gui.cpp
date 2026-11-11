#include "QuarkGUI/QuarkGUI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace quark
{
    namespace gui
    {

        namespace
        {

            struct STextSpan
            {
                std::string m_Text;
                Color m_Color = { 235, 235, 235, 255 };
                int m_FontStyle = 0;
                float m_SizeRatio = 1.0f;
            };

            struct STextLayoutLine
            {
                std::vector<STextSpan> m_vSpans;
                float m_Width = 0.0f;
                float m_Height = 0.0f;
            };

            std::string LowerAscii(const std::string& input)
            {
                std::string out = input;
                for (char& ch : out)
                {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                return out;
            }

            int HexDigitValue(char ch)
            {
                if (ch >= '0' && ch <= '9')
                {
                    return ch - '0';
                }
                if (ch >= 'a' && ch <= 'f')
                {
                    return 10 + (ch - 'a');
                }
                if (ch >= 'A' && ch <= 'F')
                {
                    return 10 + (ch - 'A');
                }
                return -1;
            }

            bool TryParseRichColor(const std::string& rawValue, Color& outColor)
            {
                std::string val = LowerAscii(rawValue);
                if (!val.empty() && (val.front() == '"' || val.front() == '\'') && val.size() >= 2)
                {
                    val = val.substr(1, val.size() - 2);
                }
                if (val.empty())
                {
                    return false;
                }
                if (val[0] == '#')
                {
                    if (val.size() == 7 || val.size() == 9)
                    {
                        auto parseByte = [&](size_t offset, unsigned char& outByte) -> bool
                        {
                            const int hi = HexDigitValue(val[offset]);
                            const int lo = HexDigitValue(val[offset + 1]);
                            if (hi < 0 || lo < 0)
                            {
                                return false;
                            }
                            outByte = static_cast<unsigned char>((hi << 4) | lo);
                            return true;
                        };
                        unsigned char r = 255;
                        unsigned char g = 255;
                        unsigned char b = 255;
                        unsigned char a = 255;
                        if (!parseByte(1, r) || !parseByte(3, g) || !parseByte(5, b))
                        {
                            return false;
                        }
                        if (val.size() == 9 && !parseByte(7, a))
                        {
                            return false;
                        }
                        outColor = { r, g, b, a };
                        return true;
                    }
                    return false;
                }
                if (val == "red")
                {
                    outColor = { 235, 64, 52, 255 };
                    return true;
                }
                if (val == "green" || val == "lime")
                {
                    outColor = { 64, 214, 92, 255 };
                    return true;
                }
                if (val == "blue")
                {
                    outColor = { 66, 135, 245, 255 };
                    return true;
                }
                if (val == "yellow")
                {
                    outColor = { 250, 214, 55, 255 };
                    return true;
                }
                if (val == "white")
                {
                    outColor = { 255, 255, 255, 255 };
                    return true;
                }
                if (val == "black")
                {
                    outColor = { 20, 20, 20, 255 };
                    return true;
                }
                if (val == "cyan" || val == "aqua")
                {
                    outColor = { 56, 224, 235, 255 };
                    return true;
                }
                if (val == "magenta" || val == "purple")
                {
                    outColor = { 219, 75, 235, 255 };
                    return true;
                }
                if (val == "orange")
                {
                    outColor = { 250, 148, 45, 255 };
                    return true;
                }
                if (val == "grey" || val == "gray")
                {
                    outColor = { 160, 160, 160, 255 };
                    return true;
                }
                return false;
            }

            std::vector<STextSpan> ParseTextSpans(const std::string& text,
                                                  bool richText,
                                                  Color baseColor,
                                                  int baseFontStyle,
                                                  float baseFontSize)
            {
                std::vector<STextSpan> vSpans;
                if (!richText)
                {
                    vSpans.push_back({ text, baseColor, baseFontStyle, 1.0f });
                    return vSpans;
                }

                std::vector<Color> vColorStack = { baseColor };
                std::vector<float> vSizeRatioStack = { 1.0f };
                int boldDepth = (baseFontStyle == 1 || baseFontStyle == 3) ? 1 : 0;
                int italicDepth = (baseFontStyle == 2 || baseFontStyle == 3) ? 1 : 0;

                auto makeStyle = [&]() -> int
                {
                    const bool b = boldDepth > 0;
                    const bool it = italicDepth > 0;
                    if (b && it)
                    {
                        return 3;
                    }
                    if (b)
                    {
                        return 1;
                    }
                    if (it)
                    {
                        return 2;
                    }
                    return 0;
                };

                std::string currentBuffer;
                auto flushBuffer = [&]()
                {
                    if (!currentBuffer.empty())
                    {
                        vSpans.push_back({
                            currentBuffer,
                            vColorStack.back(),
                            makeStyle(),
                            vSizeRatioStack.back()
                        });
                        currentBuffer.clear();
                    }
                };

                size_t pos = 0;
                while (pos < text.size())
                {
                    if (text[pos] == '<')
                    {
                        const size_t closePos = text.find('>', pos + 1);
                        if (closePos != std::string::npos && closePos - pos <= 48)
                        {
                            const std::string rawTag = text.substr(pos + 1, closePos - pos - 1);
                            const std::string lowerTag = LowerAscii(rawTag);
                            bool handled = false;

                            if (lowerTag == "b")
                            {
                                flushBuffer();
                                ++boldDepth;
                                handled = true;
                            }
                            else if (lowerTag == "/b")
                            {
                                flushBuffer();
                                boldDepth = std::max(0, boldDepth - 1);
                                handled = true;
                            }
                            else if (lowerTag == "i")
                            {
                                flushBuffer();
                                ++italicDepth;
                                handled = true;
                            }
                            else if (lowerTag == "/i")
                            {
                                flushBuffer();
                                italicDepth = std::max(0, italicDepth - 1);
                                handled = true;
                            }
                            else if (lowerTag.rfind("color=", 0) == 0)
                            {
                                Color parsedColor = vColorStack.back();
                                if (TryParseRichColor(rawTag.substr(6), parsedColor))
                                {
                                    flushBuffer();
                                    vColorStack.push_back(parsedColor);
                                    handled = true;
                                }
                            }
                            else if (lowerTag == "/color")
                            {
                                flushBuffer();
                                if (vColorStack.size() > 1)
                                {
                                    vColorStack.pop_back();
                                }
                                handled = true;
                            }
                            else if (lowerTag.rfind("size=", 0) == 0)
                            {
                                const float parsedSize = std::strtof(rawTag.c_str() + 5, nullptr);
                                if (parsedSize > 0.0f)
                                {
                                    flushBuffer();
                                    const float ratio = std::clamp(parsedSize / std::max(1.0f, baseFontSize), 0.25f, 8.0f);
                                    vSizeRatioStack.push_back(ratio);
                                    handled = true;
                                }
                            }
                            else if (lowerTag == "/size")
                            {
                                flushBuffer();
                                if (vSizeRatioStack.size() > 1)
                                {
                                    vSizeRatioStack.pop_back();
                                }
                                handled = true;
                            }

                            if (handled)
                            {
                                pos = closePos + 1;
                                continue;
                            }
                        }
                    }
                    currentBuffer.push_back(text[pos]);
                    ++pos;
                }
                flushBuffer();
                return vSpans;
            }

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

            const float baseFontSize = std::clamp(widget.m_TextFontSize, 1.0f, 256.0f);
            const float lineSpacing = std::clamp(widget.m_TextLineSpacing, 0.1f, 10.0f);

            auto setTextContext = [&](int styleOverride)
            {
                if (m_Backend.pfnSetTextContext)
                {
                    m_Backend.pfnSetTextContext(
                        widget.m_TextFont.c_str(),
                        styleOverride,
                        widget.m_TextMaterial.c_str());
                }
            };
            setTextContext(widget.m_TextFontStyle);

            auto measureSpanAtSize = [&](const std::string& s, float sizePx, int styleOverride) -> Vec2
            {
                setTextContext(styleOverride);
                if (m_Backend.pfnMeasureText && !s.empty())
                {
                    return m_Backend.pfnMeasureText(s.c_str(), sizePx);
                }
                return { static_cast<float>(s.size()) * (sizePx * 0.52f), sizePx };
            };

            auto measureText = [&](const std::string& s) -> Vec2
            {
                return measureSpanAtSize(s, baseFontSize, widget.m_TextFontStyle);
            };

            auto buildLayoutLines = [&](const std::string& rawText,
                                        Color defaultColor,
                                        float candidateBaseSize,
                                        float maxLineWidth,
                                        bool wrapHorizontally,
                                        float& outMaxWidth,
                                        float& outTotalHeight) -> std::vector<STextLayoutLine>
            {
                const std::vector<STextSpan> vInputSpans = ParseTextSpans(
                    rawText,
                    widget.m_TextRichText,
                    defaultColor,
                    widget.m_TextFontStyle,
                    baseFontSize);

                std::vector<STextLayoutLine> vLines;
                STextLayoutLine currentLine;
                currentLine.m_Height = candidateBaseSize;

                auto finishCurrentLine = [&]()
                {
                    if (currentLine.m_Height <= 0.0f)
                    {
                        currentLine.m_Height = candidateBaseSize;
                    }
                    vLines.push_back(currentLine);
                    currentLine = STextLayoutLine{};
                    currentLine.m_Height = candidateBaseSize;
                };

                auto appendToken = [&](const std::string& tokenText, const STextSpan& protoSpan)
                {
                    if (tokenText.empty())
                    {
                        return;
                    }
                    const float spanFontSize = std::clamp(candidateBaseSize * protoSpan.m_SizeRatio, 1.0f, 256.0f);
                    const Vec2 tokenSize = measureSpanAtSize(tokenText, spanFontSize, protoSpan.m_FontStyle);
                    if (wrapHorizontally &&
                        maxLineWidth > 4.0f &&
                        !currentLine.m_vSpans.empty() &&
                        currentLine.m_Width + tokenSize.x > maxLineWidth)
                    {
                        finishCurrentLine();
                    }

                    std::string adjustedToken = tokenText;
                    if (currentLine.m_vSpans.empty())
                    {
                        size_t firstNonSpace = 0;
                        while (firstNonSpace < adjustedToken.size() && adjustedToken[firstNonSpace] == ' ')
                        {
                            ++firstNonSpace;
                        }
                        if (firstNonSpace > 0 && wrapHorizontally && !vLines.empty())
                        {
                            adjustedToken.erase(0, firstNonSpace);
                        }
                    }
                    if (adjustedToken.empty())
                    {
                        return;
                    }

                    const Vec2 adjSize = (adjustedToken.size() == tokenText.size())
                        ? tokenSize
                        : measureSpanAtSize(adjustedToken, spanFontSize, protoSpan.m_FontStyle);

                    if (!currentLine.m_vSpans.empty() &&
                        currentLine.m_vSpans.back().m_FontStyle == protoSpan.m_FontStyle &&
                        std::fabs(currentLine.m_vSpans.back().m_SizeRatio - protoSpan.m_SizeRatio) < 0.001f &&
                        currentLine.m_vSpans.back().m_Color.r == protoSpan.m_Color.r &&
                        currentLine.m_vSpans.back().m_Color.g == protoSpan.m_Color.g &&
                        currentLine.m_vSpans.back().m_Color.b == protoSpan.m_Color.b &&
                        currentLine.m_vSpans.back().m_Color.a == protoSpan.m_Color.a)
                    {
                        currentLine.m_vSpans.back().m_Text += adjustedToken;
                    }
                    else
                    {
                        STextSpan piece = protoSpan;
                        piece.m_Text = adjustedToken;
                        currentLine.m_vSpans.push_back(piece);
                    }
                    currentLine.m_Width += adjSize.x;
                    currentLine.m_Height = std::max(currentLine.m_Height, adjSize.y);
                };

                for (const STextSpan& span : vInputSpans)
                {
                    size_t idx = 0;
                    while (idx < span.m_Text.size())
                    {
                        if (span.m_Text[idx] == '\n')
                        {
                            finishCurrentLine();
                            ++idx;
                            continue;
                        }
                        if (!wrapHorizontally)
                        {
                            const size_t nextNl = span.m_Text.find('\n', idx);
                            const std::string segment = (nextNl == std::string::npos)
                                ? span.m_Text.substr(idx)
                                : span.m_Text.substr(idx, nextNl - idx);
                            appendToken(segment, span);
                            idx = (nextNl == std::string::npos) ? span.m_Text.size() : nextNl;
                        }
                        else
                        {
                            size_t tokenEnd = idx;
                            const bool isSpace = (span.m_Text[idx] == ' ' || span.m_Text[idx] == '\t');
                            while (tokenEnd < span.m_Text.size() &&
                                   span.m_Text[tokenEnd] != '\n' &&
                                   ((span.m_Text[tokenEnd] == ' ' || span.m_Text[tokenEnd] == '\t') == isSpace))
                            {
                                ++tokenEnd;
                            }
                            appendToken(span.m_Text.substr(idx, tokenEnd - idx), span);
                            idx = tokenEnd;
                        }
                    }
                }
                if (!currentLine.m_vSpans.empty() || vLines.empty())
                {
                    finishCurrentLine();
                }

                outMaxWidth = 0.0f;
                outTotalHeight = 0.0f;
                for (size_t lineIdx = 0; lineIdx < vLines.size(); ++lineIdx)
                {
                    outMaxWidth = std::max(outMaxWidth, vLines[lineIdx].m_Width);
                    if (lineIdx + 1 < vLines.size())
                    {
                        outTotalHeight += vLines[lineIdx].m_Height * lineSpacing;
                    }
                    else
                    {
                        outTotalHeight += vLines[lineIdx].m_Height;
                    }
                }
                return vLines;
            };

            auto drawFormattedWidgetText = [&](const std::string& rawText,
                                               Color defaultColor,
                                               Rectangle boxRect,
                                               float horizontalPadding,
                                               Rectangle baseClipRect)
            {
                if (rawText.empty() || !m_Backend.pfnDrawText)
                {
                    return;
                }

                const bool wrapH = (widget.m_TextHorizontalOverflow == 0);
                const bool truncateV = (widget.m_TextVerticalOverflow != 0);
                const float availWidth = std::max(1.0f, boxRect.width - horizontalPadding * 2.0f);
                const float availHeight = std::max(1.0f, baseClipRect.height);

                float chosenBaseSize = baseFontSize;
                float layoutWidth = 0.0f;
                float layoutHeight = 0.0f;
                std::vector<STextLayoutLine> vLines;

                if (widget.m_TextBestFit)
                {
                    const int minFitSize = 6;
                    const int maxFitSize = std::clamp(static_cast<int>(std::floor(boxRect.height - 4.0f)), minFitSize, 128);
                    int bestSize = minFitSize;
                    for (int testSize = maxFitSize; testSize >= minFitSize; --testSize)
                    {
                        float testW = 0.0f;
                        float testH = 0.0f;
                        std::vector<STextLayoutLine> vCandidate = buildLayoutLines(
                            rawText,
                            defaultColor,
                            static_cast<float>(testSize),
                            availWidth,
                            wrapH,
                            testW,
                            testH);
                        if (testW <= availWidth + 1.5f && testH <= availHeight + 1.5f)
                        {
                            bestSize = testSize;
                            vLines = std::move(vCandidate);
                            layoutWidth = testW;
                            layoutHeight = testH;
                            break;
                        }
                        if (testSize == minFitSize)
                        {
                            vLines = std::move(vCandidate);
                            layoutWidth = testW;
                            layoutHeight = testH;
                        }
                    }
                    chosenBaseSize = static_cast<float>(bestSize);
                }
                else
                {
                    vLines = buildLayoutLines(rawText, defaultColor, chosenBaseSize, availWidth, wrapH, layoutWidth, layoutHeight);
                }

                Rectangle effectiveClip = baseClipRect;
                if (!truncateV)
                {
                    effectiveClip.y = std::min(effectiveClip.y, boxRect.y - 1000.0f);
                    effectiveClip.height = std::max(effectiveClip.height, boxRect.height + 2000.0f);
                }

                const bool useScissor = m_Backend.pfnBeginScissorMode &&
                    m_Backend.pfnEndScissorMode &&
                    effectiveClip.width > 1.0f &&
                    effectiveClip.height > 1.0f;
                if (useScissor)
                {
                    m_Backend.pfnBeginScissorMode(
                        static_cast<int>(std::floor(effectiveClip.x)),
                        static_cast<int>(std::floor(effectiveClip.y)),
                        static_cast<int>(std::ceil(effectiveClip.width)),
                        static_cast<int>(std::ceil(effectiveClip.height)));
                }

                float curY = AlignTextY(boxRect, { layoutWidth, layoutHeight }, widget.m_TextVerticalAlignment);
                for (size_t lineIdx = 0; lineIdx < vLines.size(); ++lineIdx)
                {
                    const STextLayoutLine& line = vLines[lineIdx];
                    float curX = AlignTextX(
                        boxRect,
                        { line.m_Width, line.m_Height },
                        widget.m_TextHorizontalAlignment,
                        horizontalPadding);
                    for (const STextSpan& span : line.m_vSpans)
                    {
                        if (span.m_Text.empty())
                        {
                            continue;
                        }
                        const float spanFontSize = std::clamp(chosenBaseSize * span.m_SizeRatio, 1.0f, 256.0f);
                        const Vec2 spanSize = measureSpanAtSize(span.m_Text, spanFontSize, span.m_FontStyle);
                        const float spanY = curY + (line.m_Height - spanSize.y) * 0.5f;
                        setTextContext(span.m_FontStyle);
                        m_Backend.pfnDrawText(
                            span.m_Text.c_str(),
                            std::round(curX),
                            std::round(spanY),
                            spanFontSize,
                            lineSpacing,
                            span.m_Color);
                        curX += spanSize.x;
                    }
                    curY += line.m_Height * lineSpacing;
                }

                if (useScissor)
                {
                    m_Backend.pfnEndScissorMode();
                }
                setTextContext(widget.m_TextFontStyle);
            };

            auto drawClippedText = [&](const std::string& s, float x, float y, Color color, Rectangle clipRect)
            {
                if (s.empty() || !m_Backend.pfnDrawText)
                {
                    return;
                }
                setTextContext(widget.m_TextFontStyle);
                if (m_Backend.pfnBeginScissorMode && m_Backend.pfnEndScissorMode && clipRect.width > 1.0f && clipRect.height > 1.0f)
                {
                    m_Backend.pfnBeginScissorMode(
                        static_cast<int>(std::floor(clipRect.x)),
                        static_cast<int>(std::floor(clipRect.y)),
                        static_cast<int>(std::ceil(clipRect.width)),
                        static_cast<int>(std::ceil(clipRect.height)));
                    m_Backend.pfnDrawText(s.c_str(), std::round(x), std::round(y), baseFontSize, lineSpacing, color);
                    m_Backend.pfnEndScissorMode();
                }
                else
                {
                    m_Backend.pfnDrawText(s.c_str(), std::round(x), std::round(y), baseFontSize, lineSpacing, color);
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

                    if (widget.m_TextEnabled)
                    {
                        drawFormattedWidgetText(
                            widget.m_Text,
                            widget.m_TextColor,
                            rect,
                            theme.m_PaddingX,
                            ShrinkRect(rect, 3.0f));
                    }
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

                    if (widget.m_TextEnabled)
                    {
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

                        if (!widget.m_Focused)
                        {
                            drawFormattedWidgetText(displayStr, textColor, contentRect, 0.0f, contentRect);
                        }
                        else
                        {
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

                            if (m_Backend.pfnDrawLineEx)
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

                    const float arrowZoneWidth = 26.0f;
                    if (widget.m_TextEnabled)
                    {
                        std::string selectedText = widget.m_Text.empty() ? "Combo" : widget.m_Text;
                        if (!widget.m_vItems.empty())
                        {
                            const int clampedIdx = std::clamp(widget.m_SelectedIndex, 0, static_cast<int>(widget.m_vItems.size()) - 1);
                            selectedText = widget.m_vItems[static_cast<size_t>(clampedIdx)];
                        }

                        const Rectangle labelClip = {
                            rect.x + theme.m_PaddingX,
                            rect.y + 2.0f,
                            std::max(1.0f, rect.width - theme.m_PaddingX - arrowZoneWidth),
                            std::max(1.0f, rect.height - 4.0f)
                        };

                        drawFormattedWidgetText(selectedText, widget.m_TextColor, labelClip, 0.0f, labelClip);
                    }

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

                if (m_Backend.pfnSetTextContext)
                {
                    m_Backend.pfnSetTextContext(
                        pWidget->m_TextFont.c_str(),
                        pWidget->m_TextFontStyle,
                        pWidget->m_TextMaterial.c_str());
                }

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
