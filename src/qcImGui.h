#pragma once

#include "QuarkCore/QuarkCore.hpp"

namespace qc {

#if defined(_WIN32)
    #if defined(BUILD_LIBTYPE_SHARED)
        #define QCIMGUI_API __declspec(dllexport)
    #else
        #define QCIMGUI_API __declspec(dllimport)
    #endif
#else
    #define QCIMGUI_API
#endif

QCIMGUI_API bool qcImGuiSetup(bool darkTheme);
QCIMGUI_API void qcImGuiShutdown();
QCIMGUI_API void qcImGuiBegin();
QCIMGUI_API void qcImGuiEnd();
QCIMGUI_API void qcImGuiProcessEvent(const SDL_Event* event);
QCIMGUI_API void qcImGuiImageRect(const Texture2D* texture, int width, int height, Rectangle sourceRect);

} // namespace qc
