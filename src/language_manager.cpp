#include "headers/language_manager.h"
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

static std::unordered_map<std::string, std::string> word_cache;
// language_manager.cpp
bool LanguageManager::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    cache.clear();
    f >> data;
    return true;
}

void LanguageManager::set_lang(const std::string& lang) {
    current = lang;
    load("assets/lang/" + lang + ".json");
}

const char* LanguageManager::word(const std::string& key) const {
    auto it = cache.find(key);
    if (it != cache.end()) return it->second.c_str();

    const nlohmann::json* node = &data;
    size_t start = 0;

    while (true) {
        size_t dot = key.find('.', start);
        std::string part = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

        if (!node->contains(part)) {
            cache[key] = key;
            return cache[key].c_str();
        }

        node = &(*node)[part];

        if (dot == std::string::npos) {
            cache[key] = node->is_string() ? node->get<std::string>() : key;
            return cache[key].c_str();
        }

        start = dot + 1;
    }
}