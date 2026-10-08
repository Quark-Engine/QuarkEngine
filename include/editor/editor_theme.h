#ifndef __EDITOR_EDITOR_THEME_H__
#define __EDITOR_EDITOR_THEME_H__

#include <string>
#include <vector>

struct SEditorTheme
{
    std::string id;
    std::string name;
};

struct SThemeRuntimeState;

class CThemeManager
{
public:
    static void Apply(bool lightTheme);
    static bool Apply(const std::string& themeId);
    static bool ReloadFonts(const std::string& themeId);
    static bool PushVariant(const std::string& component, const std::string& variant,
        bool disabled = false);
    static void PopVariant();
    static float GetAppliedFontScale();
    static void ProcessPendingFonts();
    static std::vector<SEditorTheme> GetAvailableThemes();

private:
    static SThemeRuntimeState& GetRuntimeState();
};

#endif // __EDITOR_EDITOR_THEME_H__