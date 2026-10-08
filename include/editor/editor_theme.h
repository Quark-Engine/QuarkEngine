#ifndef __EDITOR_EDITOR_THEME_H__
#define __EDITOR_EDITOR_THEME_H__

#include <string>
#include <vector>

struct SEditorTheme
{
    std::string id;
    std::string name;
};

class CThemeManager
{
public:
    static void Apply(bool lightTheme);
    static bool Apply(const std::string& themeId);
    static std::vector<SEditorTheme> GetAvailableThemes();
};

#endif // __EDITOR_EDITOR_THEME_H__