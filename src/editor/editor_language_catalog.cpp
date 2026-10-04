#include "editor/editor_language_catalog.h"

#include <cstring>

namespace
{

constexpr const char* gs_aLanguageLabels[] = {
    "Arabic",
    "Azerbaijani",
    "Belarusian",
    "Bosnian",
    "Brazilian Portuguese",
    "Bulgarian",
    "Catalan",
    "Chuvash",
    "Czech",
    "Danish",
    "Dutch",
    "English",
    "Esperanto",
    "Estonian",
    "Finnish",
    "French",
    "Galician",
    "German",
    "Greek",
    "Hungarian",
    "Index",
    "Italian",
    "Japanese",
    "Korean",
    "Kyrgyz",
    "License",
    "Norwegian",
    "Persian",
    "Polish",
    "Portuguese",
    "Romanian",
    "Russian",
    "Serbian",
    "Serbian (Cyrillic)",
    "Simplified Chinese",
    "Slovak",
    "Spanish",
    "Swedish",
    "Traditional Chinese",
    "Turkish",
    "Ukrainian"
};

constexpr const char* gs_aLanguageCodes[] = {
    "arabic",
    "azerbaijani",
    "belarusian",
    "bosnian",
    "brazilian_portuguese",
    "bulgarian",
    "catalan",
    "chuvash",
    "czech",
    "danish",
    "dutch",
    "english",
    "esperanto",
    "estonian",
    "finnish",
    "french",
    "galician",
    "german",
    "greek",
    "hungarian",
    "index",
    "italian",
    "japanese",
    "korean",
    "kyrgyz",
    "license",
    "norwegian",
    "persian",
    "polish",
    "portuguese",
    "romanian",
    "russian",
    "serbian",
    "serbian_cyrillic",
    "simplified_chinese",
    "slovak",
    "spanish",
    "swedish",
    "traditional_chinese",
    "turkish",
    "ukrainian"
};

static_assert(sizeof(gs_aLanguageLabels) / sizeof(gs_aLanguageLabels[0]) ==
    sizeof(gs_aLanguageCodes) / sizeof(gs_aLanguageCodes[0]),
    "language labels and codes must stay index-aligned");

constexpr int g_LanguageCount = static_cast<int>(sizeof(gs_aLanguageCodes) / sizeof(gs_aLanguageCodes[0]));

} // anonymous

int CLanguageCatalog::Count()
{
    return g_LanguageCount;
}

const char* CLanguageCatalog::Label(int index)
{
    if (index < 0 || index >= g_LanguageCount)
    {
        return gs_aLanguageLabels[0];
    }
    return gs_aLanguageLabels[index];
}

const char* CLanguageCatalog::Code(int index)
{
    if (index < 0 || index >= g_LanguageCount)
    {
        return gs_aLanguageCodes[0];
    }
    return gs_aLanguageCodes[index];
}

const char* CLanguageCatalog::GetLabel(void* pUserData, int index)
{
    return Label(index);
}

int CLanguageCatalog::IndexOf(const char* pCode)
{
    if (!pCode)
    {
        return 0;
    }
    for (int i = 0; i < g_LanguageCount; i++)
    {
        if (strcmp(gs_aLanguageCodes[i], pCode) == 0)
        {
            return i;
        }
    }
    return 0;
}