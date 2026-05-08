#pragma once

#include "nlohmann/json.hpp"
#include <string>


class LanguageManager {
public:
    static LanguageManager& get() {
        static LanguageManager instance;
        return instance;
    }

    std::string current;

    bool load(const std::string& path);
    void set_lang(const std::string& lang);
    const char* word(const std::string& key) const;
    std::string editor_font_path() const;
    std::string editor_font_merge_path() const;

private:
    nlohmann::json data;
    mutable std::unordered_map<std::string, std::string> cache;
};

std::string load_or_create_config();
