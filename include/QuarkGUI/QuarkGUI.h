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

enum class EGuiWidgetType : int
{
    BUTTON = 0,
    INPUT_FIELD = 1,
    COMBO = 2
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

struct SGuiTheme
{
    Color m_PanelBgColor = { 45, 45, 45, 255 };
    Color m_PanelBorderColor = { 58, 58, 58, 255 };

    Color m_OuterBorderColor = { 32, 32, 32, 255 };
    Color m_BorderColor = { 88, 88, 88, 255 };
    Color m_BorderHoveredColor = { 112, 112, 112, 255 };
    Color m_BorderFocusedColor = { 132, 132, 132, 255 };

    Color m_ButtonColor = { 72, 72, 72, 255 };
    Color m_ButtonHoveredColor = { 88, 88, 88, 255 };
    Color m_ButtonActiveColor = { 58, 58, 58, 255 };

    Color m_InputBgColor = { 58, 58, 58, 255 };
    Color m_InputHoveredColor = { 72, 72, 72, 255 };
    Color m_InputActiveColor = { 50, 50, 50, 255 };

    Color m_ComboBgColor = { 72, 72, 72, 255 };
    Color m_ComboHoveredColor = { 88, 88, 88, 255 };
    Color m_ComboActiveColor = { 58, 58, 58, 255 };
    Color m_ComboPopupBgColor = { 50, 50, 50, 255 };
    Color m_ComboItemHoverColor = { 68, 68, 68, 255 };
    Color m_ComboItemSelectedColor = { 58, 58, 58, 255 };

    Color m_TextColor = { 235, 235, 235, 255 };
    Color m_PlaceholderColor = { 176, 176, 176, 255 };
    Color m_ArrowColor = { 205, 205, 205, 255 };

    float m_CornerRadius = 3.0f;
    float m_FontSize = 16.0f;
    float m_PaddingX = 10.0f;
    float m_ComboItemHeight = 28.0f;
};

struct SGuiBackend
{
    void (*pfnDrawRectangle)(int posX, int posY, int width, int height, Color color) = nullptr;
    void (*pfnDrawRectangleRounded)(Rectangle rec, float roundness, int segments, Color color) = nullptr;
    void (*pfnDrawTriangle)(Vec2 v1, Vec2 v2, Vec2 v3, Color color) = nullptr;
    void (*pfnDrawLineEx)(Vec2 startPos, Vec2 endPos, float thick, Color color) = nullptr;
    void (*pfnBeginScissorMode)(int x, int y, int width, int height) = nullptr;
    void (*pfnEndScissorMode)() = nullptr;
    void (*pfnDrawText)(const char* pText, float x, float y, float fontSize, float lineSpacing, Color color) = nullptr;
    Vec2 (*pfnMeasureText)(const char* pText, float fontSize) = nullptr;
    double (*pfnGetTime)() = nullptr;
};

struct SGuiInputState
{
    Vec2 m_MousePosition = { 0.0f, 0.0f };
    bool m_MouseDown = false;
    bool m_MousePressed = false;
    bool m_MouseReleased = false;
    float m_MouseWheelMove = 0.0f;

    bool m_KeyBackspacePressed = false;
    bool m_KeyDeletePressed = false;
    bool m_KeyLeftPressed = false;
    bool m_KeyRightPressed = false;
    bool m_KeyHomePressed = false;
    bool m_KeyEndPressed = false;
    bool m_KeyEnterPressed = false;
    bool m_KeyEscapePressed = false;

    std::vector<uint32_t> m_vTypedCodepoints;
};

struct SGuiWidgetDesc
{
    uint64_t m_WidgetId = 0;
    EGuiWidgetType m_WidgetType = EGuiWidgetType::BUTTON;
    Vec2 m_Position = { 40.0f, 40.0f };
    Vec2 m_Size = { 160.0f, 30.0f };
    std::string m_Text;
    std::string m_Placeholder;
    std::vector<std::string> m_vItems;
    int m_SelectedIndex = 0;
    float m_TextFontSize = 16.0f;
    float m_TextLineSpacing = 1.0f;
    Color m_TextColor = { 235, 235, 235, 255 };
    int m_TextHorizontalAlignment = 1;
    int m_TextVerticalAlignment = 1;
    bool m_ButtonInteractable = true;
    EGuiButtonTransition m_ButtonTransition = EGuiButtonTransition::COLOR_TINT;
    Color m_ButtonNormalColor = { 72, 72, 72, 255 };
    Color m_ButtonHighlightedColor = { 88, 88, 88, 255 };
    Color m_ButtonPressedColor = { 58, 58, 58, 255 };
    Color m_ButtonSelectedColor = { 88, 88, 88, 255 };
    Color m_ButtonDisabledColor = { 50, 50, 50, 255 };
    float m_ButtonColorMultiplier = 1.0f;
    float m_ButtonFadeDuration = 0.1f;
    EGuiButtonNavigation m_ButtonNavigation = EGuiButtonNavigation::AUTOMATIC;
    bool m_ShowNavigationVisualization = false;

    bool m_Hovered = false;
    bool m_Pressed = false;
    bool m_Clicked = false;
    bool m_ValueChanged = false;
    bool m_Focused = false;
    bool m_ComboOpen = false;
    int m_HoveredItemIndex = -1;
    int m_CaretPos = 0;
    int m_ClickCount = 0;
};

namespace quark
{
    namespace gui
    {

        class CGuiSystem
        {
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

#endif // __QUARK_GUI_H__
