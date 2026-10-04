#include "QuarkCore/QuarkCore.hpp"
#include "plugins/plugin_manager.h"
#include <filesystem>
#include <iostream>
#include <imgui.h>

using namespace qc;

namespace fs = std::filesystem;

namespace
{
std::string NormalizePluginPath(const std::string& path)
{
    return fs::path(path).lexically_normal().string();
}
} // anonymous

void CPluginManager::LoadOne(const std::string& filepath, SPluginContext* pCtx)
{
    CDynamicLibrary library;
    if (!library.Open(filepath))
    {
        TraceLog(LogLevel::Error, "PLUGIN", TextFormat("Failed to load '%s': %s", filepath.c_str(), library.GetError().c_str()));
        return;
    }

    using GetPluginFunc = SPlugin*(*)();
    GetPluginFunc pfnGetPlugin = reinterpret_cast<GetPluginFunc>(library.GetSymbol("GetPlugin"));

    if (!pfnGetPlugin)
    {
        TraceLog(LogLevel::Error, "PLUGIN", TextFormat("Failed to load '%s': missing 'GetPlugin' entry point", filepath.c_str()));
        return;
    }

    SPlugin* pPlugin = pfnGetPlugin();
    if (!pPlugin)
    {
        return;
    }

    m_vPlugins.push_back({ std::move(library), pPlugin, filepath });
    TraceLog(LogLevel::Info, "PLUGIN", TextFormat("Loaded '%s' v%s", pPlugin->pName, pPlugin->pVersion));

    if (pCtx != nullptr && pPlugin->pfnOnLoad)
    {
        m_pRegisteringPlugin = pPlugin;
        pCtx->deltaTime = 0.0f;
        pCtx->entityCount = 0;
        pCtx->pSelected = nullptr;
        pPlugin->pfnOnLoad(pCtx);
        m_pRegisteringPlugin = nullptr;
    }
}

void CPluginManager::SetDisabledPlugins(const std::vector<std::string>& vPluginPaths)
{
    m_vDisabledPlugins.clear();
    for (const std::string& path : vPluginPaths)
    {
        m_vDisabledPlugins.push_back(NormalizePluginPath(path));
    }
}

void CPluginManager::LoadAll(const std::string& pluginDir, SPluginContext* pCtx)
{
    if (!fs::exists(pluginDir))
    {
        fs::create_directories(pluginDir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(pluginDir))
    {
        fs::path bin;

        if (entry.is_directory())
        {
            for (const auto& file : fs::directory_iterator(entry.path()))
            {
                const std::string ext = file.path().extension().string();
#ifdef _WIN32
                if (ext == ".dll")
                {
                    bin = file.path();
                    break;
                }
#elif __APPLE__
                if (ext == ".dylib")
                {
                    bin = file.path();
                    break;
                }
#else
                if (ext == ".so")
                {
                    bin = file.path();
                    break;
                }
#endif
            }
        }
        else if (entry.is_regular_file())
        {
            const std::string ext = entry.path().extension().string();
#ifdef _WIN32
            if (ext == ".dll") bin = entry.path();
#elif __APPLE__
            if (ext == ".dylib") bin = entry.path();
#else
            if (ext == ".so") bin = entry.path();
#endif
        }

        if (bin.empty()) continue;

        const std::string normalizedPath = NormalizePluginPath(bin.string());
        if (std::find(m_vDisabledPlugins.begin(), m_vDisabledPlugins.end(), normalizedPath) !=
            m_vDisabledPlugins.end())
        {
            TraceLog(LogLevel::Info, "PLUGIN", TextFormat("Skipping disabled plugin '%s'",
                bin.filename().string().c_str()));
            continue;
        }

        const fs::path sentinel = bin.parent_path() / (bin.stem().string() + ".disabled");
        if (fs::exists(sentinel))
        {
            TraceLog(LogLevel::Info, "PLUGIN", TextFormat("Skipping disabled plugin '%s'", bin.filename().string().c_str()));
            continue;
        }

        LoadOne(bin.string(), pCtx);
    }
}

void CPluginManager::UnloadAll()
{
    for (auto& lp : m_vPlugins)
    {
        if (lp.pPlugin->pfnOnUnload) lp.pPlugin->pfnOnUnload();
    }
    m_vEventCallbacks.clear();
    m_vUiCallbacks.clear();
    m_vPlugins.clear();
}

void CPluginManager::RemoveCallbacksForPlugin(SPlugin* pPlugin)
{
    m_vUiCallbacks.erase(
        std::remove_if(m_vUiCallbacks.begin(), m_vUiCallbacks.end(),
            [pPlugin](const SRegisteredUICallback& callback)
            {
                return callback.pPlugin == pPlugin;
            }),
        m_vUiCallbacks.end());
    m_vEventCallbacks.erase(
        std::remove_if(m_vEventCallbacks.begin(), m_vEventCallbacks.end(),
            [pPlugin](const SRegisteredEventCallback& callback)
            {
                return callback.pPlugin == pPlugin;
            }),
        m_vEventCallbacks.end());
}

void CPluginManager::UnloadPlugin(int index)
{
    if (index < 0 || index >= static_cast<int>(m_vPlugins.size()))
    {
        return;
    }

    SLoadedPlugin& plugin = m_vPlugins[index];
    SPlugin* pPlugin = plugin.pPlugin;
    if (pPlugin->pfnOnUnload)
    {
        pPlugin->pfnOnUnload();
    }
    RemoveCallbacksForPlugin(pPlugin);
    m_vPlugins.erase(m_vPlugins.begin() + index);
}

void CPluginManager::UpdateAll(SPluginContext& ctx)
{
    for (auto& lp : m_vPlugins)
    {
        if (lp.pPlugin->pfnOnUpdate) lp.pPlugin->pfnOnUpdate(&ctx);
    }
}

void CPluginManager::DrawUiAll(SPluginContext& ctx)
{
    for (auto& lp : m_vPlugins)
    {
        if (lp.pPlugin->pfnOnDrawUI) lp.pPlugin->pfnOnDrawUI(&ctx);
    }
}

void CPluginManager::RegisterUiCallback(EUIRegion region, FPluginUICallback callback)
{
    m_vUiCallbacks.push_back({m_pRegisteringPlugin, region, callback});
}

void CPluginManager::DrawUiRegion(EUIRegion region, SPluginContext& ctx)
{
    for (size_t callbackIndex = 0; callbackIndex < m_vUiCallbacks.size(); ++callbackIndex)
    {
        const SRegisteredUICallback cb = m_vUiCallbacks[callbackIndex];
        if (cb.region == region)
        {
            cb.callback(&ctx);
        }
    }
}

void CPluginManager::RegisterEventCallback(EPluginEvent event, FPluginEventCallback callback)
{
    if (callback)
    {
        m_vEventCallbacks.push_back({m_pRegisteringPlugin, event, callback});
    }
}

void CPluginManager::UnregisterEventCallback(EPluginEvent event, FPluginEventCallback callback)
{
    m_vEventCallbacks.erase(
        std::remove_if(m_vEventCallbacks.begin(), m_vEventCallbacks.end(),
            [event, callback](const SRegisteredEventCallback& registered)
            {
                return registered.event == event && registered.callback == callback;
            }),
        m_vEventCallbacks.end());
}

void CPluginManager::DispatchEvent(EPluginEvent event, SPluginContext& ctx, int entityIndex)
{
    const std::vector<SRegisteredEventCallback> vCallbacks = m_vEventCallbacks;
    for (const SRegisteredEventCallback& registered : vCallbacks)
    {
        if (registered.event == event && registered.callback)
        {
            registered.callback(&ctx, event, entityIndex);
        }
    }
}
