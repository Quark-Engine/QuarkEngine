#define NOMINMAX

#include "QuarkCore/QuarkCore.hpp"
#include "plugin_manager.h"
#include <filesystem>
#include <iostream>
#include <imgui.h>

using namespace qc;

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
    #ifdef _WIN32
        LibHandle handle = LoadLibraryA(filepath.c_str());
        if (!handle) {
            DWORD err = GetLastError();
            char msg[256];
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, 0, msg, sizeof(msg), nullptr);
            TraceLog(LogLevel::Error, "PLUGIN", TextFormat("Failed to load '%s': %s", filepath.c_str(), msg));
            return;
        }
    #else
        LibHandle handle = dlopen(filepath.c_str(), RTLD_NOW);
        if (!handle) {
            TraceLog(LogLevel::Error, "PLUGIN", TextFormat("Failed to load '%s': %s", filepath.c_str(), dlerror()));
            return;
        }
    #endif

        using get_plugin_func = Plugin*(*)();
        get_plugin_func get_plugin = (get_plugin_func)get_sym(handle, "get_plugin");

        if (!get_plugin) {
            close_lib(handle);
            return;
        }

        Plugin* plugin = get_plugin();
        if (!plugin) {
            close_lib(handle);
            return;
        }

        plugins.push_back({ handle, plugin, filepath });
        TraceLog(LogLevel::Info, "PLUGIN", TextFormat("Loaded '%s' v%s", plugin->name, plugin->version));
}

void PluginManager::load_all(const std::string& plugin_dir, PluginContext* ctx) {
    if (!fs::exists(plugin_dir)) {
        fs::create_directories(plugin_dir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        fs::path bin;

        if (entry.is_directory()) {
            for (auto& f : fs::directory_iterator(entry.path())) {
                std::string ext = f.path().extension().string();
#ifdef _WIN32
                if (ext == ".dll") { bin = f.path(); break; }
#elif __APPLE__
                if (ext == ".dylib") { bin = f.path(); break; }
#else
                if (ext == ".so") { bin = f.path(); break; }
#endif
            }
        } 
        
        else if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
#ifdef _WIN32
            if (ext == ".dll") bin = entry.path();
#elif __APPLE__
            if (ext == ".dylib") bin = entry.path();
#else
            if (ext == ".so") bin = entry.path();
#endif
        }

        if (bin.empty()) continue;

        fs::path sentinel = bin.parent_path() / (bin.stem().string() + ".disabled");
        if (fs::exists(sentinel)) {
            TraceLog(LOG_INFO, "PLUGIN: Skipping disabled plugin '%s'",
                bin.filename().string().c_str());
            continue;
        }

        load(bin.string());
    }

    for (auto& lp : plugins) {
        ctx->delta_time   = 0.0f;
        ctx->entity_count = 0;
        ctx->selected     = nullptr;
        if (lp.plugin->on_load) lp.plugin->on_load(ctx);
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

void PluginManager::register_ui_callback(UIRegion region, PluginUICallback callback) {
    ui_callbacks.push_back({region, callback});
}

void PluginManager::draw_ui_region(UIRegion region, PluginContext& ctx) {
    for (RegisteredUICallback cb : ui_callbacks) {
        if (cb.region == region)
            cb.callback(&ctx);
    }
}