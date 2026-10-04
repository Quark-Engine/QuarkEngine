#ifndef __EDITOR_EDITOR_DESKTOP_H__
#define __EDITOR_EDITOR_DESKTOP_H__

#include <string>

class CDesktopIntegration
{
public:
    static void OpenUrl(const char* pUrl);

    static std::string SaveFileDialog(const char* pFilterLabel, const char* pFilter,
        const char* pDefExt, const std::string& defaultName);
};

#endif // __EDITOR_EDITOR_DESKTOP_H__