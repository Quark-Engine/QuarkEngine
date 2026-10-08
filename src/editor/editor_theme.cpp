#include "editor/editor_theme.h"

#include "imgui.h"
#include "language_manager.h"

#include "nlohmann/json.hpp"
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using json = nlohmann::json;
namespace fs = std::filesystem;

struct SThemeOverrides
{
    bool lightBase = false;
    std::string fontPath;
    float fontSize = 16.0f;
    float fontScale = 1.0f;
    std::vector<std::pair<ImGuiCol, ImVec4>> vColors;
    std::vector<std::pair<float ImGuiStyle::*, float>> vFloatStyle;
    std::vector<std::pair<ImVec2 ImGuiStyle::*, ImVec2>> vVectorStyle;
    std::vector<std::pair<bool ImGuiStyle::*, bool>> vBoolStyle;
    std::vector<std::pair<ImVec4 ImGuiStyle::*, ImVec4>> vStyleColors;
};

using TColorNames = std::unordered_map<std::string, ImGuiCol>;
using TFloatStyleNames = std::unordered_map<std::string, float ImGuiStyle::*>;
using TVectorStyleNames = std::unordered_map<std::string, ImVec2 ImGuiStyle::*>;
using TBoolStyleNames = std::unordered_map<std::string, bool ImGuiStyle::*>;
using TStyleColorNames = std::unordered_map<std::string, ImVec4 ImGuiStyle::*>;

float g_AppliedFontScale = 1.0f;
SThemeOverrides g_PendingFontOverrides;
std::string g_PendingFontThemeId;
bool g_HasPendingFonts = false;

const TColorNames& GetColorNames()
{
    static const TColorNames s_Colors = {
        {"Text", ImGuiCol_Text}, {"TextDisabled", ImGuiCol_TextDisabled},
        {"WindowBg", ImGuiCol_WindowBg}, {"ChildBg", ImGuiCol_ChildBg},
        {"PopupBg", ImGuiCol_PopupBg}, {"Border", ImGuiCol_Border},
        {"BorderShadow", ImGuiCol_BorderShadow}, {"FrameBg", ImGuiCol_FrameBg},
        {"FrameBgHovered", ImGuiCol_FrameBgHovered}, {"FrameBgActive", ImGuiCol_FrameBgActive},
        {"TitleBg", ImGuiCol_TitleBg}, {"TitleBgActive", ImGuiCol_TitleBgActive},
        {"TitleBgCollapsed", ImGuiCol_TitleBgCollapsed}, {"MenuBarBg", ImGuiCol_MenuBarBg},
        {"ScrollbarBg", ImGuiCol_ScrollbarBg}, {"ScrollbarGrab", ImGuiCol_ScrollbarGrab},
        {"ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered},
        {"ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive},
        {"CheckMark", ImGuiCol_CheckMark}, {"SliderGrab", ImGuiCol_SliderGrab},
        {"SliderGrabActive", ImGuiCol_SliderGrabActive}, {"Button", ImGuiCol_Button},
        {"ButtonHovered", ImGuiCol_ButtonHovered}, {"ButtonActive", ImGuiCol_ButtonActive},
        {"Header", ImGuiCol_Header}, {"HeaderHovered", ImGuiCol_HeaderHovered},
        {"HeaderActive", ImGuiCol_HeaderActive}, {"Separator", ImGuiCol_Separator},
        {"SeparatorHovered", ImGuiCol_SeparatorHovered},
        {"SeparatorActive", ImGuiCol_SeparatorActive}, {"ResizeGrip", ImGuiCol_ResizeGrip},
        {"ResizeGripHovered", ImGuiCol_ResizeGripHovered},
        {"ResizeGripActive", ImGuiCol_ResizeGripActive},
        {"InputTextCursor", ImGuiCol_InputTextCursor}, {"TabHovered", ImGuiCol_TabHovered},
        {"Tab", ImGuiCol_Tab}, {"TabSelected", ImGuiCol_TabSelected},
        {"TabSelectedOverline", ImGuiCol_TabSelectedOverline}, {"TabDimmed", ImGuiCol_TabDimmed},
        {"TabDimmedSelected", ImGuiCol_TabDimmedSelected},
        {"TabDimmedSelectedOverline", ImGuiCol_TabDimmedSelectedOverline},
        {"DockingPreview", ImGuiCol_DockingPreview}, {"DockingEmptyBg", ImGuiCol_DockingEmptyBg},
        {"PlotLines", ImGuiCol_PlotLines}, {"PlotLinesHovered", ImGuiCol_PlotLinesHovered},
        {"PlotHistogram", ImGuiCol_PlotHistogram},
        {"PlotHistogramHovered", ImGuiCol_PlotHistogramHovered},
        {"TableHeaderBg", ImGuiCol_TableHeaderBg},
        {"TableBorderStrong", ImGuiCol_TableBorderStrong},
        {"TableBorderLight", ImGuiCol_TableBorderLight}, {"TableRowBg", ImGuiCol_TableRowBg},
        {"TableRowBgAlt", ImGuiCol_TableRowBgAlt}, {"TextLink", ImGuiCol_TextLink},
        {"TextSelectedBg", ImGuiCol_TextSelectedBg}, {"TreeLines", ImGuiCol_TreeLines},
        {"DragDropTarget", ImGuiCol_DragDropTarget},
        {"DragDropTargetBg", ImGuiCol_DragDropTargetBg},
        {"UnsavedMarker", ImGuiCol_UnsavedMarker}, {"NavCursor", ImGuiCol_NavCursor},
        {"NavWindowingHighlight", ImGuiCol_NavWindowingHighlight},
        {"NavWindowingDimBg", ImGuiCol_NavWindowingDimBg},
        {"ModalWindowDimBg", ImGuiCol_ModalWindowDimBg},
        {"TabActive", ImGuiCol_TabSelected}, {"TabUnfocused", ImGuiCol_TabDimmed},
        {"TabUnfocusedActive", ImGuiCol_TabDimmedSelected}
    };
    return s_Colors;
}

const TFloatStyleNames& GetFloatStyleNames()
{
    static const TFloatStyleNames s_Styles = {
        {"alpha", &ImGuiStyle::Alpha}, {"disabled_alpha", &ImGuiStyle::DisabledAlpha},
        {"window_rounding", &ImGuiStyle::WindowRounding},
        {"window_border_size", &ImGuiStyle::WindowBorderSize},
        {"child_rounding", &ImGuiStyle::ChildRounding},
        {"child_border_size", &ImGuiStyle::ChildBorderSize},
        {"popup_rounding", &ImGuiStyle::PopupRounding},
        {"popup_border_size", &ImGuiStyle::PopupBorderSize},
        {"frame_rounding", &ImGuiStyle::FrameRounding},
        {"frame_border_size", &ImGuiStyle::FrameBorderSize},
        {"indent_spacing", &ImGuiStyle::IndentSpacing},
        {"scrollbar_size", &ImGuiStyle::ScrollbarSize},
        {"scrollbar_rounding", &ImGuiStyle::ScrollbarRounding},
        {"grab_min_size", &ImGuiStyle::GrabMinSize},
        {"grab_rounding", &ImGuiStyle::GrabRounding},
        {"image_rounding", &ImGuiStyle::ImageRounding},
        {"image_border_size", &ImGuiStyle::ImageBorderSize},
        {"scrollbar_padding", &ImGuiStyle::ScrollbarPadding},
        {"columns_min_spacing", &ImGuiStyle::ColumnsMinSpacing},
        {"log_slider_deadzone", &ImGuiStyle::LogSliderDeadzone},
        {"tab_rounding", &ImGuiStyle::TabRounding},
        {"tab_border_size", &ImGuiStyle::TabBorderSize},
        {"tab_close_button_min_width_selected", &ImGuiStyle::TabCloseButtonMinWidthSelected},
        {"tab_close_button_min_width_unselected", &ImGuiStyle::TabCloseButtonMinWidthUnselected},
        {"hub_card_rounding", &ImGuiStyle::HubCardRounding},
        {"hub_card_border_size", &ImGuiStyle::HubCardBorderSize},
        {"tab_min_width_base", &ImGuiStyle::TabMinWidthBase},
        {"tab_min_width_shrink", &ImGuiStyle::TabMinWidthShrink},
        {"tab_bar_border_size", &ImGuiStyle::TabBarBorderSize},
        {"tab_bar_overline_size", &ImGuiStyle::TabBarOverlineSize},
        {"tree_lines_size", &ImGuiStyle::TreeLinesSize},
        {"tree_lines_rounding", &ImGuiStyle::TreeLinesRounding},
        {"separator_size", &ImGuiStyle::SeparatorSize},
        {"separator_text_border_size", &ImGuiStyle::SeparatorTextBorderSize},
        {"docking_separator_size", &ImGuiStyle::DockingSeparatorSize},
        {"drag_drop_target_rounding", &ImGuiStyle::DragDropTargetRounding},
        {"drag_drop_target_border_size", &ImGuiStyle::DragDropTargetBorderSize},
        {"drag_drop_target_padding", &ImGuiStyle::DragDropTargetPadding},
        {"color_marker_size", &ImGuiStyle::ColorMarkerSize},
        {"mouse_cursor_scale", &ImGuiStyle::MouseCursorScale},
        {"curve_tessellation_tol", &ImGuiStyle::CurveTessellationTol},
        {"circle_tessellation_max_error", &ImGuiStyle::CircleTessellationMaxError},
        {"table_angled_headers_angle", &ImGuiStyle::TableAngledHeadersAngle}
    };
    return s_Styles;
}

const TVectorStyleNames& GetVectorStyleNames()
{
    static const TVectorStyleNames s_Styles = {
        {"window_padding", &ImGuiStyle::WindowPadding},
        {"window_min_size", &ImGuiStyle::WindowMinSize},
        {"window_title_align", &ImGuiStyle::WindowTitleAlign},
        {"frame_padding", &ImGuiStyle::FramePadding},
        {"item_spacing", &ImGuiStyle::ItemSpacing},
        {"item_inner_spacing", &ImGuiStyle::ItemInnerSpacing},
        {"cell_padding", &ImGuiStyle::CellPadding},
        {"touch_extra_padding", &ImGuiStyle::TouchExtraPadding},
        {"button_text_align", &ImGuiStyle::ButtonTextAlign},
        {"selectable_text_align", &ImGuiStyle::SelectableTextAlign},
        {"table_angled_headers_text_align", &ImGuiStyle::TableAngledHeadersTextAlign},
        {"separator_text_align", &ImGuiStyle::SeparatorTextAlign},
        {"separator_text_padding", &ImGuiStyle::SeparatorTextPadding},
        {"display_window_padding", &ImGuiStyle::DisplayWindowPadding},
        {"display_safe_area_padding", &ImGuiStyle::DisplaySafeAreaPadding}
    };
    return s_Styles;
}

const TBoolStyleNames& GetBoolStyleNames()
{
    static const TBoolStyleNames s_Styles = {
        {"button_gradient", &ImGuiStyle::ButtonGradient},
        {"combo_gradient", &ImGuiStyle::ComboGradient},
        {"docking_tab_gradient", &ImGuiStyle::DockingTabGradient},
        {"docking_node_has_close_button", &ImGuiStyle::DockingNodeHasCloseButton},
        {"anti_aliased_lines", &ImGuiStyle::AntiAliasedLines},
        {"anti_aliased_lines_use_tex", &ImGuiStyle::AntiAliasedLinesUseTex},
        {"anti_aliased_fill", &ImGuiStyle::AntiAliasedFill}
    };
    return s_Styles;
}

const TStyleColorNames& GetStyleColorNames()
{
    static const TStyleColorNames s_Styles = {
        {"button_gradient_top", &ImGuiStyle::ButtonGradientTop},
        {"button_gradient_bottom", &ImGuiStyle::ButtonGradientBottom},
        {"button_hovered_gradient_top", &ImGuiStyle::ButtonHoveredGradientTop},
        {"button_hovered_gradient_bottom", &ImGuiStyle::ButtonHoveredGradientBottom},
        {"button_active_gradient_top", &ImGuiStyle::ButtonActiveGradientTop},
        {"button_active_gradient_bottom", &ImGuiStyle::ButtonActiveGradientBottom},
        {"combo_gradient_top", &ImGuiStyle::ComboGradientTop},
        {"combo_gradient_bottom", &ImGuiStyle::ComboGradientBottom},
        {"combo_hovered_gradient_top", &ImGuiStyle::ComboHoveredGradientTop},
        {"combo_hovered_gradient_bottom", &ImGuiStyle::ComboHoveredGradientBottom},
        {"combo_active_gradient_top", &ImGuiStyle::ComboActiveGradientTop},
        {"combo_active_gradient_bottom", &ImGuiStyle::ComboActiveGradientBottom},
        {"docking_tab_gradient_top", &ImGuiStyle::DockingTabGradientTop},
        {"docking_tab_gradient_bottom", &ImGuiStyle::DockingTabGradientBottom},
        {"docking_tab_selected_gradient_top", &ImGuiStyle::DockingTabSelectedGradientTop},
        {"docking_tab_selected_gradient_bottom", &ImGuiStyle::DockingTabSelectedGradientBottom},
        {"docking_tab_hovered_gradient_top", &ImGuiStyle::DockingTabHoveredGradientTop},
        {"docking_tab_hovered_gradient_bottom", &ImGuiStyle::DockingTabHoveredGradientBottom},
        {"hub_card", &ImGuiStyle::HubCard},
        {"hub_card_selected", &ImGuiStyle::HubCardSelected},
        {"hub_card_hovered", &ImGuiStyle::HubCardHovered},
        {"hub_card_border", &ImGuiStyle::HubCardBorder},
        {"hub_card_selected_border", &ImGuiStyle::HubCardSelectedBorder}
    };
    return s_Styles;
}

bool ParseColor(const json& value, ImVec4& color)
{
    if (!value.is_string())
    {
        return false;
    }
    const std::string hex = value.get<std::string>();
    if ((hex.size() != 7 && hex.size() != 9) || hex[0] != '#')
    {
        return false;
    }

    unsigned int aComponents[4] = {0, 0, 0, 255};
    for (size_t component = 0; component < (hex.size() == 7 ? 3u : 4u); ++component)
    {
        unsigned int parsed = 0;
        for (size_t digit = 0; digit < 2; ++digit)
        {
            const unsigned char c = static_cast<unsigned char>(hex[1 + component * 2 + digit]);
            if (!std::isxdigit(c))
            {
                return false;
            }
            parsed = parsed * 16u + (c <= '9' ? c - '0' :
                (std::tolower(c) - 'a' + 10));
        }
        aComponents[component] = parsed;
    }
    color = ImVec4(aComponents[0] / 255.0f, aComponents[1] / 255.0f,
        aComponents[2] / 255.0f, aComponents[3] / 255.0f);
    return true;
}

bool ParseThemeOverrides(const json& data, SThemeOverrides& overrides, std::string& error)
{
    if (!data.is_object())
    {
        error = "theme root must be an object";
        return false;
    }

    if (data.contains("base"))
    {
        if (!data["base"].is_string())
        {
            error = "'base' must be 'dark' or 'light'";
            return false;
        }
        const std::string base = data["base"].get<std::string>();
        if (base != "dark" && base != "light")
        {
            error = "'base' must be 'dark' or 'light'";
            return false;
        }
        overrides.lightBase = base == "light";
    }

    if (data.contains("font"))
    {
        const json& font = data["font"];
        if (!font.is_object())
        {
            error = "'font' must be an object";
            return false;
        }
        for (const auto& [name, value] : font.items())
        {
            if (name == "path")
            {
                if (!value.is_string() || value.get<std::string>().empty())
                {
                    error = "font path must be a non-empty string";
                    return false;
                }
                overrides.fontPath = value.get<std::string>();
            }
            else if (name == "size")
            {
                if (!value.is_number())
                {
                    error = "font size must be a number";
                    return false;
                }
                overrides.fontSize = value.get<float>();
                if (!std::isfinite(overrides.fontSize) ||
                    overrides.fontSize < 6.0f || overrides.fontSize > 64.0f)
                {
                    error = "font size must be between 6 and 64 pixels";
                    return false;
                }
            }
            else if (name == "scale")
            {
                if (!value.is_number())
                {
                    error = "font scale must be a number";
                    return false;
                }
                overrides.fontScale = value.get<float>();
                if (!std::isfinite(overrides.fontScale) ||
                    overrides.fontScale < 0.5f || overrides.fontScale > 3.0f)
                {
                    error = "font scale must be between 0.5 and 3.0";
                    return false;
                }
            }
            else
            {
                error = "unknown font property: " + name;
                return false;
            }
        }
    }

    if (data.contains("colors"))
    {
        if (!data["colors"].is_object())
        {
            error = "'colors' must be an object";
            return false;
        }
        for (const auto& [name, value] : data["colors"].items())
        {
            const auto color = GetColorNames().find(name);
            ImVec4 parsed;
            if (color == GetColorNames().end() || !ParseColor(value, parsed))
            {
                error = "unknown color or invalid HEX value: " + name;
                return false;
            }
            overrides.vColors.emplace_back(color->second, parsed);
        }
    }

    if (data.contains("style"))
    {
        if (!data["style"].is_object())
        {
            error = "'style' must be an object";
            return false;
        }
        for (const auto& [name, value] : data["style"].items())
        {
            const auto scalar = GetFloatStyleNames().find(name);
            const auto vector = GetVectorStyleNames().find(name);
            const auto boolean = GetBoolStyleNames().find(name);
            const auto styleColor = GetStyleColorNames().find(name);
            if (scalar != GetFloatStyleNames().end())
            {
                if (!value.is_number())
                {
                    error = "style value must be a number: " + name;
                    return false;
                }
                const float parsed = value.get<float>();
                const bool isNegativeAngle = name == "table_angled_headers_angle";
                const bool isTabCloseWidth = name == "tab_close_button_min_width_selected" ||
                    name == "tab_close_button_min_width_unselected";
                const bool outsideRange = isNegativeAngle
                    ? parsed < -50.0f || parsed > 50.0f
                    : isTabCloseWidth
                        ? parsed < -1.0f || parsed > 1000.0f
                        : parsed < 0.0f || parsed > 1000.0f;
                if (!std::isfinite(parsed) || outsideRange)
                {
                    error = "style value is outside the supported range: " + name;
                    return false;
                }
                overrides.vFloatStyle.emplace_back(scalar->second, parsed);
            }
            else if (vector != GetVectorStyleNames().end())
            {
                if (!value.is_array() || value.size() != 2 ||
                    !value[0].is_number() || !value[1].is_number())
                {
                    error = "style value must be a two-number array: " + name;
                    return false;
                }
                const float x = value[0].get<float>();
                const float y = value[1].get<float>();
                if (!std::isfinite(x) || !std::isfinite(y) ||
                    x < 0.0f || y < 0.0f || x > 1000.0f || y > 1000.0f)
                {
                    error = "style value is outside the supported range: " + name;
                    return false;
                }
                overrides.vVectorStyle.emplace_back(vector->second, ImVec2(x, y));
            }
            else if (boolean != GetBoolStyleNames().end())
            {
                if (!value.is_boolean())
                {
                    error = "style value must be a boolean: " + name;
                    return false;
                }
                overrides.vBoolStyle.emplace_back(boolean->second, value.get<bool>());
            }
            else if (styleColor != GetStyleColorNames().end())
            {
                ImVec4 parsed;
                if (!ParseColor(value, parsed))
                {
                    error = "style color must be a HEX value: " + name;
                    return false;
                }
                overrides.vStyleColors.emplace_back(styleColor->second, parsed);
            }
            else
            {
                error = "unknown style property: " + name;
                return false;
            }
        }
    }
    return true;
}

bool LanguageUsesMsPgothic(const std::string& languageCode)
{
    return languageCode == "japanese" ||
        languageCode == "korean" ||
        languageCode == "simplified_chinese" ||
        languageCode == "traditional_chinese";
}

const ImWchar* GetMsPgothicGlyphRanges(ImGuiIO& io, const std::string& languageCode)
{
    if (languageCode == "japanese")
    {
        return io.Fonts->GetGlyphRangesJapanese();
    }
    if (languageCode == "korean")
    {
        return io.Fonts->GetGlyphRangesKorean();
    }
    if (languageCode == "simplified_chinese")
    {
        return io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    }
    if (languageCode == "traditional_chinese")
    {
        return io.Fonts->GetGlyphRangesChineseFull();
    }
    return nullptr;
}

bool ReloadEditorFonts(const SThemeOverrides& overrides, std::string& error)
{
    const std::string languageCode = CLanguageManager::Get().m_Current;
    const std::string languageFontPath = CLanguageManager::Get().EditorFontPath();
    const std::string fontPath = overrides.fontPath.empty() ? languageFontPath : overrides.fontPath;
    std::error_code fileError;
    if (!fs::is_regular_file(fontPath, fileError))
    {
        error = "font file not found: " + fontPath;
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFont* pDefaultFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), overrides.fontSize);
    if (!pDefaultFont)
    {
        error = "failed to load font: " + fontPath;
        io.Fonts->Clear();
        pDefaultFont = io.Fonts->AddFontFromFileTTF(languageFontPath.c_str(), 16.0f);
        if (!pDefaultFont)
        {
            pDefaultFont = io.Fonts->AddFontDefault();
        }
        io.FontDefault = pDefaultFont;
        io.Fonts->Build();
        return false;
    }

    const auto mergeFont = [&io, &overrides](const std::string& path, const ImWchar* pGlyphRanges)
    {
        if (path.empty() || path == overrides.fontPath)
        {
            return;
        }
        ImFontConfig config = {};
        config.MergeMode = true;
        config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(path.c_str(), overrides.fontSize, &config, pGlyphRanges);
    };

    const std::string mergeFontPath = CLanguageManager::Get().EditorFontMergePath();
    if (!mergeFontPath.empty())
    {
        mergeFont(mergeFontPath, nullptr);
    }
    if (LanguageUsesMsPgothic(languageCode) &&
        fontPath != "assets/MS-Pgothic-Regular.ttf")
    {
        mergeFont("assets/MS-Pgothic-Regular.ttf",
            GetMsPgothicGlyphRanges(io, languageCode));
    }

    io.FontDefault = pDefaultFont;
    if (!io.Fonts->Build())
    {
        error = "failed to build the font atlas";
        return false;
    }
    return true;
}

bool QueueEditorFonts(const std::string& themeId, const SThemeOverrides& overrides,
    std::string& error)
{
    const std::string fontPath = overrides.fontPath.empty()
        ? CLanguageManager::Get().EditorFontPath()
        : overrides.fontPath;
    std::error_code fileError;
    if (!fs::is_regular_file(fontPath, fileError))
    {
        error = "font file not found: " + fontPath;
        return false;
    }

    g_PendingFontOverrides = overrides;
    g_PendingFontThemeId = themeId;
    g_HasPendingFonts = true;
    return true;
}

bool IsThemeId(const std::string& id)
{
    if (id.empty())
    {
        return false;
    }
    for (const unsigned char c : id)
    {
        if (!std::isalnum(c) && c != '-' && c != '_')
        {
            return false;
        }
    }
    return true;
}

std::string FriendlyThemeName(const std::string& id)
{
    std::string name = id;
    std::replace(name.begin(), name.end(), '-', ' ');
    std::replace(name.begin(), name.end(), '_', ' ');
    if (!name.empty())
    {
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    }
    return name;
}

void ReportThemeError(const std::string& themeId, const std::string& reason)
{
    std::cerr << "Failed to load editor theme '" << themeId << "': " << reason << '\n';
}

} // namespace

void CThemeManager::Apply(bool lightTheme)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.ButtonGradient = true;
    style.ComboGradient = false;
    style.DockingTabGradient = false;
    style.ButtonGradientTop = ImVec4(96.0f / 255.0f, 101.0f / 255.0f, 107.0f / 255.0f, 1.0f);
    style.ButtonGradientBottom = ImVec4(68.0f / 255.0f, 73.0f / 255.0f, 79.0f / 255.0f, 1.0f);
    style.ButtonHoveredGradientTop = ImVec4(107.0f / 255.0f, 113.0f / 255.0f, 120.0f / 255.0f, 1.0f);
    style.ButtonHoveredGradientBottom = ImVec4(76.0f / 255.0f, 81.0f / 255.0f, 88.0f / 255.0f, 1.0f);
    style.ButtonActiveGradientTop = ImVec4(63.0f / 255.0f, 67.0f / 255.0f, 72.0f / 255.0f, 1.0f);
    style.ButtonActiveGradientBottom = ImVec4(54.0f / 255.0f, 58.0f / 255.0f, 63.0f / 255.0f, 1.0f);
    style.ComboGradientTop = ImVec4(0.15f, 0.16f, 0.18f, 1.0f);
    style.ComboGradientBottom = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.ComboHoveredGradientTop = ImVec4(0.23f, 0.25f, 0.27f, 1.0f);
    style.ComboHoveredGradientBottom = ImVec4(0.18f, 0.20f, 0.22f, 1.0f);
    style.ComboActiveGradientTop = ImVec4(0.20f, 0.22f, 0.24f, 1.0f);
    style.ComboActiveGradientBottom = ImVec4(0.15f, 0.17f, 0.19f, 1.0f);
    style.DockingTabGradientTop = ImVec4(0.20f, 0.21f, 0.23f, 1.0f);
    style.DockingTabGradientBottom = ImVec4(0.17f, 0.18f, 0.20f, 1.0f);
    style.DockingTabSelectedGradientTop = ImVec4(0.27f, 0.30f, 0.34f, 1.0f);
    style.DockingTabSelectedGradientBottom = ImVec4(0.22f, 0.24f, 0.27f, 1.0f);
    style.DockingTabHoveredGradientTop = ImVec4(0.36f, 0.39f, 0.43f, 1.0f);
    style.DockingTabHoveredGradientBottom = ImVec4(0.29f, 0.32f, 0.36f, 1.0f);
    style.HubCard = ImVec4(26.0f / 255.0f, 28.0f / 255.0f, 31.0f / 255.0f, 1.0f);
    style.HubCardSelected = ImVec4(30.0f / 255.0f, 80.0f / 255.0f, 140.0f / 255.0f, 1.0f);
    style.HubCardHovered = ImVec4(40.0f / 255.0f, 42.0f / 255.0f, 46.0f / 255.0f, 180.0f / 255.0f);
    style.HubCardBorder = ImVec4(50.0f / 255.0f, 52.0f / 255.0f, 56.0f / 255.0f, 1.0f);
    style.HubCardSelectedBorder = ImVec4(50.0f / 255.0f, 130.0f / 255.0f, 220.0f / 255.0f, 1.0f);
    style.HubCardRounding = 0.0f;
    style.HubCardBorderSize = 1.0f;

    if (lightTheme)
    {
        ImGui::StyleColorsLight();
        style.HubCard = ImVec4(239.0f / 255.0f, 242.0f / 255.0f, 247.0f / 255.0f, 1.0f);
        style.HubCardSelected = ImVec4(190.0f / 255.0f, 214.0f / 255.0f, 245.0f / 255.0f, 1.0f);
        style.HubCardHovered = ImVec4(215.0f / 255.0f, 226.0f / 255.0f, 242.0f / 255.0f, 220.0f / 255.0f);
        style.HubCardBorder = ImVec4(190.0f / 255.0f, 198.0f / 255.0f, 210.0f / 255.0f, 1.0f);
        style.HubCardSelectedBorder = ImVec4(75.0f / 255.0f, 130.0f / 255.0f, 205.0f / 255.0f, 1.0f);
        style.ButtonGradientTop = ImVec4(246.0f / 255.0f, 248.0f / 255.0f, 252.0f / 255.0f, 1.0f);
        style.ButtonGradientBottom = ImVec4(190.0f / 255.0f, 204.0f / 255.0f, 224.0f / 255.0f, 1.0f);
        style.ButtonHoveredGradientTop = ImVec4(225.0f / 255.0f, 234.0f / 255.0f, 247.0f / 255.0f, 1.0f);
        style.ButtonHoveredGradientBottom = ImVec4(160.0f / 255.0f, 184.0f / 255.0f, 217.0f / 255.0f, 1.0f);
        style.ButtonActiveGradientTop = ImVec4(155.0f / 255.0f, 181.0f / 255.0f, 218.0f / 255.0f, 1.0f);
        style.ButtonActiveGradientBottom = ImVec4(104.0f / 255.0f, 140.0f / 255.0f, 191.0f / 255.0f, 1.0f);
        style.ComboGradientTop = ImVec4(0.86f, 0.88f, 0.91f, 1.0f);
        style.ComboGradientBottom = ImVec4(0.79f, 0.82f, 0.87f, 1.0f);
        style.ComboHoveredGradientTop = ImVec4(0.79f, 0.84f, 0.91f, 1.0f);
        style.ComboHoveredGradientBottom = ImVec4(0.70f, 0.77f, 0.86f, 1.0f);
        style.ComboActiveGradientTop = ImVec4(0.70f, 0.79f, 0.92f, 1.0f);
        style.ComboActiveGradientBottom = ImVec4(0.60f, 0.70f, 0.84f, 1.0f);
        style.WindowRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;
        style.FramePadding = ImVec2(6, 3);
        style.ItemSpacing = ImVec2(6, 4);

        ImVec4* pColors = style.Colors;
        pColors[ImGuiCol_Text] = ImVec4(0.07f, 0.09f, 0.12f, 1.0f);
        pColors[ImGuiCol_TextDisabled] = ImVec4(0.30f, 0.33f, 0.38f, 1.0f);
        pColors[ImGuiCol_WindowBg] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
        pColors[ImGuiCol_ChildBg] = ImVec4(0.97f, 0.98f, 0.99f, 1.0f);
        pColors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        pColors[ImGuiCol_Border] = ImVec4(0.70f, 0.73f, 0.78f, 1.0f);
        pColors[ImGuiCol_Separator] = ImVec4(0.78f, 0.80f, 0.84f, 1.0f);
        pColors[ImGuiCol_FrameBg] = ImVec4(0.86f, 0.88f, 0.91f, 1.0f);
        pColors[ImGuiCol_FrameBgHovered] = ImVec4(0.79f, 0.84f, 0.91f, 1.0f);
        pColors[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.79f, 0.92f, 1.0f);
        pColors[ImGuiCol_TitleBg] = ImVec4(0.84f, 0.86f, 0.89f, 1.0f);
        pColors[ImGuiCol_TitleBgActive] = ImVec4(0.78f, 0.82f, 0.88f, 1.0f);
        pColors[ImGuiCol_MenuBarBg] = ImVec4(0.88f, 0.90f, 0.93f, 1.0f);
        pColors[ImGuiCol_Button] = ImVec4(0.82f, 0.85f, 0.90f, 1.0f);
        pColors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.79f, 0.91f, 1.0f);
        pColors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.71f, 0.87f, 1.0f);
        pColors[ImGuiCol_Header] = ImVec4(0.84f, 0.88f, 0.94f, 1.0f);
        pColors[ImGuiCol_HeaderHovered] = ImVec4(0.73f, 0.82f, 0.94f, 1.0f);
        pColors[ImGuiCol_HeaderActive] = ImVec4(0.62f, 0.75f, 0.92f, 1.0f);
        pColors[ImGuiCol_Tab] = ImVec4(0.84f, 0.87f, 0.91f, 1.0f);
        pColors[ImGuiCol_TabHovered] = ImVec4(0.72f, 0.81f, 0.93f, 1.0f);
        pColors[ImGuiCol_TabActive] = ImVec4(0.96f, 0.97f, 0.99f, 1.0f);
        pColors[ImGuiCol_TabUnfocused] = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
        pColors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.92f, 0.94f, 0.97f, 1.0f);
        pColors[ImGuiCol_CheckMark] = ImVec4(0.12f, 0.45f, 0.85f, 1.0f);
        pColors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.56f, 0.84f, 1.0f);
        pColors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.42f, 0.73f, 1.0f);
        pColors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.58f, 0.91f, 0.35f);
        pColors[ImGuiCol_DockingPreview] = ImVec4(0.25f, 0.55f, 0.90f, 0.35f);
        return;
    }

    // ====== SHAPES ======
    style.WindowRounding = 0.0f;
    style.FrameRounding  = 0.0f;
    style.PopupRounding  = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;

    style.FramePadding = ImVec2(6, 3);
    style.ItemSpacing = ImVec2(6, 4);

    ImVec4* pColors = style.Colors;

    // ====== GLOBAL ======
    pColors[ImGuiCol_Text]           = ImVec4(0.80f, 0.82f, 0.85f, 1.00f); // #c9cdd1
    pColors[ImGuiCol_TextDisabled]   = ImVec4(0.54f, 0.58f, 0.63f, 1.00f);
    pColors[ImGuiCol_WindowBg]       = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #2a2c2f
    pColors[ImGuiCol_ChildBg]        = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
    pColors[ImGuiCol_PopupBg]        = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #32353a
    pColors[ImGuiCol_DockingEmptyBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);

    // ====== BORDERS ======
    pColors[ImGuiCol_Border]         = ImVec4(0.27f, 0.28f, 0.30f, 1.00f); // #44484d
    pColors[ImGuiCol_Separator]      = ImVec4(0.24f, 0.25f, 0.27f, 1.00f);

    // ====== FRAMES (inputs, edits) ======
    pColors[ImGuiCol_FrameBg]        = ImVec4(0.14f, 0.15f, 0.16f, 1.00f); // #24272a
    pColors[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045 hover
    pColors[ImGuiCol_FrameBgActive]  = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff
    pColors[ImGuiCol_InputTextCursor]= ImVec4(0.93f, 0.95f, 0.98f, 1.00f);

    // ====== TITLE / MENUBAR ======
    pColors[ImGuiCol_TitleBg]        = ImVec4(0.19f, 0.20f, 0.22f, 1.00f); // #31343a
    pColors[ImGuiCol_TitleBgActive]  = ImVec4(0.24f, 0.26f, 0.29f, 1.00f);
    pColors[ImGuiCol_MenuBarBg]      = ImVec4(0.19f, 0.20f, 0.22f, 1.00f);

    // ====== BUTTONS ======
    pColors[ImGuiCol_Button]         = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #51565c
    pColors[ImGuiCol_ButtonHovered]  = ImVec4(0.36f, 0.38f, 0.41f, 1.00f); // #5C6169 hover
    pColors[ImGuiCol_ButtonActive]   = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // #3D4247 pressed

    // ====== HEADERS (Tree, Selectable) ======
    pColors[ImGuiCol_Header]         = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #292B2E
    pColors[ImGuiCol_HeaderHovered]  = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045
    pColors[ImGuiCol_HeaderActive]   = ImVec4(0.18f, 0.53f, 0.78f, 1.00f); // #2e87c7ff selection

    // ====== SELECTION ======
    pColors[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.47f, 0.78f, 0.35f); // #2e78c759

    // ====== SCROLLBAR ======
    pColors[ImGuiCol_ScrollbarBg]    = ImVec4(0.18f, 0.20f, 0.22f, 1.00f); // #2f3337
    pColors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.33f, 0.36f, 0.38f, 1.00f); // #555b62
    pColors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    pColors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.53f, 0.56f, 1.00f); // #80878FFF

    // ====== TABS ======
    pColors[ImGuiCol_Tab]            = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #333538FF
    pColors[ImGuiCol_TabHovered]     = ImVec4(0.36f, 0.39f, 0.43f, 1.00f);
    pColors[ImGuiCol_TabActive]      = ImVec4(0.27f, 0.30f, 0.34f, 1.00f);
    pColors[ImGuiCol_TabUnfocused]   = ImVec4(0.17f, 0.18f, 0.20f, 1.00f);
    pColors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);

    // ====== CHECKBOX ======
    pColors[ImGuiCol_CheckMark]      = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== RESIZE GRIP ======
    pColors[ImGuiCol_ResizeGrip]         = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #4D5259FF
    pColors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    pColors[ImGuiCol_ResizeGripActive]   = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== DOCKING ======
    pColors[ImGuiCol_DockingPreview] = ImVec4(0.78f, 0.52f, 0.17f, 0.4f); // #c9802b66
    pColors[ImGuiCol_NavCursor]      = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
    pColors[ImGuiCol_TextLink]       = ImVec4(0.40f, 0.72f, 0.98f, 1.00f);
}

bool CThemeManager::Apply(const std::string& themeId)
{
    if (themeId == "quark-dark" || themeId == "quark-light")
    {
        SThemeOverrides overrides;
        std::string error;
        if (!QueueEditorFonts(themeId, overrides, error))
        {
            ReportThemeError(themeId, error);
            return false;
        }
        Apply(themeId == "quark-light");
        g_AppliedFontScale = 1.0f;
        return true;
    }

    if (!IsThemeId(themeId))
    {
        ReportThemeError(themeId, "invalid theme id");
        return false;
    }

    const fs::path themePath = fs::path("assets") / "themes" / (themeId + ".json");
    std::ifstream input(themePath);
    if (!input.is_open())
    {
        ReportThemeError(themeId, "file not found: " + themePath.string());
        return false;
    }

    json data;
    try
    {
        input >> data;
    }
    catch (const std::exception& exception)
    {
        ReportThemeError(themeId, exception.what());
        return false;
    }

    SThemeOverrides overrides;
    std::string error;
    if (!ParseThemeOverrides(data, overrides, error))
    {
        ReportThemeError(themeId, error);
        return false;
    }

    if (!QueueEditorFonts(themeId, overrides, error))
    {
        ReportThemeError(themeId, error);
        return false;
    }

    Apply(overrides.lightBase);
    ImGuiStyle& style = ImGui::GetStyle();
    for (const auto& [color, value] : overrides.vColors)
    {
        style.Colors[color] = value;
    }
    for (const auto& [pStyleField, value] : overrides.vFloatStyle)
    {
        style.*pStyleField = value;
    }
    for (const auto& [pStyleField, value] : overrides.vVectorStyle)
    {
        style.*pStyleField = value;
    }
    for (const auto& [pStyleField, value] : overrides.vBoolStyle)
    {
        style.*pStyleField = value;
    }
    for (const auto& [pStyleField, value] : overrides.vStyleColors)
    {
        style.*pStyleField = value;
    }
    g_AppliedFontScale = overrides.fontScale;
    return true;
}

bool CThemeManager::ReloadFonts(const std::string& themeId)
{
    SThemeOverrides overrides;
    std::string error;
    if (themeId != "quark-dark" && themeId != "quark-light")
    {
        if (!IsThemeId(themeId))
        {
            ReportThemeError(themeId, "invalid theme id");
            return false;
        }

        const fs::path themePath = fs::path("assets") / "themes" / (themeId + ".json");
        std::ifstream input(themePath);
        if (!input.is_open())
        {
            ReportThemeError(themeId, "file not found: " + themePath.string());
            return false;
        }

        json data;
        try
        {
            input >> data;
        }
        catch (const std::exception& exception)
        {
            ReportThemeError(themeId, exception.what());
            return false;
        }
        if (!ParseThemeOverrides(data, overrides, error))
        {
            ReportThemeError(themeId, error);
            return false;
        }
    }

    if (!QueueEditorFonts(themeId, overrides, error))
    {
        ReportThemeError(themeId, error);
        return false;
    }
    return true;
}

float CThemeManager::GetAppliedFontScale()
{
    return g_AppliedFontScale;
}

void CThemeManager::ProcessPendingFonts()
{
    if (!g_HasPendingFonts)
    {
        return;
    }

    std::string error;
    if (!ReloadEditorFonts(g_PendingFontOverrides, error))
    {
        ReportThemeError(g_PendingFontThemeId, error);
    }
    g_HasPendingFonts = false;
    g_PendingFontThemeId.clear();
}

std::vector<SEditorTheme> CThemeManager::GetAvailableThemes()
{
    std::vector<SEditorTheme> vThemes = {
        {"quark-dark", "Quark Dark"},
        {"quark-light", "Quark Light"}
    };

    std::error_code error;
    const fs::path themeDirectory = fs::path("assets") / "themes";
    if (!fs::exists(themeDirectory, error))
    {
        if (error)
        {
            ReportThemeError("directory", error.message());
        }
        return vThemes;
    }

    fs::directory_iterator iterator(themeDirectory, error);
    const fs::directory_iterator end;
    if (error)
    {
        ReportThemeError("directory", error.message());
        return vThemes;
    }
    for (; iterator != end; iterator.increment(error))
    {
        if (error)
        {
            ReportThemeError("directory", error.message());
            break;
        }
        if (!iterator->is_regular_file(error) || error ||
            iterator->path().extension() != ".json")
        {
            error.clear();
            continue;
        }

        const std::string id = iterator->path().stem().string();
        if (!IsThemeId(id) || std::any_of(vThemes.begin(), vThemes.end(),
            [&id](const SEditorTheme& theme) { return theme.id == id; }))
        {
            continue;
        }

        std::string name = FriendlyThemeName(id);
        std::ifstream input(iterator->path());
        if (input.is_open())
        {
            try
            {
                json data;
                input >> data;
                if (data.is_object() && data.contains("name") && data["name"].is_string())
                {
                    const std::string configuredName = data["name"].get<std::string>();
                    if (!configuredName.empty())
                    {
                        name = configuredName;
                    }
                }
            }
            catch (const std::exception& exception)
            {
                ReportThemeError(id, exception.what());
            }
        }
        vThemes.push_back({id, name});
    }

    std::sort(vThemes.begin() + 2, vThemes.end(),
        [](const SEditorTheme& a, const SEditorTheme& b) { return a.name < b.name; });
    return vThemes;
}