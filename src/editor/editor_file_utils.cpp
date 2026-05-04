#include "editor_file_utils.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool move_file(const std::string& source, const std::string& destination) {
    try {
        if (!fs::exists(source)) return false;
        if (fs::is_directory(source)) return false;
        
        fs::create_directories(fs::path(destination).parent_path());
        fs::rename(source, destination);
        return true;
    } catch (...) {
        return false;
    }
}

bool move_directory(const std::string& source, const std::string& destination) {
    try {
        if (!fs::exists(source)) return false;
        if (!fs::is_directory(source)) return false;
        
        fs::create_directories(fs::path(destination).parent_path());
        fs::rename(source, destination);
        return true;
    } catch (...) {
        return false;
    }
}

bool copy_file(const std::string& source, const std::string& destination) {
    try {
        if (!fs::exists(source)) return false;
        if (fs::is_directory(source)) return false;
        
        fs::create_directories(fs::path(destination).parent_path());
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool copy_directory(const std::string& source, const std::string& destination) {
    try {
        if (!fs::exists(source)) return false;
        if (!fs::is_directory(source)) return false;
        
        fs::create_directories(destination);
        fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> get_directory_contents(const std::string& path) {
    std::vector<std::string> contents;
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) return contents;
        
        for (const auto& entry : fs::directory_iterator(path)) {
            contents.push_back(entry.path().string());
        }
    } catch (...) {
    }
    
    return contents;
}

bool is_directory(const std::string& path) {
    try {
        return fs::is_directory(path);
    } catch (...) {
        return false;
    }
}

bool is_file(const std::string& path) {
    try {
        return fs::is_regular_file(path);
    } catch (...) {
        return false;
    }
}

bool delete_file_or_directory(const std::string& path) {
    try {
        if (!fs::exists(path)) return false;
        
        if (fs::is_directory(path)) {
            fs::remove_all(path);
        } else {
            fs::remove(path);
        }
        return true;
    } catch (...) {
        return false;
    }
}
