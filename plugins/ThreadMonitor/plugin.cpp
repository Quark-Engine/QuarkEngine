#include "plugins/plugin.h"

#include <array>
#include <algorithm>
#include <cstdio>
#include <utility>

namespace
{

constexpr int TIMING_SAMPLE_COUNT = 120;

struct STimingHistory
{
    std::array<float, TIMING_SAMPLE_COUNT> aUpdateFrame = {};
    std::array<float, TIMING_SAMPLE_COUNT> aRendererUpdate = {};
    std::array<float, TIMING_SAMPLE_COUNT> aShadowPass = {};
    std::array<float, TIMING_SAMPLE_COUNT> aMain3dPass = {};
    std::array<float, TIMING_SAMPLE_COUNT> aImguiEditor = {};
    std::array<float, TIMING_SAMPLE_COUNT> aEndDrawing = {};
    std::array<float, TIMING_SAMPLE_COUNT> aHierarchy = {};
    std::array<float, TIMING_SAMPLE_COUNT> aInspector = {};
    std::array<float, TIMING_SAMPLE_COUNT> aScenePanel = {};
    std::array<float, TIMING_SAMPLE_COUNT> aAssets = {};
    std::array<float, TIMING_SAMPLE_COUNT> aModelViewer = {};
    std::array<float, TIMING_SAMPLE_COUNT> aPluginUpdate = {};
    int sampleCount = 0;
};

STimingHistory s_TimingHistory;

void AddTimingSample(std::array<float, TIMING_SAMPLE_COUNT>& aSamples, float value)
{
    std::move(aSamples.begin() + 1, aSamples.end(), aSamples.begin());
    aSamples.back() = value;
}

void RecordFrameTimings(const SPluginFrameTimings& timings)
{
    AddTimingSample(s_TimingHistory.aUpdateFrame, timings.updateFrameMs);
    AddTimingSample(s_TimingHistory.aRendererUpdate, timings.sceneRendererUpdateMs);
    AddTimingSample(s_TimingHistory.aShadowPass, timings.shadowPassMs);
    AddTimingSample(s_TimingHistory.aMain3dPass, timings.main3dPassMs);
    AddTimingSample(s_TimingHistory.aImguiEditor, timings.imguiEditorMs);
    AddTimingSample(s_TimingHistory.aEndDrawing, timings.endDrawingMs);
    AddTimingSample(s_TimingHistory.aHierarchy, timings.hierarchyMs);
    AddTimingSample(s_TimingHistory.aInspector, timings.inspectorMs);
    AddTimingSample(s_TimingHistory.aScenePanel, timings.scenePanelMs);
    AddTimingSample(s_TimingHistory.aAssets, timings.assetsMs);
    AddTimingSample(s_TimingHistory.aModelViewer, timings.modelViewerMs);
    AddTimingSample(s_TimingHistory.aPluginUpdate, timings.pluginUpdateMs);
    s_TimingHistory.sampleCount = std::min(s_TimingHistory.sampleCount + 1, TIMING_SAMPLE_COUNT);
}

float AverageTiming(const std::array<float, TIMING_SAMPLE_COUNT>& aSamples)
{
    if (s_TimingHistory.sampleCount <= 0)
    {
        return 0.0f;
    }

    float total = 0.0f;
    const int firstSample = TIMING_SAMPLE_COUNT - s_TimingHistory.sampleCount;
    for (int sampleIndex = firstSample; sampleIndex < TIMING_SAMPLE_COUNT; ++sampleIndex)
    {
        total += aSamples[sampleIndex];
    }
    return total / static_cast<float>(s_TimingHistory.sampleCount);
}

void DrawTimingLine(SPluginContext* pCtx, const char* pLabel,
    const std::array<float, TIMING_SAMPLE_COUNT>& aSamples, float maximum)
{
    if (!pCtx->pfnUiPlotLines || s_TimingHistory.sampleCount <= 0)
    {
        return;
    }
    pCtx->pfnUiPlotLines(pLabel, aSamples.data(), TIMING_SAMPLE_COUNT, 0.0f, maximum);
}

void DrawFrameTimings(SPluginContext* pCtx)
{
    if (!pCtx->pfnUiText)
    {
        return;
    }

    const SPluginFrameTimings& timings = pCtx->frameTimings;
    char label[192];
    std::snprintf(label, sizeof(label),
        "UpdateFrame: %.3f ms (avg %.3f)", timings.updateFrameMs,
        AverageTiming(s_TimingHistory.aUpdateFrame));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "CSceneRenderer::Update: %.3f ms (avg %.3f)", timings.sceneRendererUpdateMs,
        AverageTiming(s_TimingHistory.aRendererUpdate));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "Shadow pass: %.3f ms (avg %.3f)", timings.shadowPassMs,
        AverageTiming(s_TimingHistory.aShadowPass));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "Main 3D pass: %.3f ms (avg %.3f)", timings.main3dPassMs,
        AverageTiming(s_TimingHistory.aMain3dPass));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "ImGui/editor UI: %.3f ms (avg %.3f)", timings.imguiEditorMs,
        AverageTiming(s_TimingHistory.aImguiEditor));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "EndDrawing: %.3f ms (avg %.3f)", timings.endDrawingMs,
        AverageTiming(s_TimingHistory.aEndDrawing));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "  Hierarchy: %.3f ms (avg %.3f)", timings.hierarchyMs,
        AverageTiming(s_TimingHistory.aHierarchy));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "  Inspector: %.3f ms (avg %.3f)", timings.inspectorMs,
        AverageTiming(s_TimingHistory.aInspector));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "  Scene panel: %.3f ms (avg %.3f)", timings.scenePanelMs,
        AverageTiming(s_TimingHistory.aScenePanel));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "  Assets: %.3f ms (avg %.3f)", timings.assetsMs,
        AverageTiming(s_TimingHistory.aAssets));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "  Model/material viewer: %.3f ms (avg %.3f)", timings.modelViewerMs,
        AverageTiming(s_TimingHistory.aModelViewer));
    pCtx->pfnUiText(label);
    std::snprintf(label, sizeof(label),
        "Plugin update + UI: %.3f ms (avg %.3f)", timings.pluginUpdateMs,
        AverageTiming(s_TimingHistory.aPluginUpdate));
    pCtx->pfnUiText(label);

    DrawTimingLine(pCtx, "UpdateFrame (ms)", s_TimingHistory.aUpdateFrame, 20.0f);
    DrawTimingLine(pCtx, "SceneRenderer::Update (ms)", s_TimingHistory.aRendererUpdate, 20.0f);
    DrawTimingLine(pCtx, "Shadow pass (ms)", s_TimingHistory.aShadowPass, 20.0f);
    DrawTimingLine(pCtx, "Main 3D pass (ms)", s_TimingHistory.aMain3dPass, 20.0f);
    DrawTimingLine(pCtx, "ImGui/editor UI (ms)", s_TimingHistory.aImguiEditor, 20.0f);
    DrawTimingLine(pCtx, "EndDrawing (ms)", s_TimingHistory.aEndDrawing, 20.0f);
    DrawTimingLine(pCtx, "Hierarchy (ms)", s_TimingHistory.aHierarchy, 10.0f);
    DrawTimingLine(pCtx, "Inspector (ms)", s_TimingHistory.aInspector, 10.0f);
    DrawTimingLine(pCtx, "Scene panel (ms)", s_TimingHistory.aScenePanel, 10.0f);
    DrawTimingLine(pCtx, "Assets (ms)", s_TimingHistory.aAssets, 10.0f);
    DrawTimingLine(pCtx, "Model viewer (ms)", s_TimingHistory.aModelViewer, 10.0f);
    DrawTimingLine(pCtx, "Plugin update + UI (ms)", s_TimingHistory.aPluginUpdate, 10.0f);
}

void OnLoad(SPluginContext*)
{
    s_TimingHistory = {};
}

void OnUnload()
{
    s_TimingHistory = {};
}

void OnUpdate(SPluginContext* pCtx)
{
    if (pCtx)
    {
        RecordFrameTimings(pCtx->frameTimings);
    }
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
        if (pCtx->pfnUiCollapsingHeader &&
            pCtx->pfnUiCollapsingHeader("Frame timings"))
        {
            DrawFrameTimings(pCtx);
            if (pCtx->pfnUiSeparator)
            {
                pCtx->pfnUiSeparator();
            }
        }

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
