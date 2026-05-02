#pragma once
#include "plugin.h"
#include <vector>
#include <string>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN

    #define CloseWindow WinCloseWindow
    #define ShowCursor WinShowCursor
    #define Rectangle WinRectangle

    #include <windows.h>

    #undef CloseWindow
    #undef ShowCursor
    #undef Rectangle
    typedef HMODULE LibHandle;
#else
    #include <dlfcn.h>
    typedef void* LibHandle;
#endif

struct LoadedPlugin {
    LibHandle handle;
    Plugin* plugin;
    std::string filepath;
};

class PluginManager {
public:
    void load_all(const std::string& plugin_dir);
    void load(const std::string& filepath);
    void unload_all();
    void update_all(PluginContext& ctx);
    void draw_ui_all(PluginContext& ctx);
    
    const std::vector<LoadedPlugin>& get_plugins() const { return plugins; }

private:
    std::vector<LoadedPlugin> plugins;
};