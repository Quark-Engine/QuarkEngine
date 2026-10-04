#include "plugins/plugin.h"

#include <cstdio>

namespace
{

void OnLoad(SPluginContext*)
{
}

void OnUnload()
{
}

void OnUpdate(SPluginContext*)
{
}

void OnDrawUI(SPluginContext* pCtx)
{
    if (!pCtx || !pCtx->pfnUiBegin || !pCtx->pfnUiEnd)
    {
        return;
    }

    const bool isOpen = pCtx->pfnUiBegin("Thread Monitor");
    if (isOpen)
    {
        if (!pCtx->pThreadUsages || pCtx->threadUsageCount <= 0)
        {
            pCtx->pfnUiText("Thread statistics are not available yet.");
        }
        else
        {
            char label[128];
            for (int threadIndex = 0; threadIndex < pCtx->threadUsageCount; ++threadIndex)
            {
                const SPluginThreadUsage& thread = pCtx->pThreadUsages[threadIndex];
                std::snprintf(label, sizeof(label), "%s  %.1f%%",
                    thread.pName ? thread.pName : "Unnamed thread",
                    thread.utilizationPercent);
                pCtx->pfnUiText(label);
                std::snprintf(label, sizeof(label), "Task: %s",
                    thread.pCurrentTask ? thread.pCurrentTask : "Idle");
                pCtx->pfnUiText(label);

                if (pCtx->pfnUiPlotLines && thread.pHistory && thread.historyCount > 0)
                {
                    std::snprintf(label, sizeof(label), "##thread_usage_%d", threadIndex);
                    pCtx->pfnUiPlotLines(label, thread.pHistory,
                        thread.historyCount, 0.0f, 100.0f);
                }
                if (pCtx->pfnUiSeparator && threadIndex + 1 < pCtx->threadUsageCount)
                {
                    pCtx->pfnUiSeparator();
                }
            }
        }
    }
    pCtx->pfnUiEnd();
}

SPlugin g_Plugin
{
    "ThreadMonitor", "1.0.0",
    OnLoad, OnUnload, OnUpdate, OnDrawUI
};

} // namespace

PLUGIN_EXPORT SPlugin* GetPlugin()
{
    return &g_Plugin;
}
