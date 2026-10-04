#include "editor/editor_theme.h"

#include "imgui.h"

void CThemeManager::Apply(bool lightTheme)
{
    ImGuiStyle& style = ImGui::GetStyle();

    if (lightTheme)
    {
        ImGui::StyleColorsLight();
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