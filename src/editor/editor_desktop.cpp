#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#define Rectangle WinRectangle
#include <windows.h>
#include <commdlg.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef near
#undef far
#endif

#include "editor/editor_desktop.h"

#include "QuarkCore/QuarkCore.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

void CDesktopIntegration::OpenUrl(const char* pUrl)
{
#ifdef _WIN32
    system((std::string("start ") + pUrl).c_str());
#elif defined(__APPLE__)
    system((std::string("open ") + pUrl).c_str());
#elif defined(__linux__)
    system((std::string("xdg-open ") + pUrl).c_str());
#else
    TraceLog(LogLevel::Error, "EDITOR", TextFormat("Cannot open URL: %s", pUrl));
#endif
}

std::string CDesktopIntegration::SaveFileDialog(const char* pFilterLabel, const char* pFilter,
    const char* pDefExt, const std::string& defaultName)
{
#ifdef _WIN32
    char aPath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = pFilter;
    ofn.lpstrFile = aPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = pDefExt;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    if (!defaultName.empty() && defaultName.size() < MAX_PATH)
    {
        memcpy(aPath, defaultName.c_str(), defaultName.size() + 1);
    }
    if (GetSaveFileNameA(&ofn))
    {
        return aPath;
    }
    return {};

#elif defined(__linux__)
    std::string cmd = "zenity --file-selection --save ";
    cmd += "--file-filter='";
    cmd += pFilterLabel;
    cmd += " (*.";
    cmd += pDefExt;
    cmd += ")' --file-filter='All Files (*)' --confirm-overwrite 2>/dev/null";
    if (!defaultName.empty())
    {
        cmd += " --filename='" + defaultName + "'";
    }
    FILE* pPipe = popen(cmd.c_str(), "r");
    if (!pPipe)
    {
        return {};
    }
    char aResult[1024] = {};
    if (fgets(aResult, sizeof(aResult), pPipe))
    {
        const size_t len = strlen(aResult);
        if (len > 0 && aResult[len - 1] == '\n')
        {
            aResult[len - 1] = '\0';
        }
    }
    pclose(pPipe);
    return aResult;

#elif defined(__APPLE__)
    std::string script = "POSIX path of (choose file name with prompt \"Save\"";
    if (!defaultName.empty())
    {
        std::string escaped = defaultName;
        std::string::size_type pos;
        while ((pos = escaped.find('\\')) != std::string::npos)
        {
            escaped.replace(pos, 1, "\\\\");
        }
        while ((pos = escaped.find('"')) != std::string::npos)
        {
            escaped.replace(pos, 1, "\\\"");
        }
        script += " default name \"" + escaped + "\"";
    }
    script += ")";
    std::string cmd = "osascript -e \"" + script + "\" 2>/dev/null";
    FILE* pPipe = popen(cmd.c_str(), "r");
    if (!pPipe)
    {
        return {};
    }
    char aResult[1024] = {};
    if (fgets(aResult, sizeof(aResult), pPipe))
    {
        const size_t len = strlen(aResult);
        if (len > 0 && aResult[len - 1] == '\n')
        {
            aResult[len - 1] = '\0';
        }
    }
    pclose(pPipe);
    return aResult;

#else
    return {};
#endif
}