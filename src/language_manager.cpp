#include "headers/language_manager.h"
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

static std::unordered_map<std::string, std::string> word_cache;

static std::string normalize_language_code(const std::string& code) {
    if (code == "ru_ru") return "russian";
    return code;
}

static std::string resolve_font_path(const std::string& font_value) {
    if (font_value.empty()) return "assets/Rubik-Regular.ttf";
    if (font_value.find('/') != std::string::npos || font_value.find('\\') != std::string::npos)
        return font_value;
    return "assets/" + font_value;
}

bool LanguageManager::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    cache.clear();
    f >> data;
    return true;
}

void LanguageManager::set_lang(const std::string& code) {
    current = normalize_language_code(code);
    load("assets/lang/" + current + ".json");

    const std::string path = "config.json";
    json j;
    if (std::filesystem::exists(path)) {
        std::ifstream in(path);
        try { in >> j; } catch (...) {}
    }
    j["language"] = current;
    std::ofstream out(path);
    out << j.dump(4);
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

std::string LanguageManager::editor_font_path() const {
    if (data.contains("_meta") && data["_meta"].is_object()) {
        const auto& meta = data["_meta"];
        if (meta.contains("editor_font") && meta["editor_font"].is_string())
            return resolve_font_path(meta["editor_font"].get<std::string>());
    }

    return "assets/Rubik-Regular.ttf";
}

std::string LanguageManager::editor_font_merge_path() const {
    if (data.contains("_meta") && data["_meta"].is_object()) {
        const auto& meta = data["_meta"];
        if (meta.contains("editor_font_merge") && meta["editor_font_merge"].is_string())
            return resolve_font_path(meta["editor_font_merge"].get<std::string>());
    }

    return "";
}

std::string load_or_create_config() {
    const std::string path = "config.json";

    if (!std::filesystem::exists(path)) {
        json def;
        def["language"] = "en_us";
        def["projects"] = json::array();

        std::ofstream out(path);
        out << def.dump(4);

        return "en_us";
    }

    std::ifstream in(path);
    if (!in.is_open())
        return "en_us";

    json j;
    try {
        in >> j;
    } catch (...) {
        return "en_us";
    }

    if (j.contains("language")) {
        return normalize_language_code(j["language"].get<std::string>());
    }

    return "en_us";
}
