#include "plugins/plugin.h"

#include "component.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace
{

constexpr int FRAME_SAMPLE_COUNT = 120;
constexpr float MAP_HEIGHT = 220.0f;
constexpr float MAP_MARGIN = 12.0f;

struct SDeveloperToolsState
{
    std::array<float, FRAME_SAMPLE_COUNT> aFrameTimes = {};
    std::array<float, FRAME_SAMPLE_COUNT> aPluginTimes = {};
    int frameSampleCount = 0;
    bool showColliders = true;
    bool showLights = true;
    bool showMeshBounds = true;
    bool showMetrics = false;
    bool showDebugLog = false;
};

SDeveloperToolsState s_State;

struct SEntityDebugData
{
    float x = 0.0f;
    float z = 0.0f;
    float scaleX = 1.0f;
    float scaleZ = 1.0f;
    float rotationY = 0.0f;
    float meshOffsetX = 0.0f;
    float meshOffsetZ = 0.0f;
    float meshHalfX = 0.5f;
    float meshHalfZ = 0.5f;
    float colliderOffsetX = 0.0f;
    float colliderOffsetZ = 0.0f;
    float colliderX = 0.5f;
    float colliderZ = 0.5f;
    float colliderRadius = 0.5f;
    float lightRange = 5.0f;
    bool hasCollider = false;
    bool colliderRound = false;
    bool hasLight = false;
    bool hasMesh = false;
};

void AddFrameSample(std::array<float, FRAME_SAMPLE_COUNT>& samples, float value)
{
    std::move(samples.begin() + 1, samples.end(), samples.begin());
    samples.back() = value;
}

void OnLoad(SPluginContext*)
{
    s_State = {};
    s_State.showColliders = true;
    s_State.showLights = true;
    s_State.showMeshBounds = true;
}

void OnUnload()
{
    s_State = {};
}

void OnUpdate(SPluginContext* pCtx)
{
    if (!pCtx)
    {
        return;
    }

    const auto startTime = std::chrono::steady_clock::now();
    const float frameTimeMs = std::max(0.0f, pCtx->deltaTime * 1000.0f);
    AddFrameSample(s_State.aFrameTimes, frameTimeMs);

    const auto elapsed = std::chrono::steady_clock::now() - startTime;
    const float pluginTimeMs = std::chrono::duration<float, std::milli>(elapsed).count();
    AddFrameSample(s_State.aPluginTimes, pluginTimeMs);
    s_State.frameSampleCount = std::min(s_State.frameSampleCount + 1, FRAME_SAMPLE_COUNT);
}

SEntityDebugData ReadEntityDebugData(const CEntity& entity)
{
    SEntityDebugData data;
    if (!entity.m_pComponents)
    {
        return data;
    }

    const std::shared_ptr<CTransformComponent> pTransform =
        entity.m_pComponents->GetComponentOfType<CTransformComponent>();
    if (pTransform)
    {
        data.x = pTransform->m_Position.x;
        data.z = pTransform->m_Position.z;
        data.scaleX = std::abs(pTransform->m_Scale.x);
        data.scaleZ = std::abs(pTransform->m_Scale.z);
        data.rotationY = pTransform->m_Rotation.y * 0.0174532925f;
    }

    const std::shared_ptr<CCollisionComponent> pCollider =
        entity.m_pComponents->GetComponentOfType<CCollisionComponent>();
    if (pCollider && pCollider->m_Enabled)
    {
        const float colliderCenterX = pCollider->m_Center.x * data.scaleX;
        const float colliderCenterZ = pCollider->m_Center.z * data.scaleZ;
        const float cosRotation = std::cos(data.rotationY);
        const float sinRotation = std::sin(data.rotationY);
        data.hasCollider = true;
        data.colliderOffsetX = colliderCenterX * cosRotation - colliderCenterZ * sinRotation;
        data.colliderOffsetZ = colliderCenterX * sinRotation + colliderCenterZ * cosRotation;
        data.colliderX = std::abs(pCollider->m_Size.x * data.scaleX) * 0.5f;
        data.colliderZ = std::abs(pCollider->m_Size.z * data.scaleZ) * 0.5f;
        data.colliderRadius = std::abs(pCollider->m_Radius) *
            std::max(data.scaleX, data.scaleZ);
        if (pCollider->m_ColliderType == COLLIDER_SPHERE ||
            pCollider->m_ColliderType == COLLIDER_CAPSULE)
        {
            data.colliderX = data.colliderRadius;
            data.colliderZ = data.colliderRadius;
            data.colliderRound = true;
        }
    }

    const std::shared_ptr<CLightComponent> pLight =
        entity.m_pComponents->GetComponentOfType<CLightComponent>();
    if (pLight && pLight->m_Enabled && pLight->m_Light.m_Enabled)
    {
        data.hasLight = true;
        data.lightRange = std::max(0.1f, pLight->m_Light.m_Range);
    }

    const std::shared_ptr<CMeshComponent> pMesh =
        entity.m_pComponents->GetComponentOfType<CMeshComponent>();
    data.hasMesh = pMesh && pMesh->m_Enabled && pMesh->m_Model.meshes &&
        pMesh->m_Model.meshCount > 0;
    if (data.hasMesh)
    {
        float minX = 0.0f;
        float maxX = 0.0f;
        float minZ = 0.0f;
        float maxZ = 0.0f;
        bool hasVertices = false;
        for (int meshIndex = 0; meshIndex < pMesh->m_Model.meshCount; ++meshIndex)
        {
            const qc::Mesh& mesh = pMesh->m_Model.meshes[meshIndex];
            if (!mesh.vertices)
            {
                continue;
            }

            for (int vertexIndex = 0; vertexIndex < mesh.vertexCount; ++vertexIndex)
            {
                const float x = mesh.vertices[vertexIndex * 3];
                const float z = mesh.vertices[vertexIndex * 3 + 2];
                if (!hasVertices)
                {
                    minX = maxX = x;
                    minZ = maxZ = z;
                    hasVertices = true;
                    continue;
                }
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minZ = std::min(minZ, z);
                maxZ = std::max(maxZ, z);
            }
        }

        if (hasVertices)
        {
            const float localCenterX = (maxX + minX) * 0.5f * data.scaleX;
            const float localCenterZ = (maxZ + minZ) * 0.5f * data.scaleZ;
            const float localHalfX = (maxX - minX) * 0.5f * data.scaleX;
            const float localHalfZ = (maxZ - minZ) * 0.5f * data.scaleZ;
            const float cosRotation = std::abs(std::cos(data.rotationY));
            const float sinRotation = std::abs(std::sin(data.rotationY));
            data.meshOffsetX = localCenterX * std::cos(data.rotationY) -
                localCenterZ * std::sin(data.rotationY);
            data.meshOffsetZ = localCenterX * std::sin(data.rotationY) +
                localCenterZ * std::cos(data.rotationY);
            data.meshHalfX = cosRotation * localHalfX + sinRotation * localHalfZ;
            data.meshHalfZ = sinRotation * localHalfX + cosRotation * localHalfZ;
        }
    }
    return data;
}

void DrawSceneMap(SPluginContext* pCtx)
{
    if (!pCtx->pScene || !pCtx->pfnUiBeginChild || !pCtx->pfnUiEndChild ||
        !pCtx->pfnUiGetContentRegionAvail || !pCtx->pfnUiGetCursorScreenPos ||
        !pCtx->pfnUiDrawRect || !pCtx->pfnUiDrawLine ||
        !pCtx->pfnUiDrawCircle || !pCtx->pfnUiDrawCircleFilled || !pCtx->pfnUiDrawText)
    {
        pCtx->pfnUiText("Scene visualization callbacks are unavailable.");
        return;
    }

    const bool isVisible = pCtx->pfnUiBeginChild("##developer_scene_map", 0.0f, MAP_HEIGHT);
    if (isVisible)
    {
        float width = 0.0f;
        float height = 0.0f;
        float originX = 0.0f;
        float originY = 0.0f;
        pCtx->pfnUiGetContentRegionAvail(&width, &height);
        pCtx->pfnUiGetCursorScreenPos(&originX, &originY);

        const float mapWidth = std::max(40.0f, width);
        const float mapHeight = std::max(40.0f, height);
        const float centerX = originX + mapWidth * 0.5f;
        const float centerY = originY + mapHeight * 0.5f;
        float worldMinX = -10.0f;
        float worldMaxX = 10.0f;
        float worldMinZ = -10.0f;
        float worldMaxZ = 10.0f;

        for (const CEntity& entity : pCtx->pScene->m_vEntities)
        {
            const SEntityDebugData data = ReadEntityDebugData(entity);
            const float extentX = std::max(data.hasMesh ? data.meshHalfX : 0.0f, data.colliderX);
            const float extentZ = std::max(data.hasMesh ? data.meshHalfZ : 0.0f, data.colliderZ);
            const float meshCenterX = data.x + data.meshOffsetX;
            const float meshCenterZ = data.z + data.meshOffsetZ;
            const float colliderCenterX = data.x + data.colliderOffsetX;
            const float colliderCenterZ = data.z + data.colliderOffsetZ;
            worldMinX = std::min(worldMinX, std::min(meshCenterX - extentX, colliderCenterX - extentX));
            worldMaxX = std::max(worldMaxX, std::max(meshCenterX + extentX, colliderCenterX + extentX));
            worldMinZ = std::min(worldMinZ, std::min(meshCenterZ - extentZ, colliderCenterZ - extentZ));
            worldMaxZ = std::max(worldMaxZ, std::max(meshCenterZ + extentZ, colliderCenterZ + extentZ));
            if (data.hasLight)
            {
                worldMinX = std::min(worldMinX, data.x - data.lightRange);
                worldMaxX = std::max(worldMaxX, data.x + data.lightRange);
                worldMinZ = std::min(worldMinZ, data.z - data.lightRange);
                worldMaxZ = std::max(worldMaxZ, data.z + data.lightRange);
            }
        }

        const float worldWidth = std::max(1.0f, worldMaxX - worldMinX);
        const float worldHeight = std::max(1.0f, worldMaxZ - worldMinZ);
        const float scale = std::max(0.01f, std::min(
            (mapWidth - MAP_MARGIN * 2.0f) / worldWidth,
            (mapHeight - MAP_MARGIN * 2.0f) / worldHeight));
        const float worldCenterX = (worldMinX + worldMaxX) * 0.5f;
        const float worldCenterZ = (worldMinZ + worldMaxZ) * 0.5f;
        const auto mapX = [centerX, scale, worldCenterX](float value)
        {
            return centerX + (value - worldCenterX) * scale;
        };
        const auto mapY = [centerY, scale, worldCenterZ](float value)
        {
            return centerY - (value - worldCenterZ) * scale;
        };

        pCtx->pfnUiDrawRect(originX, originY, originX + mapWidth, originY + mapHeight,
            0.45f, 0.48f, 0.54f, 0.8f, 1.0f);
        pCtx->pfnUiDrawLine(centerX, originY + MAP_MARGIN, centerX,
            originY + mapHeight - MAP_MARGIN, 0.45f, 0.48f, 0.54f, 0.35f, 1.0f);
        pCtx->pfnUiDrawLine(originX + MAP_MARGIN, centerY, originX + mapWidth - MAP_MARGIN,
            centerY, 0.45f, 0.48f, 0.54f, 0.35f, 1.0f);
        pCtx->pfnUiDrawText(originX + 5.0f, originY + 4.0f, "Top view XZ",
            0.72f, 0.74f, 0.78f, 1.0f);

        for (int entityIndex = 0; entityIndex < static_cast<int>(pCtx->pScene->m_vEntities.size());
            ++entityIndex)
        {
            const CEntity& entity = pCtx->pScene->m_vEntities[entityIndex];
            const SEntityDebugData data = ReadEntityDebugData(entity);
            const float pointX = mapX(data.x);
            const float pointY = mapY(data.z);
            const float meshX = mapX(data.x + data.meshOffsetX);
            const float meshY = mapY(data.z + data.meshOffsetZ);
            const float colliderX = mapX(data.x + data.colliderOffsetX);
            const float colliderY = mapY(data.z + data.colliderOffsetZ);
            const bool isSelected = pCtx->pSelected && *pCtx->pSelected == entityIndex;

            if (s_State.showMeshBounds && data.hasMesh)
            {
                const float halfX = std::max(2.0f, data.meshHalfX * scale);
                const float halfZ = std::max(2.0f, data.meshHalfZ * scale);
                pCtx->pfnUiDrawRect(meshX - halfX, meshY - halfZ, meshX + halfX,
                    meshY + halfZ, 0.25f, 0.7f, 1.0f, 0.75f, 1.0f);
            }

            if (s_State.showColliders && data.hasCollider)
            {
                if (data.colliderRound)
                {
                    pCtx->pfnUiDrawCircle(colliderX, colliderY,
                        std::max(2.0f, data.colliderRadius * scale),
                        1.0f, 0.65f, 0.2f, 0.95f, 1.5f);
                }
                else
                {
                    const float halfX = std::max(2.0f, data.colliderX * scale);
                    const float halfZ = std::max(2.0f, data.colliderZ * scale);
                    pCtx->pfnUiDrawRect(colliderX - halfX, colliderY - halfZ, colliderX + halfX,
                        colliderY + halfZ, 1.0f, 0.65f, 0.2f, 0.95f, 1.5f);
                }
            }

            if (s_State.showLights && data.hasLight)
            {
                pCtx->pfnUiDrawCircle(pointX, pointY,
                    std::clamp(data.lightRange * scale, 3.0f, mapWidth * 0.45f),
                    1.0f, 0.9f, 0.2f, 0.25f, 1.0f);
                pCtx->pfnUiDrawCircleFilled(pointX, pointY, 3.0f,
                    1.0f, 0.9f, 0.2f, 1.0f);
            }

            pCtx->pfnUiDrawCircleFilled(pointX, pointY, isSelected ? 4.0f : 2.5f,
                isSelected ? 1.0f : 0.85f,
                isSelected ? 0.35f : 0.85f,
                isSelected ? 0.35f : 0.85f, 1.0f);
        }
    }
    pCtx->pfnUiEndChild();
}

void DrawProfiler(SPluginContext* pCtx)
{
    if (s_State.frameSampleCount == 0)
    {
        pCtx->pfnUiText("Waiting for frame samples...");
        return;
    }

    float frameTotal = 0.0f;
    float framePeak = 0.0f;
    float pluginTotal = 0.0f;
    for (int sampleIndex = FRAME_SAMPLE_COUNT - s_State.frameSampleCount;
        sampleIndex < FRAME_SAMPLE_COUNT; ++sampleIndex)
    {
        frameTotal += s_State.aFrameTimes[sampleIndex];
        framePeak = std::max(framePeak, s_State.aFrameTimes[sampleIndex]);
        pluginTotal += s_State.aPluginTimes[sampleIndex];
    }

    const float averageFrameTime = frameTotal / static_cast<float>(s_State.frameSampleCount);
    const float averagePluginTime = pluginTotal / static_cast<float>(s_State.frameSampleCount);
    const float fps = averageFrameTime > 0.0f ? 1000.0f / averageFrameTime : 0.0f;
    char summary[192];
    std::snprintf(summary, sizeof(summary),
        "Frame: %.2f ms avg, %.2f ms peak (%.1f FPS) | Plugin update: %.3f ms avg",
        averageFrameTime, framePeak, fps, averagePluginTime);
    pCtx->pfnUiText(summary);

    if (pCtx->pfnUiPlotLinesEx)
    {
        pCtx->pfnUiPlotLinesEx("Frame time (ms)", s_State.aFrameTimes.data(),
            FRAME_SAMPLE_COUNT, 0, nullptr, 0.0f, 50.0f, 0.0f, 70.0f);
        pCtx->pfnUiPlotLinesEx("Plugin update (ms)", s_State.aPluginTimes.data(),
            FRAME_SAMPLE_COUNT, 0, nullptr, 0.0f, 2.0f, 0.0f, 55.0f);
    }
    else if (pCtx->pfnUiPlotLines)
    {
        pCtx->pfnUiPlotLines("Frame time (ms)", s_State.aFrameTimes.data(),
            FRAME_SAMPLE_COUNT, 0.0f, 50.0f);
    }

    pCtx->pfnUiText("GPU timings are not exposed to plugins.");
    if (pCtx->pThreadUsages && pCtx->threadUsageCount > 0 && pCtx->pfnUiSeparator)
    {
        pCtx->pfnUiSeparator();
    }
    if (pCtx->pThreadUsages && pCtx->threadUsageCount > 0)
    {
        pCtx->pfnUiText("Worker threads");
        for (int threadIndex = 0; threadIndex < pCtx->threadUsageCount; ++threadIndex)
        {
            const SPluginThreadUsage& thread = pCtx->pThreadUsages[threadIndex];
            char label[160];
            std::snprintf(label, sizeof(label), "%s: %.1f%% | %s",
                thread.pName ? thread.pName : "Unnamed thread",
                thread.utilizationPercent,
                thread.pCurrentTask ? thread.pCurrentTask : "Idle");
            pCtx->pfnUiText(label);
        }
    }
}

void OnDrawUI(SPluginContext* pCtx)
{
    if (!pCtx || !pCtx->pfnUiBegin || !pCtx->pfnUiEnd ||
        !pCtx->pfnUiText || !pCtx->pfnUiCheckbox || !pCtx->pfnUiButton)
    {
        return;
    }

    const bool isOpen = pCtx->pfnUiBegin("Developer Tools");
    if (isOpen)
    {
        if (pCtx->pfnUiCollapsingHeader &&
            pCtx->pfnUiCollapsingHeader("CPU and thread profiler"))
        {
            DrawProfiler(pCtx);
        }

        if (pCtx->pfnUiCollapsingHeader &&
            pCtx->pfnUiCollapsingHeader("Scene debug map"))
        {
            pCtx->pfnUiCheckbox("Colliders", &s_State.showColliders);
            pCtx->pfnUiCheckbox("Lights", &s_State.showLights);
            pCtx->pfnUiCheckbox("Mesh bounds", &s_State.showMeshBounds);
            pCtx->pfnUiText("Schematic XZ view; rotation and hierarchy are not projected.");
            DrawSceneMap(pCtx);
        }

        if (pCtx->pfnUiCollapsingHeader &&
            pCtx->pfnUiCollapsingHeader("Logs and diagnostics"))
        {
            if (pCtx->pfnUiButton("Open debug log"))
            {
                s_State.showDebugLog = true;
            }
            if (pCtx->pfnUiButton("Open ImGui metrics"))
            {
                s_State.showMetrics = true;
            }
            pCtx->pfnUiText("This opens Dear ImGui's log; engine logs are not exposed to plugins.");
        }
    }
    pCtx->pfnUiEnd();

    if (s_State.showDebugLog && pCtx->pfnUiShowDebugLogWindow)
    {
        pCtx->pfnUiShowDebugLogWindow(&s_State.showDebugLog);
    }
    if (s_State.showMetrics && pCtx->pfnUiShowMetricsWindow)
    {
        pCtx->pfnUiShowMetricsWindow(&s_State.showMetrics);
    }
}

SPlugin s_Plugin
{
    "DeveloperTools", "1.0.0",
    OnLoad, OnUnload, OnUpdate, OnDrawUI
};

} // anonymous

PLUGIN_EXPORT SPlugin* GetPlugin()
{
    return &s_Plugin;
}
