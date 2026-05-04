#pragma once

#include "nlohmann/json.hpp"
#include <string>


class LanguageManager {
public:
    static LanguageManager& get() {
        static LanguageManager instance;
        return instance;
    }

    bool load(const std::string& path);
    void set_lang(const std::string& lang);
    const char* word(const std::string& key) const;

private:
    std::string current;
    nlohmann::json data;
    mutable std::unordered_map<std::string, std::string> cache;
};