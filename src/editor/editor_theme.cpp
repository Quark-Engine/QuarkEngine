#include "editor/editor_theme.h"

#include "imgui.h"
#include "language_manager.h"

#include "nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

    using Json = nlohmann::json;
    namespace fs = std::filesystem;

    struct SComponentStateOverride
    {
        bool m_HasColor = false;
        ImVec4 m_Color;
        bool m_HasGradient = false;
        ImVec4 m_GradientTop;
        ImVec4 m_GradientBottom;
        bool m_HasAlpha = false;
        float m_Alpha = 1.0f;
    };

    struct SComponentVariantOverride
    {
        std::string m_Component;
        std::string m_Variant;
        std::array<SComponentStateOverride, 4> m_aStates;
    };

    struct SThemeOverrides
    {
        bool m_LightBase = false;
        std::string m_FontPath;
        float m_FontSize = 16.0f;
        float m_FontScale = 1.0f;
        std::vector<SComponentVariantOverride> m_vComponents;
        std::vector<std::pair<ImGuiCol, ImVec4>> m_vColors;
        std::vector<std::pair<float ImGuiStyle::*, float>> m_vFloatStyle;
        std::vector<std::pair<ImVec2 ImGuiStyle::*, ImVec2>> m_vVectorStyle;
        std::vector<std::pair<bool ImGuiStyle::*, bool>> m_vBoolStyle;
        std::vector<std::pair<ImVec4 ImGuiStyle::*, ImVec4>> m_vStyleColors;
    };

    struct SThemeVariantRestore
    {
        int m_ColorPushCount = 0;
        bool m_HasAlpha = false;
        bool m_LegacyWidgetStyle = false;
        bool m_HasButtonGradient = false;
        bool m_ButtonGradient = false;
        std::array<ImVec4, 3> m_aButtonGradientTop;
        std::array<ImVec4, 3> m_aButtonGradientBottom;
    };

    using TColorNames = std::unordered_map<std::string, ImGuiCol>;
    using TFloatStyleNames = std::unordered_map<std::string, float ImGuiStyle::*>;
    using TVectorStyleNames = std::unordered_map<std::string, ImVec2 ImGuiStyle::*>;
    using TBoolStyleNames = std::unordered_map<std::string, bool ImGuiStyle::*>;
    using TStyleColorNames = std::unordered_map<std::string, ImVec4 ImGuiStyle::*>;

    float s_AppliedFontScale = 1.0f;
    SThemeOverrides s_PendingFontOverrides;
    std::string s_PendingFontThemeId;
    bool s_HasPendingFonts = false;

    const TColorNames& GetColorNames()
    {
        static const TColorNames s_Colors = {
            { "Text", ImGuiCol_Text }, { "TextDisabled", ImGuiCol_TextDisabled }, { "WindowBg", ImGuiCol_WindowBg }, { "ChildBg", ImGuiCol_ChildBg }, { "PopupBg", ImGuiCol_PopupBg }, { "Border", ImGuiCol_Border }, { "BorderShadow", ImGuiCol_BorderShadow }, { "FrameBg", ImGuiCol_FrameBg }, { "FrameBgHovered", ImGuiCol_FrameBgHovered }, { "FrameBgActive", ImGuiCol_FrameBgActive }, { "TitleBg", ImGuiCol_TitleBg }, { "TitleBgActive", ImGuiCol_TitleBgActive }, { "TitleBgCollapsed", ImGuiCol_TitleBgCollapsed }, { "MenuBarBg", ImGuiCol_MenuBarBg }, { "ScrollbarBg", ImGuiCol_ScrollbarBg }, { "ScrollbarGrab", ImGuiCol_ScrollbarGrab }, { "ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered }, { "ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive }, { "CheckMark", ImGuiCol_CheckMark }, { "SliderGrab", ImGuiCol_SliderGrab }, { "SliderGrabActive", ImGuiCol_SliderGrabActive }, { "Button", ImGuiCol_Button }, { "ButtonHovered", ImGuiCol_ButtonHovered }, { "ButtonActive", ImGuiCol_ButtonActive }, { "Header", ImGuiCol_Header }, { "HeaderHovered", ImGuiCol_HeaderHovered }, { "HeaderActive", ImGuiCol_HeaderActive }, { "Separator", ImGuiCol_Separator }, { "SeparatorHovered", ImGuiCol_SeparatorHovered }, { "SeparatorActive", ImGuiCol_SeparatorActive }, { "ResizeGrip", ImGuiCol_ResizeGrip }, { "ResizeGripHovered", ImGuiCol_ResizeGripHovered }, { "ResizeGripActive", ImGuiCol_ResizeGripActive }, { "InputTextCursor", ImGuiCol_InputTextCursor }, { "TabHovered", ImGuiCol_TabHovered }, { "Tab", ImGuiCol_Tab }, { "TabSelected", ImGuiCol_TabSelected }, { "TabSelectedOverline", ImGuiCol_TabSelectedOverline }, { "TabDimmed", ImGuiCol_TabDimmed }, { "TabDimmedSelected", ImGuiCol_TabDimmedSelected }, { "TabDimmedSelectedOverline", ImGuiCol_TabDimmedSelectedOverline }, { "DockingPreview", ImGuiCol_DockingPreview }, { "DockingEmptyBg", ImGuiCol_DockingEmptyBg }, { "PlotLines", ImGuiCol_PlotLines }, { "PlotLinesHovered", ImGuiCol_PlotLinesHovered }, { "PlotHistogram", ImGuiCol_PlotHistogram }, { "PlotHistogramHovered", ImGuiCol_PlotHistogramHovered }, { "TableHeaderBg", ImGuiCol_TableHeaderBg }, { "TableBorderStrong", ImGuiCol_TableBorderStrong }, { "TableBorderLight", ImGuiCol_TableBorderLight }, { "TableRowBg", ImGuiCol_TableRowBg }, { "TableRowBgAlt", ImGuiCol_TableRowBgAlt }, { "TextLink", ImGuiCol_TextLink }, { "TextSelectedBg", ImGuiCol_TextSelectedBg }, { "TreeLines", ImGuiCol_TreeLines }, { "DragDropTarget", ImGuiCol_DragDropTarget }, { "DragDropTargetBg", ImGuiCol_DragDropTargetBg }, { "UnsavedMarker", ImGuiCol_UnsavedMarker }, { "NavCursor", ImGuiCol_NavCursor }, { "NavWindowingHighlight", ImGuiCol_NavWindowingHighlight }, { "NavWindowingDimBg", ImGuiCol_NavWindowingDimBg }, { "ModalWindowDimBg", ImGuiCol_ModalWindowDimBg }, { "TabActive", ImGuiCol_TabSelected }, { "TabUnfocused", ImGuiCol_TabDimmed }, { "TabUnfocusedActive", ImGuiCol_TabDimmedSelected }
        };
        return s_Colors;
    }

    const TFloatStyleNames& GetFloatStyleNames()
    {
        static const TFloatStyleNames s_Styles = {
            { "alpha", &ImGuiStyle::Alpha }, { "disabled_alpha", &ImGuiStyle::DisabledAlpha }, { "window_rounding", &ImGuiStyle::WindowRounding }, { "window_border_size", &ImGuiStyle::WindowBorderSize }, { "child_rounding", &ImGuiStyle::ChildRounding }, { "child_border_size", &ImGuiStyle::ChildBorderSize }, { "popup_rounding", &ImGuiStyle::PopupRounding }, { "popup_border_size", &ImGuiStyle::PopupBorderSize }, { "frame_rounding", &ImGuiStyle::FrameRounding }, { "frame_border_size", &ImGuiStyle::FrameBorderSize }, { "indent_spacing", &ImGuiStyle::IndentSpacing }, { "scrollbar_size", &ImGuiStyle::ScrollbarSize }, { "scrollbar_rounding", &ImGuiStyle::ScrollbarRounding }, { "grab_min_size", &ImGuiStyle::GrabMinSize }, { "grab_rounding", &ImGuiStyle::GrabRounding }, { "image_rounding", &ImGuiStyle::ImageRounding }, { "image_border_size", &ImGuiStyle::ImageBorderSize }, { "scrollbar_padding", &ImGuiStyle::ScrollbarPadding }, { "columns_min_spacing", &ImGuiStyle::ColumnsMinSpacing }, { "log_slider_deadzone", &ImGuiStyle::LogSliderDeadzone }, { "tab_rounding", &ImGuiStyle::TabRounding }, { "tab_border_size", &ImGuiStyle::TabBorderSize }, { "tab_close_button_min_width_selected", &ImGuiStyle::TabCloseButtonMinWidthSelected }, { "tab_close_button_min_width_unselected", &ImGuiStyle::TabCloseButtonMinWidthUnselected }, { "hub_card_rounding", &ImGuiStyle::HubCardRounding }, { "hub_card_border_size", &ImGuiStyle::HubCardBorderSize }, { "tab_min_width_base", &ImGuiStyle::TabMinWidthBase }, { "tab_min_width_shrink", &ImGuiStyle::TabMinWidthShrink }, { "tab_bar_border_size", &ImGuiStyle::TabBarBorderSize }, { "tab_bar_overline_size", &ImGuiStyle::TabBarOverlineSize }, { "tree_lines_size", &ImGuiStyle::TreeLinesSize }, { "tree_lines_rounding", &ImGuiStyle::TreeLinesRounding }, { "separator_size", &ImGuiStyle::SeparatorSize }, { "separator_text_border_size", &ImGuiStyle::SeparatorTextBorderSize }, { "docking_separator_size", &ImGuiStyle::DockingSeparatorSize }, { "drag_drop_target_rounding", &ImGuiStyle::DragDropTargetRounding }, { "drag_drop_target_border_size", &ImGuiStyle::DragDropTargetBorderSize }, { "drag_drop_target_padding", &ImGuiStyle::DragDropTargetPadding }, { "color_marker_size", &ImGuiStyle::ColorMarkerSize }, { "mouse_cursor_scale", &ImGuiStyle::MouseCursorScale }, { "curve_tessellation_tol", &ImGuiStyle::CurveTessellationTol }, { "circle_tessellation_max_error", &ImGuiStyle::CircleTessellationMaxError }, { "table_angled_headers_angle", &ImGuiStyle::TableAngledHeadersAngle }
        };
        return s_Styles;
    }

    const TVectorStyleNames& GetVectorStyleNames()
    {
        static const TVectorStyleNames s_Styles = {
            { "window_padding", &ImGuiStyle::WindowPadding },
            { "window_min_size", &ImGuiStyle::WindowMinSize },
            { "window_title_align", &ImGuiStyle::WindowTitleAlign },
            { "frame_padding", &ImGuiStyle::FramePadding },
            { "item_spacing", &ImGuiStyle::ItemSpacing },
            { "item_inner_spacing", &ImGuiStyle::ItemInnerSpacing },
            { "cell_padding", &ImGuiStyle::CellPadding },
            { "touch_extra_padding", &ImGuiStyle::TouchExtraPadding },
            { "button_text_align", &ImGuiStyle::ButtonTextAlign },
            { "selectable_text_align", &ImGuiStyle::SelectableTextAlign },
            { "table_angled_headers_text_align", &ImGuiStyle::TableAngledHeadersTextAlign },
            { "separator_text_align", &ImGuiStyle::SeparatorTextAlign },
            { "separator_text_padding", &ImGuiStyle::SeparatorTextPadding },
            { "display_window_padding", &ImGuiStyle::DisplayWindowPadding },
            { "display_safe_area_padding", &ImGuiStyle::DisplaySafeAreaPadding }
        };
        return s_Styles;
    }

    const TBoolStyleNames& GetBoolStyleNames()
    {
        static const TBoolStyleNames s_Styles = {
            { "button_gradient", &ImGuiStyle::ButtonGradient },
            { "combo_gradient", &ImGuiStyle::ComboGradient },
            { "docking_tab_gradient", &ImGuiStyle::DockingTabGradient },
            { "docking_node_has_close_button", &ImGuiStyle::DockingNodeHasCloseButton },
            { "anti_aliased_lines", &ImGuiStyle::AntiAliasedLines },
            { "anti_aliased_lines_use_tex", &ImGuiStyle::AntiAliasedLinesUseTex },
            { "anti_aliased_fill", &ImGuiStyle::AntiAliasedFill }
        };
        return s_Styles;
    }

    const TStyleColorNames& GetStyleColorNames()
    {
        static const TStyleColorNames s_Styles = {
            { "button_gradient_top", &ImGuiStyle::ButtonGradientTop },
            { "button_gradient_bottom", &ImGuiStyle::ButtonGradientBottom },
            { "button_hovered_gradient_top", &ImGuiStyle::ButtonHoveredGradientTop },
            { "button_hovered_gradient_bottom", &ImGuiStyle::ButtonHoveredGradientBottom },
            { "button_active_gradient_top", &ImGuiStyle::ButtonActiveGradientTop },
            { "button_active_gradient_bottom", &ImGuiStyle::ButtonActiveGradientBottom },
            { "combo_gradient_top", &ImGuiStyle::ComboGradientTop },
            { "combo_gradient_bottom", &ImGuiStyle::ComboGradientBottom },
            { "combo_hovered_gradient_top", &ImGuiStyle::ComboHoveredGradientTop },
            { "combo_hovered_gradient_bottom", &ImGuiStyle::ComboHoveredGradientBottom },
            { "combo_active_gradient_top", &ImGuiStyle::ComboActiveGradientTop },
            { "combo_active_gradient_bottom", &ImGuiStyle::ComboActiveGradientBottom },
            { "docking_tab_gradient_top", &ImGuiStyle::DockingTabGradientTop },
            { "docking_tab_gradient_bottom", &ImGuiStyle::DockingTabGradientBottom },
            { "docking_tab_selected_gradient_top", &ImGuiStyle::DockingTabSelectedGradientTop },
            { "docking_tab_selected_gradient_bottom", &ImGuiStyle::DockingTabSelectedGradientBottom },
            { "docking_tab_hovered_gradient_top", &ImGuiStyle::DockingTabHoveredGradientTop },
            { "docking_tab_hovered_gradient_bottom", &ImGuiStyle::DockingTabHoveredGradientBottom },
            { "hub_card", &ImGuiStyle::HubCard },
            { "hub_card_selected", &ImGuiStyle::HubCardSelected },
            { "hub_card_hovered", &ImGuiStyle::HubCardHovered },
            { "hub_card_border", &ImGuiStyle::HubCardBorder },
            { "hub_card_selected_border", &ImGuiStyle::HubCardSelectedBorder }
        };
        return s_Styles;
    }

    bool ParseColor(const Json& value, ImVec4& color)
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

        unsigned int aComponents[4] = { 0, 0, 0, 255 };
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
                parsed = parsed * 16u + (c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10));
            }
            aComponents[component] = parsed;
        }
        color = ImVec4(aComponents[0] / 255.0f, aComponents[1] / 255.0f,
                       aComponents[2] / 255.0f, aComponents[3] / 255.0f);
        return true;
    }

    using TSemanticColors = std::unordered_map<std::string, ImVec4>;

    bool ParseThemeColor(const Json& value, const TSemanticColors& semanticColors,
                         ImVec4& color)
    {
        if (!value.is_string())
        {
            return false;
        }

        const std::string valueText = value.get<std::string>();
        if (!valueText.empty() && valueText[0] == '@')
        {
            const auto semantic = semanticColors.find(valueText.substr(1));
            if (semantic == semanticColors.end())
            {
                return false;
            }
            color = semantic->second;
            return true;
        }

        return ParseColor(value, color);
    }

    ImVec4 ScaleColor(const ImVec4& color, float scale)
    {
        return ImVec4(
            std::min(color.x * scale, 1.0f),
            std::min(color.y * scale, 1.0f),
            std::min(color.z * scale, 1.0f),
            color.w);
    }

    void AddSemanticColorOverrides(const TSemanticColors& semanticColors,
                                   SThemeOverrides& overrides)
    {
        const auto addColors = [&semanticColors, &overrides](
                                   const std::string& role, std::initializer_list<ImGuiCol> aTargets)
        {
            const auto semantic = semanticColors.find(role);
            if (semantic == semanticColors.end())
            {
                return;
            }
            for (const ImGuiCol target : aTargets)
            {
                overrides.m_vColors.emplace_back(target, semantic->second);
            }
        };
        const auto addStyleColors = [&semanticColors, &overrides](
                                        const std::string& role, std::initializer_list<const char*> aTargets)
        {
            const auto semantic = semanticColors.find(role);
            if (semantic == semanticColors.end())
            {
                return;
            }
            for (const char* pTarget : aTargets)
            {
                const auto styleColor = GetStyleColorNames().find(pTarget);
                if (styleColor != GetStyleColorNames().end())
                {
                    overrides.m_vStyleColors.emplace_back(styleColor->second, semantic->second);
                }
            }
        };

        addColors("accent", { ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive, ImGuiCol_HeaderActive, ImGuiCol_TextLink, ImGuiCol_NavCursor, ImGuiCol_TabSelectedOverline, ImGuiCol_ResizeGripActive });
        addColors("accent_hovered", { ImGuiCol_SeparatorHovered, ImGuiCol_ResizeGripHovered });
        addStyleColors("accent", { "hub_card_selected_border" });
        addColors("surface", { ImGuiCol_WindowBg, ImGuiCol_ChildBg, ImGuiCol_DockingEmptyBg });
        addColors("surface_raised", { ImGuiCol_PopupBg, ImGuiCol_FrameBg, ImGuiCol_TitleBg });
        addColors("surface_overlay", { ImGuiCol_NavWindowingDimBg, ImGuiCol_ModalWindowDimBg });
        addColors("text", { ImGuiCol_Text });
        addColors("text_muted", { ImGuiCol_TextDisabled });
        addColors("border", { ImGuiCol_Border, ImGuiCol_Separator, ImGuiCol_TableBorderStrong, ImGuiCol_TableBorderLight });
    }

    bool GetComponentColorSlots(const std::string& component,
                                std::array<ImGuiCol, 3>& aColorSlots)
    {
        if (component == "button")
        {
            aColorSlots = { ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive };
        }
        else if (component == "header")
        {
            aColorSlots = { ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive };
        }
        else if (component == "input")
        {
            aColorSlots = { ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive };
        }
        else if (component == "tab")
        {
            aColorSlots = { ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabActive };
        }
        else
        {
            return false;
        }
        return true;
    }

    bool ParseComponentState(const Json& value, const std::string& component,
                             const std::string& stateName, const TSemanticColors& semanticColors,
                             SComponentStateOverride& state, std::string& error)
    {
        if (stateName == "disabled")
        {
            if (!value.is_object())
            {
                error = "components." + component +
                    ".disabled must be an object with an alpha value between 0 and 1";
                return false;
            }
            for (const auto& [property, propertyValue] : value.items())
            {
                (void)propertyValue;
                if (property != "alpha")
                {
                    error = "unknown theme parameter: components." + component +
                        ".disabled." + property;
                    return false;
                }
            }
            if (!value.contains("alpha") || !value["alpha"].is_number())
            {
                error = "components." + component +
                    ".disabled.alpha must be a number between 0 and 1";
                return false;
            }
            state.m_Alpha = value["alpha"].get<float>();
            if (!std::isfinite(state.m_Alpha) || state.m_Alpha < 0.0f || state.m_Alpha > 1.0f)
            {
                error = "components." + component +
                    ".disabled.alpha must be between 0 and 1";
                return false;
            }
            state.m_HasAlpha = true;
            return true;
        }

        if (value.is_string())
        {
            if (!ParseThemeColor(value, semanticColors, state.m_Color))
            {
                error = "invalid color in components." + component + "." +
                    stateName + ": " + value.get<std::string>();
                return false;
            }
            state.m_HasColor = true;
            return true;
        }

        if (!value.is_object())
        {
            error = "components." + component + "." + stateName +
                " must be a color or an object";
            return false;
        }

        for (const auto& [property, propertyValue] : value.items())
        {
            if (property == "color")
            {
                if (!ParseThemeColor(propertyValue, semanticColors, state.m_Color))
                {
                    error = "invalid color in components." + component + "." +
                        stateName + ".color";
                    return false;
                }
                state.m_HasColor = true;
            }
            else if (property == "gradient")
            {
                if (component != "button")
                {
                    error = "gradient is not supported for component: " + component;
                    return false;
                }
                if (!propertyValue.is_object())
                {
                    error = "components." + component + "." + stateName +
                        ".gradient must be an object";
                    return false;
                }
                for (const auto& [gradientName, gradientValue] : propertyValue.items())
                {
                    ImVec4 parsedColor;
                    if (gradientName != "top" && gradientName != "bottom")
                    {
                        error = "unknown theme parameter: components." + component +
                            "." + stateName + ".gradient." + gradientName;
                        return false;
                    }
                    if (!ParseThemeColor(gradientValue, semanticColors, parsedColor))
                    {
                        error = "invalid color in components." + component + "." +
                            stateName + ".gradient." + gradientName;
                        return false;
                    }
                    if (gradientName == "top")
                    {
                        state.m_GradientTop = parsedColor;
                    }
                    else
                    {
                        state.m_GradientBottom = parsedColor;
                    }
                }
                if (!propertyValue.contains("top") || !propertyValue.contains("bottom"))
                {
                    error = "components." + component + "." + stateName +
                        ".gradient requires both top and bottom colors";
                    return false;
                }
                state.m_HasGradient = true;
            }
            else
            {
                error = "unknown theme parameter: components." + component + "." +
                    stateName + "." + property;
                return false;
            }
        }

        if (!state.m_HasColor && !state.m_HasGradient)
        {
            error = "components." + component + "." + stateName +
                " must define a color or gradient";
            return false;
        }
        return true;
    }

    bool ParseComponents(const Json& data, const TSemanticColors& semanticColors,
                         SThemeOverrides& overrides, std::string& error)
    {
        if (!data.is_object())
        {
            error = "'components' must be an object";
            return false;
        }

        for (const auto& [component, variants] : data.items())
        {
            std::array<ImGuiCol, 3> aColorSlots;
            if (!GetComponentColorSlots(component, aColorSlots))
            {
                error = "unknown theme component: components." + component;
                return false;
            }
            if (!variants.is_object())
            {
                error = "components." + component + " must be an object of variants";
                return false;
            }
            for (const auto& [variant, states] : variants.items())
            {
                if (variant.empty() || !std::all_of(variant.begin(), variant.end(), [](const unsigned char c)
                                                    {
                                                        return std::isalnum(c) || c == '_' || c == '-';
                                                    }))
                {
                    error = "invalid component variant name: components." + component +
                        "." + variant;
                    return false;
                }
                if (!states.is_object())
                {
                    error = "components." + component + "." + variant +
                        " must be an object of states";
                    return false;
                }
                if (states.empty())
                {
                    error = "components." + component + "." + variant +
                        " must define at least one state";
                    return false;
                }

                SComponentVariantOverride componentOverride;
                componentOverride.m_Component = component;
                componentOverride.m_Variant = variant;
                for (const auto& [stateName, stateValue] : states.items())
                {
                    size_t stateIndex = 0;
                    if (stateName == "normal")
                    {
                        stateIndex = 0;
                    }
                    else if (stateName == "hovered")
                    {
                        stateIndex = 1;
                    }
                    else if (stateName == "active")
                    {
                        stateIndex = 2;
                    }
                    else if (stateName == "disabled")
                    {
                        stateIndex = 3;
                    }
                    else
                    {
                        error = "unknown theme state: components." + component + "." +
                            variant + "." + stateName;
                        return false;
                    }
                    if (!ParseComponentState(stateValue, component, stateName,
                                             semanticColors, componentOverride.m_aStates[stateIndex], error))
                    {
                        error = "components." + component + "." + variant + ": " + error;
                        return false;
                    }
                }
                overrides.m_vComponents.push_back(std::move(componentOverride));
            }
        }
        return true;
    }

    bool ParseThemeOverrides(const Json& data, SThemeOverrides& overrides, std::string& error)
    {
        if (!data.is_object())
        {
            error = "theme root must be an object";
            return false;
        }

        for (const auto& [name, value] : data.items())
        {
            (void)value;
            if (name != "schema_version" && name != "name" && name != "base" &&
                name != "font" && name != "semantic" && name != "components" &&
                name != "style" && name != "colors")
            {
                error = "unknown theme parameter: " + name;
                return false;
            }
        }

        if (data.contains("schema_version"))
        {
            if (!data["schema_version"].is_number_integer() ||
                data["schema_version"].get<int>() != 1)
            {
                error = "'schema_version' must be the integer 1";
                return false;
            }
        }

        if (data.contains("name") &&
            (!data["name"].is_string() || data["name"].get<std::string>().empty()))
        {
            error = "'name' must be a non-empty string";
            return false;
        }

        TSemanticColors semanticColors;
        if (data.contains("semantic"))
        {
            if (!data["semantic"].is_object())
            {
                error = "'semantic' must be an object";
                return false;
            }
            static const std::array<const char*, 21> aSemanticRoles = {
                "accent", "accent_hovered", "accent_active", "surface", "surface_raised",
                "text", "text_muted", "border", "success", "warning", "danger", "info",
                "surface_overlay", "success_hovered", "success_active", "warning_hovered",
                "warning_active", "danger_hovered", "danger_active", "info_hovered",
                "info_active"
            };
            for (const auto& [role, value] : data["semantic"].items())
            {
                const bool knownRole = std::any_of(aSemanticRoles.begin(), aSemanticRoles.end(),
                                                   [&role](const char* pKnownRole)
                                                   {
                                                       return role == pKnownRole;
                                                   });
                ImVec4 parsedColor;
                if (!knownRole)
                {
                    error = "unknown semantic color role: semantic." + role;
                    return false;
                }
                if (!ParseColor(value, parsedColor))
                {
                    error = "semantic." + role + " must be a HEX color";
                    return false;
                }
                semanticColors.emplace(role, parsedColor);
            }
        }
        static const std::array<const char*, 5> aDerivableRoles = {
            "accent", "success", "warning", "danger", "info"
        };
        for (const char* pRole : aDerivableRoles)
        {
            const auto role = semanticColors.find(pRole);
            if (role == semanticColors.end())
            {
                continue;
            }

            const ImVec4 baseColor = role->second;
            const std::string hoveredRole = std::string(pRole) + "_hovered";
            const std::string activeRole = std::string(pRole) + "_active";
            if (semanticColors.find(hoveredRole) == semanticColors.end())
            {
                semanticColors.emplace(hoveredRole, ScaleColor(baseColor, 1.12f));
            }
            if (semanticColors.find(activeRole) == semanticColors.end())
            {
                semanticColors.emplace(activeRole, ScaleColor(baseColor, 0.86f));
            }
        }
        AddSemanticColorOverrides(semanticColors, overrides);

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
            overrides.m_LightBase = base == "light";
        }

        if (data.contains("font"))
        {
            const Json& font = data["font"];
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
                    overrides.m_FontPath = value.get<std::string>();
                }
                else if (name == "size")
                {
                    if (!value.is_number())
                    {
                        error = "font size must be a number";
                        return false;
                    }
                    overrides.m_FontSize = value.get<float>();
                    if (!std::isfinite(overrides.m_FontSize) ||
                        overrides.m_FontSize < 6.0f || overrides.m_FontSize > 64.0f)
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
                    overrides.m_FontScale = value.get<float>();
                    if (!std::isfinite(overrides.m_FontScale) ||
                        overrides.m_FontScale < 0.5f || overrides.m_FontScale > 3.0f)
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
                if (color == GetColorNames().end())
                {
                    error = "unknown theme color: colors." + name;
                    return false;
                }
                if (!ParseThemeColor(value, semanticColors, parsed))
                {
                    error = "invalid color or unknown semantic reference: colors." + name;
                    return false;
                }
                overrides.m_vColors.emplace_back(color->second, parsed);
            }
        }

        if (data.contains("components") &&
            !ParseComponents(data["components"], semanticColors, overrides, error))
        {
            return false;
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
                    overrides.m_vFloatStyle.emplace_back(scalar->second, parsed);
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
                    overrides.m_vVectorStyle.emplace_back(vector->second, ImVec2(x, y));
                }
                else if (boolean != GetBoolStyleNames().end())
                {
                    if (!value.is_boolean())
                    {
                        error = "style value must be a boolean: " + name;
                        return false;
                    }
                    overrides.m_vBoolStyle.emplace_back(boolean->second, value.get<bool>());
                }
                else if (styleColor != GetStyleColorNames().end())
                {
                    ImVec4 parsed;
                    if (!ParseThemeColor(value, semanticColors, parsed))
                    {
                        error = "style color must be a HEX value or known semantic reference: " +
                            name;
                        return false;
                    }
                    overrides.m_vStyleColors.emplace_back(styleColor->second, parsed);
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
        const std::string fontPath = overrides.m_FontPath.empty() ? languageFontPath : overrides.m_FontPath;
        std::error_code fileError;
        if (!fs::is_regular_file(fontPath, fileError))
        {
            error = "font file not found: " + fontPath;
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFont* pDefaultFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), overrides.m_FontSize);
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
            if (path.empty() || path == overrides.m_FontPath)
            {
                return;
            }
            ImFontConfig config = {};
            config.MergeMode = true;
            config.PixelSnapH = true;
            io.Fonts->AddFontFromFileTTF(path.c_str(), overrides.m_FontSize, &config, pGlyphRanges);
        };

        const std::string mergeFontPath = CLanguageManager::Get().EditorFontMergePath();
        if (!mergeFontPath.empty())
        {
            mergeFont(mergeFontPath, nullptr);
        }
        if (LanguageUsesMsPgothic(languageCode) &&
            fontPath != "assets/font/MS-Pgothic-Regular.ttf")
        {
            mergeFont("assets/font/MS-Pgothic-Regular.ttf",
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
        const std::string fontPath = overrides.m_FontPath.empty()
            ? CLanguageManager::Get().EditorFontPath()
            : overrides.m_FontPath;
        std::error_code fileError;
        if (!fs::is_regular_file(fontPath, fileError))
        {
            error = "font file not found: " + fontPath;
            return false;
        }

        s_PendingFontOverrides = overrides;
        s_PendingFontThemeId = themeId;
        s_HasPendingFonts = true;
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

    bool ReadThemeJson(std::istream& input, Json& data, std::string& error)
    {
        try
        {
            data = Json::parse(input, nullptr, true, true);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        return true;
    }

} // anonymous

struct SThemeRuntimeState
{
    std::vector<SComponentVariantOverride> m_vActiveComponents;
    std::vector<SThemeVariantRestore> m_vVariantRestoreStack;
};

SThemeRuntimeState& CThemeManager::GetRuntimeState()
{
    static SThemeRuntimeState s_State;
    return s_State;
}

void CThemeManager::Apply(bool lightTheme)
{
    GetRuntimeState().m_vActiveComponents.clear();

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.QuarkLegacyWidgetStyle = true;
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
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;

    style.FramePadding = ImVec2(6, 3);
    style.ItemSpacing = ImVec2(6, 4);

    ImVec4* pColors = style.Colors;

    // ====== GLOBAL ======
    pColors[ImGuiCol_Text] = ImVec4(0.80f, 0.82f, 0.85f, 1.00f); // #c9cdd1
    pColors[ImGuiCol_TextDisabled] = ImVec4(0.54f, 0.58f, 0.63f, 1.00f);
    pColors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #2a2c2f
    pColors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
    pColors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #32353a
    pColors[ImGuiCol_DockingEmptyBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);

    // ====== BORDERS ======
    pColors[ImGuiCol_Border] = ImVec4(0.27f, 0.28f, 0.30f, 1.00f); // #44484d
    pColors[ImGuiCol_Separator] = ImVec4(0.24f, 0.25f, 0.27f, 1.00f);

    // ====== FRAMES (inputs, edits) ======
    pColors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f); // #24272a
    pColors[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045 hover
    pColors[ImGuiCol_FrameBgActive] = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff
    pColors[ImGuiCol_InputTextCursor] = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);

    // ====== TITLE / MENUBAR ======
    pColors[ImGuiCol_TitleBg] = ImVec4(0.19f, 0.20f, 0.22f, 1.00f); // #31343a
    pColors[ImGuiCol_TitleBgActive] = ImVec4(0.24f, 0.26f, 0.29f, 1.00f);
    pColors[ImGuiCol_MenuBarBg] = ImVec4(0.19f, 0.20f, 0.22f, 1.00f);

    // ====== BUTTONS ======
    pColors[ImGuiCol_Button] = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #51565c
    pColors[ImGuiCol_ButtonHovered] = ImVec4(0.36f, 0.38f, 0.41f, 1.00f); // #5C6169 hover
    pColors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // #3D4247 pressed

    // ====== HEADERS (Tree, Selectable) ======
    pColors[ImGuiCol_Header] = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #292B2E
    pColors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045
    pColors[ImGuiCol_HeaderActive] = ImVec4(0.18f, 0.53f, 0.78f, 1.00f); // #2e87c7ff selection

    // ====== SELECTION ======
    pColors[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.47f, 0.78f, 0.35f); // #2e78c759

    // ====== SCROLLBAR ======
    pColors[ImGuiCol_ScrollbarBg] = ImVec4(0.18f, 0.20f, 0.22f, 1.00f); // #2f3337
    pColors[ImGuiCol_ScrollbarGrab] = ImVec4(0.33f, 0.36f, 0.38f, 1.00f); // #555b62
    pColors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    pColors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.53f, 0.56f, 1.00f); // #80878FFF

    // ====== TABS ======
    pColors[ImGuiCol_Tab] = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #333538FF
    pColors[ImGuiCol_TabHovered] = ImVec4(0.36f, 0.39f, 0.43f, 1.00f);
    pColors[ImGuiCol_TabActive] = ImVec4(0.27f, 0.30f, 0.34f, 1.00f);
    pColors[ImGuiCol_TabUnfocused] = ImVec4(0.17f, 0.18f, 0.20f, 1.00f);
    pColors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);

    // ====== CHECKBOX ======
    pColors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== RESIZE GRIP ======
    pColors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #4D5259FF
    pColors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    pColors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== DOCKING ======
    pColors[ImGuiCol_DockingPreview] = ImVec4(0.78f, 0.52f, 0.17f, 0.4f); // #c9802b66
    pColors[ImGuiCol_NavCursor] = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
    pColors[ImGuiCol_TextLink] = ImVec4(0.40f, 0.72f, 0.98f, 1.00f);
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
        s_AppliedFontScale = 1.0f;
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

    Json data;
    std::string error;
    if (!ReadThemeJson(input, data, error))
    {
        ReportThemeError(themeId, error);
        return false;
    }

    SThemeOverrides overrides;
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

    Apply(overrides.m_LightBase);
    ImGuiStyle& style = ImGui::GetStyle();
    style.QuarkLegacyWidgetStyle = false;
    for (const auto& [color, value] : overrides.m_vColors)
    {
        style.Colors[color] = value;
    }
    for (const auto& [pStyleField, value] : overrides.m_vFloatStyle)
    {
        style.*pStyleField = value;
    }
    for (const auto& [pStyleField, value] : overrides.m_vVectorStyle)
    {
        style.*pStyleField = value;
    }
    for (const auto& [pStyleField, value] : overrides.m_vBoolStyle)
    {
        style.*pStyleField = value;
    }
    for (const auto& [pStyleField, value] : overrides.m_vStyleColors)
    {
        style.*pStyleField = value;
    }
    GetRuntimeState().m_vActiveComponents = overrides.m_vComponents;
    s_AppliedFontScale = overrides.m_FontScale;
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

        Json data;
        if (!ReadThemeJson(input, data, error))
        {
            ReportThemeError(themeId, error);
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
    return s_AppliedFontScale;
}

bool CThemeManager::PushVariant(const std::string& component, const std::string& variant,
                                bool disabled)
{
    SThemeRuntimeState& runtime = GetRuntimeState();
    const auto found = std::find_if(runtime.m_vActiveComponents.begin(),
                                    runtime.m_vActiveComponents.end(),
                                    [&component, &variant](const SComponentVariantOverride& value)
                                    {
                                        return value.m_Component == component && value.m_Variant == variant;
                                    });
    if (found == runtime.m_vActiveComponents.end())
    {
        return false;
    }

    std::array<ImGuiCol, 3> aColorSlots;
    if (!GetComponentColorSlots(component, aColorSlots))
    {
        return false;
    }

    SThemeVariantRestore restore;
    ImGuiStyle& style = ImGui::GetStyle();
    restore.m_LegacyWidgetStyle = style.QuarkLegacyWidgetStyle;
    style.QuarkLegacyWidgetStyle = false;
    std::array<ImVec4 ImGuiStyle::*, 3> aGradientTop = {
        &ImGuiStyle::ButtonGradientTop,
        &ImGuiStyle::ButtonHoveredGradientTop,
        &ImGuiStyle::ButtonActiveGradientTop
    };
    std::array<ImVec4 ImGuiStyle::*, 3> aGradientBottom = {
        &ImGuiStyle::ButtonGradientBottom,
        &ImGuiStyle::ButtonHoveredGradientBottom,
        &ImGuiStyle::ButtonActiveGradientBottom
    };

    for (size_t stateIndex = 0; stateIndex < 3; ++stateIndex)
    {
        const SComponentStateOverride& state = found->m_aStates[stateIndex];
        if (state.m_HasColor)
        {
            ImGui::PushStyleColor(aColorSlots[stateIndex], state.m_Color);
            ++restore.m_ColorPushCount;
        }
        if (state.m_HasGradient && component == "button")
        {
            if (!restore.m_HasButtonGradient)
            {
                restore.m_HasButtonGradient = true;
                restore.m_ButtonGradient = style.ButtonGradient;
                for (size_t gradientIndex = 0; gradientIndex < 3; ++gradientIndex)
                {
                    restore.m_aButtonGradientTop[gradientIndex] =
                        style.*aGradientTop[gradientIndex];
                    restore.m_aButtonGradientBottom[gradientIndex] =
                        style.*aGradientBottom[gradientIndex];
                }
            }
            style.*aGradientTop[stateIndex] = state.m_GradientTop;
            style.*aGradientBottom[stateIndex] = state.m_GradientBottom;
            style.ButtonGradient = true;
        }
    }

    const SComponentStateOverride& disabledState = found->m_aStates[3];
    if (disabled && disabledState.m_HasAlpha)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * disabledState.m_Alpha);
        restore.m_HasAlpha = true;
    }

    runtime.m_vVariantRestoreStack.push_back(restore);
    return true;
}

void CThemeManager::PopVariant()
{
    SThemeRuntimeState& runtime = GetRuntimeState();
    if (runtime.m_vVariantRestoreStack.empty())
    {
        ReportThemeError("variant", "PopVariant called without a matching PushVariant");
        return;
    }

    const SThemeVariantRestore restore = runtime.m_vVariantRestoreStack.back();
    runtime.m_vVariantRestoreStack.pop_back();
    ImGui::GetStyle().QuarkLegacyWidgetStyle = restore.m_LegacyWidgetStyle;
    if (restore.m_HasAlpha)
    {
        ImGui::PopStyleVar();
    }
    if (restore.m_ColorPushCount > 0)
    {
        ImGui::PopStyleColor(restore.m_ColorPushCount);
    }

    if (restore.m_HasButtonGradient)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ButtonGradient = restore.m_ButtonGradient;
        style.ButtonGradientTop = restore.m_aButtonGradientTop[0];
        style.ButtonHoveredGradientTop = restore.m_aButtonGradientTop[1];
        style.ButtonActiveGradientTop = restore.m_aButtonGradientTop[2];
        style.ButtonGradientBottom = restore.m_aButtonGradientBottom[0];
        style.ButtonHoveredGradientBottom = restore.m_aButtonGradientBottom[1];
        style.ButtonActiveGradientBottom = restore.m_aButtonGradientBottom[2];
    }
}

void CThemeManager::ProcessPendingFonts()
{
    if (!s_HasPendingFonts)
    {
        return;
    }

    std::string error;
    if (!ReloadEditorFonts(s_PendingFontOverrides, error))
    {
        ReportThemeError(s_PendingFontThemeId, error);
    }
    s_HasPendingFonts = false;
    s_PendingFontThemeId.clear();
}

std::vector<SEditorTheme> CThemeManager::GetAvailableThemes()
{
    std::vector<SEditorTheme> vThemes = {
        { "quark-dark", "Quark Dark" },
        { "quark-light", "Quark Light" }
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
        if (!IsThemeId(id) || std::any_of(vThemes.begin(), vThemes.end(), [&id](const SEditorTheme& theme)
                                          {
                                              return theme.m_Id == id;
                                          }))
        {
            continue;
        }

        std::string name = FriendlyThemeName(id);
        std::ifstream input(iterator->path());
        if (input.is_open())
        {
            try
            {
                Json data;
                std::string parseError;
                if (!ReadThemeJson(input, data, parseError))
                {
                    ReportThemeError(id, parseError);
                    continue;
                }
                SThemeOverrides overrides;
                if (!ParseThemeOverrides(data, overrides, parseError))
                {
                    ReportThemeError(id, parseError);
                    continue;
                }
                if (data.contains("name") && data["name"].is_string())
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
                continue;
            }
        }
        else
        {
            ReportThemeError(id, "could not open theme file");
            continue;
        }
        vThemes.push_back({ id, name });
    }

    std::sort(vThemes.begin() + 2, vThemes.end(),
              [](const SEditorTheme& a, const SEditorTheme& b)
              {
                  return a.m_Name < b.m_Name;
              });
    return vThemes;
}
