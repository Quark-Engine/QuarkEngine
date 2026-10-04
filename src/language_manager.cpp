#include "language_manager.h"
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

namespace
{
std::string NormalizeLanguageCode(const std::string& code)
{
    if (code == "ru_ru") return "russian";
    return code;
}

std::string ResolveFontPath(const std::string& fontValue)
{
    if (fontValue.empty()) return "assets/Rubik-Regular.ttf";
    if (fontValue.find('/') != std::string::npos || fontValue.find('\\') != std::string::npos)
        return fontValue;
    return "assets/" + fontValue;
}
} // anonymous

bool CLanguageManager::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;
    m_Cache.clear();
    file >> m_Data;
    return true;
}

void CLanguageManager::SetLang(const std::string& code)
{
    m_Current = NormalizeLanguageCode(code);
    Load("assets/lang/" + m_Current + ".json");

    const std::string path = "config.json";
    json j;
    if (std::filesystem::exists(path))
    {
        std::ifstream input(path);
        try
        {
            input >> j;
        }
        catch (...)
        {
        }
    }
    j["language"] = m_Current;
    std::ofstream output(path);
    output << j.dump(4);
}

const char* CLanguageManager::Word(const std::string& key) const
{
    auto it = m_Cache.find(key);
    if (it != m_Cache.end()) return it->second.c_str();

    const nlohmann::json* pNode = &m_Data;
    size_t start = 0;

    while (true)
    {
        size_t dot = key.find('.', start);
        std::string part = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

        if (!pNode->contains(part))
        {
            m_Cache[key] = key;
            return m_Cache[key].c_str();
        }

        pNode = &(*pNode)[part];

        if (dot == std::string::npos)
        {
            m_Cache[key] = pNode->is_string() ? pNode->get<std::string>() : key;
            return m_Cache[key].c_str();
        }

        start = dot + 1;
    }
}

std::string CLanguageManager::EditorFontPath() const
{
    if (m_Data.contains("_meta") && m_Data["_meta"].is_object())
    {
        const auto& meta = m_Data["_meta"];
        if (meta.contains("editor_font") && meta["editor_font"].is_string())
            return ResolveFontPath(meta["editor_font"].get<std::string>());
    }

    return "assets/Rubik-Regular.ttf";
}

std::string CLanguageManager::EditorFontMergePath() const
{
    if (m_Data.contains("_meta") && m_Data["_meta"].is_object())
    {
        const auto& meta = m_Data["_meta"];
        if (meta.contains("editor_font_merge") && meta["editor_font_merge"].is_string())
            return ResolveFontPath(meta["editor_font_merge"].get<std::string>());
    }

    return "";
}

std::string LoadOrCreateConfig()
{
    const std::string path = "config.json";

    if (!std::filesystem::exists(path))
    {
        json def;
        def["language"] = "english";
        def["projects"] = json::array();

        std::ofstream output(path);
        output << def.dump(4);

        return "english";
    }

    std::ifstream input(path);
    if (!input.is_open())
        return "english";

    json j;
    try
    {
        input >> j;
    }
    catch (...)
    {
        return "english";
    }

    if (j.contains("language"))
    {
        return NormalizeLanguageCode(j["language"].get<std::string>());
    }

    return "english";
}
