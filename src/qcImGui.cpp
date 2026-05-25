#include "qcImGui.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

namespace qc {

namespace {

bool g_qc_imgui_initialized = false;

void qcImGuiEventBridge(const SDL_Event* event) {
    qcImGuiProcessEvent(event);
}

} // namespace

bool qcImGuiSetup(bool darkTheme) {
    if (g_qc_imgui_initialized) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (darkTheme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsClassic();
    }

    SDL_Window* window = GetNativeWindow();
    SDL_GLContext context = GetNativeContext();

    if (window == nullptr || context == nullptr) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplSDL3_InitForOpenGL(window, context)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    SetNativeEventCallback(qcImGuiEventBridge);
    g_qc_imgui_initialized = true;
    return true;
}

void qcImGuiShutdown() {
    if (!g_qc_imgui_initialized) {
        return;
    }

    SetNativeEventCallback(nullptr);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    g_qc_imgui_initialized = false;
}

void qcImGuiBegin() {
    if (!g_qc_imgui_initialized) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void qcImGuiEnd() {
    if (!g_qc_imgui_initialized) {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void qcImGuiProcessEvent(const SDL_Event* event) {
    if (!g_qc_imgui_initialized || event == nullptr) {
        return;
    }

    ImGui_ImplSDL3_ProcessEvent(event);
}

void qcImGuiImageRect(const Texture2D* texture, int width, int height, Rectangle sourceRect) {
    if (texture == nullptr || texture->id == 0) {
        return;
    }

    const float tex_width = static_cast<float>(texture->width);
    const float tex_height = static_cast<float>(texture->height);

    const float src_w = fabsf(sourceRect.width);
    const float src_h = fabsf(sourceRect.height);

    float u0 = sourceRect.x / tex_width;
    float v0 = sourceRect.y / tex_height;
    float u1 = (sourceRect.x + src_w) / tex_width;
    float v1 = (sourceRect.y + src_h) / tex_height;

    if (sourceRect.width < 0.0f) {
        std::swap(u0, u1);
    }
    if (sourceRect.height < 0.0f) {
        std::swap(v0, v1);
    }

    ImGui::Image(
        (ImTextureID)(intptr_t)texture->id,
        ImVec2(static_cast<float>(width), static_cast<float>(height)),
        ImVec2(u0, v0),
        ImVec2(u1, v1)
    );
}

} // namespace qc
