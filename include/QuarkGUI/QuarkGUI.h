#ifndef __QUARK_GUI_H__
#define __QUARK_GUI_H__

#include <QuarkCore/QuarkCore.hpp>
#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32)
    #if defined(QUARK_GUI_BUILD_DLL)
        #define QUARK_GUI_API __declspec(dllexport)
    #elif defined(QUARK_GUI_STATIC)
        #define QUARK_GUI_API
    #else
        #define QUARK_GUI_API __declspec(dllimport)
    #endif
#else
    #define QUARK_GUI_API __attribute__((visibility("default")))
#endif

enum class EGuiWidgetType : int {
    Button = 0,
    InputField = 1,
    Combo = 2
};

enum class EGuiButtonTransition : int
{
    NONE = 0,
    COLOR_TINT = 1
};

enum class EGuiButtonNavigation : int
{
    AUTOMATIC = 0,
    NONE = 1
};

struct SGuiTheme {
    Color panelBgColor = { 45, 45, 45, 255 };
    Color panelBorderColor = { 58, 58, 58, 255 };

    Color outerBorderColor = { 32, 32, 32, 255 };
    Color borderColor = { 88, 88, 88, 255 };
    Color borderHoveredColor = { 112, 112, 112, 255 };
    Color borderFocusedColor = { 132, 132, 132, 255 };

    Color buttonColor = { 72, 72, 72, 255 };
    Color buttonHoveredColor = { 88, 88, 88, 255 };
    Color buttonActiveColor = { 58, 58, 58, 255 };

    Color inputBgColor = { 58, 58, 58, 255 };
    Color inputHoveredColor = { 72, 72, 72, 255 };
    Color inputActiveColor = { 50, 50, 50, 255 };

    Color comboBgColor = { 72, 72, 72, 255 };
    Color comboHoveredColor = { 88, 88, 88, 255 };
    Color comboActiveColor = { 58, 58, 58, 255 };
    Color comboPopupBgColor = { 50, 50, 50, 255 };
    Color comboItemHoverColor = { 68, 68, 68, 255 };
    Color comboItemSelectedColor = { 58, 58, 58, 255 };

    Color textColor = { 235, 235, 235, 255 };
    Color placeholderColor = { 176, 176, 176, 255 };
    Color arrowColor = { 205, 205, 205, 255 };

    float cornerRadius = 3.0f;
    float fontSize = 16.0f;
    float paddingX = 10.0f;
    float comboItemHeight = 28.0f;
};

struct SGuiBackend {
    void (*pfnDrawRectangle)(int posX, int posY, int width, int height, Color color) = nullptr;
    void (*pfnDrawRectangleRounded)(Rectangle rec, float roundness, int segments, Color color) = nullptr;
    void (*pfnDrawTriangle)(Vec2 v1, Vec2 v2, Vec2 v3, Color color) = nullptr;
    void (*pfnDrawLineEx)(Vec2 startPos, Vec2 endPos, float thick, Color color) = nullptr;
    void (*pfnBeginScissorMode)(int x, int y, int width, int height) = nullptr;
    void (*pfnEndScissorMode)() = nullptr;
    void (*pfnDrawText)(const char* text, float x, float y, float fontSize, float lineSpacing, Color color) = nullptr;
    Vec2 (*pfnMeasureText)(const char* text, float fontSize) = nullptr;
    double (*pfnGetTime)() = nullptr;
};

struct SGuiInputState {
    Vec2 mousePosition = { 0.0f, 0.0f };
    bool mouseDown = false;
    bool mousePressed = false;
    bool mouseReleased = false;
    float mouseWheelMove = 0.0f;

    bool keyBackspacePressed = false;
    bool keyDeletePressed = false;
    bool keyLeftPressed = false;
    bool keyRightPressed = false;
    bool keyHomePressed = false;
    bool keyEndPressed = false;
    bool keyEnterPressed = false;
    bool keyEscapePressed = false;

    std::vector<uint32_t> vTypedCodepoints;
};

struct SGuiWidgetDesc {
    uint64_t widgetId = 0;
    EGuiWidgetType widgetType = EGuiWidgetType::Button;
    Vec2 position = { 40.0f, 40.0f };
    Vec2 size = { 160.0f, 30.0f };
    std::string text;
    std::string placeholder;
    std::vector<std::string> vItems;
    int selectedIndex = 0;
    float textFontSize = 16.0f;
    float textLineSpacing = 1.0f;
    Color textColor = { 235, 235, 235, 255 };
    int textHorizontalAlignment = 1;
    int textVerticalAlignment = 1;
    bool buttonInteractable = true;
    EGuiButtonTransition buttonTransition = EGuiButtonTransition::COLOR_TINT;
    Color buttonNormalColor = { 72, 72, 72, 255 };
    Color buttonHighlightedColor = { 88, 88, 88, 255 };
    Color buttonPressedColor = { 58, 58, 58, 255 };
    Color buttonSelectedColor = { 88, 88, 88, 255 };
    Color buttonDisabledColor = { 50, 50, 50, 255 };
    float buttonColorMultiplier = 1.0f;
    float buttonFadeDuration = 0.1f;
    EGuiButtonNavigation buttonNavigation = EGuiButtonNavigation::AUTOMATIC;
    bool showNavigationVisualization = false;

    bool hovered = false;
    bool pressed = false;
    bool clicked = false;
    bool valueChanged = false;
    bool focused = false;
    bool comboOpen = false;
    int hoveredItemIndex = -1;
    int caretPos = 0;
    int clickCount = 0;
};

namespace quark {
namespace gui {

class CGuiSystem {
public:
    static QUARK_GUI_API CGuiSystem& Get();

    QUARK_GUI_API void SetBackend(const SGuiBackend& backend);
    QUARK_GUI_API const SGuiBackend& GetBackend() const;

    QUARK_GUI_API void BeginFrame(const SGuiInputState& input, bool interactive);

    QUARK_GUI_API void ProcessWidget(SGuiWidgetDesc& widget);

    QUARK_GUI_API void EndFrame();

    QUARK_GUI_API void UpdateAndRender(std::vector<SGuiWidgetDesc*>& vWidgets, const SGuiInputState& input, bool interactive);
    QUARK_GUI_API void RenderOnly(std::vector<SGuiWidgetDesc*>& vWidgets);

    QUARK_GUI_API bool IsPointOverAnyWidget(const std::vector<SGuiWidgetDesc*>& vWidgets, Vec2 point) const;

    QUARK_GUI_API bool WantsMouseCapture() const;
    QUARK_GUI_API bool WantsKeyboardCapture() const;

private:
    QUARK_GUI_API CGuiSystem();
    QUARK_GUI_API ~CGuiSystem();
    CGuiSystem(const CGuiSystem&) = delete;
    CGuiSystem& operator=(const CGuiSystem&) = delete;

    void DrawStyledFrame(Rectangle rect, Color bgColor, Color innerBorderColor, const SGuiTheme& theme) const;
    void DrawDownArrow(Vec2 center, float halfSize, Color color) const;
    void DrawUpArrow(Vec2 center, float halfSize, Color color) const;
    Rectangle GetComboPopupRect(const SGuiWidgetDesc& widget, const SGuiTheme& theme) const;
    void HandleTextInput(SGuiWidgetDesc& widget);

    SGuiBackend m_Backend;
    SGuiInputState m_Input;
    bool m_Interactive = false;
    bool m_PopupConsumedClick = false;
    bool m_WantsMouseCapture = false;
    bool m_WantsKeyboardCapture = false;
    uint64_t m_ActiveWidgetId = 0;
    uint64_t m_FocusedWidgetId = 0;
    uint64_t m_OpenComboWidgetId = 0;
    std::vector<SGuiWidgetDesc*> m_vFrameWidgets;
};

} // gui
} // quark

#endif // QUARK_GUI_H__
