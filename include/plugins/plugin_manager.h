#ifndef __PLUGIN_MANAGER_H__
#define __PLUGIN_MANAGER_H__
#include "plugin.h"
#include "dynamic_library.h"
#include <vector>
#include <algorithm>
#include <string>

struct SLoadedPlugin
{
    CDynamicLibrary Library;
    SPlugin* pPlugin;
    std::string FilePath;
};

struct SRegisteredUICallback
{
    EUIRegion region;
    FPluginUICallback callback;
};

class CPluginManager
{
public:
    void LoadAll(const std::string& pluginDir, SPluginContext* pCtx);
    void LoadOne(const std::string& filepath);
    void UnloadAll();
    void UpdateAll(SPluginContext& ctx);
    void DrawUiAll(SPluginContext& ctx);

    void RegisterUiCallback(EUIRegion region, FPluginUICallback callback);
    void DrawUiRegion(EUIRegion region, SPluginContext& ctx);

    const std::vector<SLoadedPlugin>& GetPlugins() const
    {
        return m_vPlugins;
    }

private:
    std::vector<SLoadedPlugin> m_vPlugins;
    std::vector<SRegisteredUICallback> m_vUiCallbacks;
};

#endif // __PLUGIN_MANAGER_H__