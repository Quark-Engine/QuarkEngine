#include "QuarkCore/QuarkCore.hpp"
#include "plugins/plugin_manager.h"
#include <filesystem>
#include <iostream>
#include <imgui.h>

using namespace qc;

namespace fs = std::filesystem;

void CPluginManager::LoadOne(const std::string& filepath)
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

        const fs::path sentinel = bin.parent_path() / (bin.stem().string() + ".disabled");
        if (fs::exists(sentinel))
        {
            TraceLog(LogLevel::Info, "PLUGIN", TextFormat("Skipping disabled plugin '%s'", bin.filename().string().c_str()));
            continue;
        }

        LoadOne(bin.string());
    }

    for (auto& lp : m_vPlugins)
    {
        pCtx->deltaTime = 0.0f;
        pCtx->entityCount = 0;
        pCtx->pSelected = nullptr;
        if (lp.pPlugin->pfnOnLoad) lp.pPlugin->pfnOnLoad(pCtx);
    }
}

void CPluginManager::UnloadAll()
{
    for (auto& lp : m_vPlugins)
    {
        if (lp.pPlugin->pfnOnUnload) lp.pPlugin->pfnOnUnload();
    }
    m_vPlugins.clear();
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
    m_vUiCallbacks.push_back({region, callback});
}

void CPluginManager::DrawUiRegion(EUIRegion region, SPluginContext& ctx)
{
    for (const SRegisteredUICallback& cb : m_vUiCallbacks)
    {
        if (cb.region == region)
        {
            cb.callback(&ctx);
        }
    }
}