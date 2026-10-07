#ifndef __QC_IMGUI_H__
#define __QC_IMGUI_H__
#include "imgui.h"
#include "QuarkCore/QuarkCore.hpp"
#if defined(_WIN32)
    #if defined(QUARKENGINE_BUILD)
        #define QCIMGUI_API
    #elif defined(BUILD_LIBTYPE_SHARED)
        #define QCIMGUI_API __declspec(dllexport)
    #else
        #define QCIMGUI_API __declspec(dllimport)
    #endif
#else
    #define QCIMGUI_API
#endif

QCIMGUI_API bool QcImGuiSetup(bool darkTheme);
QCIMGUI_API void QcImGuiShutdown();
QCIMGUI_API void QcImGuiBegin();
QCIMGUI_API void QcImGuiEnd();
QCIMGUI_API void QcImGuiProcessEvent(const SDL_Event* pEvent);
QCIMGUI_API ImTextureID QcImGuiGetTextureId(const Texture2D* pTexture);
QCIMGUI_API void QcImGuiImage(const Texture2D* pTexture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f));
QCIMGUI_API void QcImGuiAddImage(ImDrawList* pDrawList, const Texture2D* pTexture, const ImVec2& min, const ImVec2& max, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f), ImU32 color = IM_COL32_WHITE);
QCIMGUI_API void QcImGuiImageRect(const Texture2D* pTexture, int width, int height, Rectangle sourceRect);
#endif // __QC_IMGUI_H__
