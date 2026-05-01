#include "plugin_manager.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static LibHandle open_lib(const std::string& path) {
    #ifdef _WIN32
        return LoadLibraryA(path.c_str());
    #else
        return dlopen(path.c_str(), RTLD_NOW);
    #endif
}

static void* get_sym(LibHandle handle, const char* name) {
    #ifdef _WIN32
        return (void*)GetProcAddress(handle, name);
    #else
        return dlsym(handle, name);
    #endif
}

static void close_lib(LibHandle handle) {
    #ifdef _WIN32
        FreeLibrary(handle);
    #else
        dlclose(handle);
    #endif
}

void PluginManager::load(const std::string& filepath) {
    LibHandle handle = open_lib(filepath);
    if (!handle) {
        std::cerr << "Failed to load plugin: " << filepath << std::endl;
        return;
    }

    using get_plugin_func = Plugin*(*)();
    get_plugin_func get_plugin = (get_plugin_func)get_sym(handle, "get_plugin");

    if (!get_plugin) {
        std::cerr << "Plugin missing quark_get_plugin:" << filepath << std::endl;
        close_lib(handle);
        return;
    }

    Plugin* plugin = get_plugin();
    if (!plugin) {
        std::cerr << "Invalid plugin (missing metadata): " << filepath << std::endl;
        close_lib(handle);
        return;
    }

    plugins.push_back({ handle, plugin, filepath });
    TraceLog(LOG_INFO, "Loaded plugin: %s %s", plugin->name, plugin->version);
}

void PluginManager::load_all(const std::string& plugin_dir) {
    if (!fs::exists(plugin_dir)) {
        fs::create_directories(plugin_dir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        const std::string ext = entry.path().extension().string();

        #ifdef _WIN32
            if (ext != ".dll") continue;
        #elif __APPLE__
            if (ext != ".dylib") continue;
        #else
            if (ext != ".so") continue;
        #endif

        load(entry.path().string());
    }
}

void PluginManager::unload_all() {
    for (auto& lp : plugins) {
        if (lp.plugin->on_unload) lp.plugin->on_unload();
        close_lib(lp.handle);
    }

    plugins.clear();
}

void PluginManager::update_all(PluginContext& ctx) {
    for (auto& lp : plugins) {
        if (lp.plugin->on_update) lp.plugin->on_update(&ctx);
    }
}

void PluginManager::draw_ui_all(PluginContext& ctx) {
    for (auto& lp : plugins) {
        if (lp.plugin->on_draw_ui) lp.plugin->on_draw_ui(&ctx);
    }
}