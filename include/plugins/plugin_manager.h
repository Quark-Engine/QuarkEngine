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
    SPlugin* pPlugin;
    EUIRegion region;
    FPluginUICallback callback;
};

struct SRegisteredEventCallback
{
    SPlugin* pPlugin;
    EPluginEvent event;
    FPluginEventCallback callback;
};

class CPluginManager
{
public:
    void LoadAll(const std::string& pluginDir, SPluginContext* pCtx);
    void LoadOne(const std::string& filepath, SPluginContext* pCtx);
    void SetDisabledPlugins(const std::vector<std::string>& vPluginPaths);
    void UnloadPlugin(int index);
    void UnloadAll();
    void UpdateAll(SPluginContext& ctx);
    void DrawUiAll(SPluginContext& ctx);

    void RegisterUiCallback(EUIRegion region, FPluginUICallback callback);
    void DrawUiRegion(EUIRegion region, SPluginContext& ctx);
    void RegisterEventCallback(EPluginEvent event, FPluginEventCallback callback);
    void UnregisterEventCallback(EPluginEvent event, FPluginEventCallback callback);
    void DispatchEvent(EPluginEvent event, SPluginContext& ctx, int entityIndex);

    const std::vector<SLoadedPlugin>& GetPlugins() const
    {
        return m_vPlugins;
    }

private:
    void RemoveCallbacksForPlugin(SPlugin* pPlugin);

    std::vector<SLoadedPlugin> m_vPlugins;
    std::vector<SRegisteredUICallback> m_vUiCallbacks;
    std::vector<SRegisteredEventCallback> m_vEventCallbacks;
    std::vector<std::string> m_vDisabledPlugins;
    SPlugin* m_pRegisteringPlugin = nullptr;
};

#endif // __PLUGIN_MANAGER_H__
