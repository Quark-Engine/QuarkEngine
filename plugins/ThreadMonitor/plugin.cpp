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
        std::array<float, TIMING_SAMPLE_COUNT> m_aUpdateFrame = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aRendererUpdate = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aShadowPass = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aMain3dPass = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aImguiEditor = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aEndDrawing = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aHierarchy = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aInspector = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aScenePanel = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aAssets = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aModelViewer = {};
        std::array<float, TIMING_SAMPLE_COUNT> m_aPluginUpdate = {};
        int m_SampleCount = 0;
    };

    STimingHistory s_TimingHistory;

    void AddTimingSample(std::array<float, TIMING_SAMPLE_COUNT>& aSamples, float value)
    {
        std::move(aSamples.begin() + 1, aSamples.end(), aSamples.begin());
        aSamples.back() = value;
    }

    void RecordFrameTimings(const SPluginFrameTimings& timings)
    {
        AddTimingSample(s_TimingHistory.m_aUpdateFrame, timings.m_UpdateFrameMs);
        AddTimingSample(s_TimingHistory.m_aRendererUpdate, timings.m_SceneRendererUpdateMs);
        AddTimingSample(s_TimingHistory.m_aShadowPass, timings.m_ShadowPassMs);
        AddTimingSample(s_TimingHistory.m_aMain3dPass, timings.m_Main3dPassMs);
        AddTimingSample(s_TimingHistory.m_aImguiEditor, timings.m_ImguiEditorMs);
        AddTimingSample(s_TimingHistory.m_aEndDrawing, timings.m_EndDrawingMs);
        AddTimingSample(s_TimingHistory.m_aHierarchy, timings.m_HierarchyMs);
        AddTimingSample(s_TimingHistory.m_aInspector, timings.m_InspectorMs);
        AddTimingSample(s_TimingHistory.m_aScenePanel, timings.m_ScenePanelMs);
        AddTimingSample(s_TimingHistory.m_aAssets, timings.m_AssetsMs);
        AddTimingSample(s_TimingHistory.m_aModelViewer, timings.m_ModelViewerMs);
        AddTimingSample(s_TimingHistory.m_aPluginUpdate, timings.m_PluginUpdateMs);
        s_TimingHistory.m_SampleCount = std::min(s_TimingHistory.m_SampleCount + 1, TIMING_SAMPLE_COUNT);
    }

    float AverageTiming(const std::array<float, TIMING_SAMPLE_COUNT>& aSamples)
    {
        if (s_TimingHistory.m_SampleCount <= 0)
        {
            return 0.0f;
        }

        float total = 0.0f;
        const int firstSample = TIMING_SAMPLE_COUNT - s_TimingHistory.m_SampleCount;
        for (int sampleIndex = firstSample; sampleIndex < TIMING_SAMPLE_COUNT; ++sampleIndex)
        {
            total += aSamples[sampleIndex];
        }
        return total / static_cast<float>(s_TimingHistory.m_SampleCount);
    }

    void DrawTimingLine(SPluginContext* pCtx, const char* pLabel,
                        const std::array<float, TIMING_SAMPLE_COUNT>& aSamples, float maximum)
    {
        if (!pCtx->pfnUiPlotLines || s_TimingHistory.m_SampleCount <= 0)
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

        const SPluginFrameTimings& timings = pCtx->m_FrameTimings;
        char aLabel[192];
        std::snprintf(aLabel, sizeof(aLabel),
                      "UpdateFrame: %.3f ms (avg %.3f)", timings.m_UpdateFrameMs,
                      AverageTiming(s_TimingHistory.m_aUpdateFrame));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "CSceneRenderer::Update: %.3f ms (avg %.3f)", timings.m_SceneRendererUpdateMs,
                      AverageTiming(s_TimingHistory.m_aRendererUpdate));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "Shadow pass: %.3f ms (avg %.3f)", timings.m_ShadowPassMs,
                      AverageTiming(s_TimingHistory.m_aShadowPass));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "Main 3D pass: %.3f ms (avg %.3f)", timings.m_Main3dPassMs,
                      AverageTiming(s_TimingHistory.m_aMain3dPass));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "ImGui/editor UI: %.3f ms (avg %.3f)", timings.m_ImguiEditorMs,
                      AverageTiming(s_TimingHistory.m_aImguiEditor));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "EndDrawing: %.3f ms (avg %.3f)", timings.m_EndDrawingMs,
                      AverageTiming(s_TimingHistory.m_aEndDrawing));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "  Hierarchy: %.3f ms (avg %.3f)", timings.m_HierarchyMs,
                      AverageTiming(s_TimingHistory.m_aHierarchy));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "  Inspector: %.3f ms (avg %.3f)", timings.m_InspectorMs,
                      AverageTiming(s_TimingHistory.m_aInspector));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "  Scene panel: %.3f ms (avg %.3f)", timings.m_ScenePanelMs,
                      AverageTiming(s_TimingHistory.m_aScenePanel));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "  Assets: %.3f ms (avg %.3f)", timings.m_AssetsMs,
                      AverageTiming(s_TimingHistory.m_aAssets));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "  Model/material viewer: %.3f ms (avg %.3f)", timings.m_ModelViewerMs,
                      AverageTiming(s_TimingHistory.m_aModelViewer));
        pCtx->pfnUiText(aLabel);
        std::snprintf(aLabel, sizeof(aLabel),
                      "Plugin update + UI: %.3f ms (avg %.3f)", timings.m_PluginUpdateMs,
                      AverageTiming(s_TimingHistory.m_aPluginUpdate));
        pCtx->pfnUiText(aLabel);

        DrawTimingLine(pCtx, "UpdateFrame (ms)", s_TimingHistory.m_aUpdateFrame, 20.0f);
        DrawTimingLine(pCtx, "SceneRenderer::Update (ms)", s_TimingHistory.m_aRendererUpdate, 20.0f);
        DrawTimingLine(pCtx, "Shadow pass (ms)", s_TimingHistory.m_aShadowPass, 20.0f);
        DrawTimingLine(pCtx, "Main 3D pass (ms)", s_TimingHistory.m_aMain3dPass, 20.0f);
        DrawTimingLine(pCtx, "ImGui/editor UI (ms)", s_TimingHistory.m_aImguiEditor, 20.0f);
        DrawTimingLine(pCtx, "EndDrawing (ms)", s_TimingHistory.m_aEndDrawing, 20.0f);
        DrawTimingLine(pCtx, "Hierarchy (ms)", s_TimingHistory.m_aHierarchy, 10.0f);
        DrawTimingLine(pCtx, "Inspector (ms)", s_TimingHistory.m_aInspector, 10.0f);
        DrawTimingLine(pCtx, "Scene panel (ms)", s_TimingHistory.m_aScenePanel, 10.0f);
        DrawTimingLine(pCtx, "Assets (ms)", s_TimingHistory.m_aAssets, 10.0f);
        DrawTimingLine(pCtx, "Model viewer (ms)", s_TimingHistory.m_aModelViewer, 10.0f);
        DrawTimingLine(pCtx, "Plugin update + UI (ms)", s_TimingHistory.m_aPluginUpdate, 10.0f);
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
            RecordFrameTimings(pCtx->m_FrameTimings);
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

            if (!pCtx->m_pThreadUsages || pCtx->m_ThreadUsageCount <= 0)
            {
                pCtx->pfnUiText("Thread statistics are not available yet.");
            }
            else
            {
                char aLabel[128];
                for (int threadIndex = 0; threadIndex < pCtx->m_ThreadUsageCount; ++threadIndex)
                {
                    const SPluginThreadUsage& thread = pCtx->m_pThreadUsages[threadIndex];
                    std::snprintf(aLabel, sizeof(aLabel), "%s  %.1f%%",
                                  thread.m_pName ? thread.m_pName : "Unnamed thread",
                                  thread.m_UtilizationPercent);
                    pCtx->pfnUiText(aLabel);
                    std::snprintf(aLabel, sizeof(aLabel), "Task: %s",
                                  thread.m_pCurrentTask ? thread.m_pCurrentTask : "Idle");
                    pCtx->pfnUiText(aLabel);

                    if (pCtx->pfnUiPlotLines && thread.m_pHistory && thread.m_HistoryCount > 0)
                    {
                        std::snprintf(aLabel, sizeof(aLabel), "##thread_usage_%d", threadIndex);
                        pCtx->pfnUiPlotLines(aLabel, thread.m_pHistory,
                                             thread.m_HistoryCount, 0.0f, 100.0f);
                    }
                    if (pCtx->pfnUiSeparator && threadIndex + 1 < pCtx->m_ThreadUsageCount)
                    {
                        pCtx->pfnUiSeparator();
                    }
                }
            }
        }
        pCtx->pfnUiEnd();
    }

    SPlugin s_Plugin{
        "ThreadMonitor", "1.0.0",
        OnLoad, OnUnload, OnUpdate, OnDrawUI
    };

} // anonymous

PLUGIN_EXPORT SPlugin* GetPlugin()
{
    return &s_Plugin;
}
