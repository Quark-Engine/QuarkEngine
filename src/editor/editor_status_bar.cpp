#include "editor/editor_status_bar.h"

#include "editor.h"

#include "QuarkCore/QuarkCore.hpp"
#include "imgui.h"
#include "imgui_internal.h"

using namespace qc;

void CStatusBar::Draw(const CEditor& editor)
{
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    if (pViewport == nullptr)
    {
        return;
    }

    const float statusBarHeight = 28.0f;
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
    if (ImGui::BeginViewportSideBar("##main_status_bar", pViewport, ImGuiDir_Down, statusBarHeight, flags))
    {
        ImGui::TextDisabled("Quark Engine Editor v%s", "1.0.0");

        if (!editor.m_StatusMessage.empty())
        {
            ImGui::SameLine();
            ImGui::Text("%s", editor.m_StatusMessage.c_str());
        }

        const char* pFpsText = TextFormat("FPS: %d", GetFPS());
        const float fpsTextWidth = ImGui::CalcTextSize(pFpsText).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - fpsTextWidth - 10.0f);
        ImGui::Text("%s", pFpsText);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}