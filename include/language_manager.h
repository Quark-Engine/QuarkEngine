#ifndef __LANGUAGE_MANAGER_H__
#define __LANGUAGE_MANAGER_H__

#include "nlohmann/json.hpp"
#include <string>
#include <unordered_map>


class CLanguageManager
{
public:
    static CLanguageManager& Get()
    {
        static CLanguageManager s_Instance;
        return s_Instance;
    }

    std::string m_Current;

    bool Load(const std::string& path);
    void SetLang(const std::string& lang);
    const char* Word(const std::string& key) const;
    std::string EditorFontPath() const;
    std::string EditorFontMergePath() const;

private:
    nlohmann::json m_Data;
    nlohmann::json m_FallbackData;
    mutable std::unordered_map<std::string, std::string> m_Cache;
};

std::string LoadOrCreateConfig();

#endif // __LANGUAGE_MANAGER_H__
