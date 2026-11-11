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
        std::array<float, FRAME_SAMPLE_COUNT> m_aFrameTimes = {};
        std::array<float, FRAME_SAMPLE_COUNT> m_aPluginTimes = {};
        int m_FrameSampleCount = 0;
        bool m_ShowColliders = true;
        bool m_ShowLights = true;
        bool m_ShowMeshBounds = true;
        bool m_ShowMetrics = false;
        bool m_ShowDebugLog = false;
    };

    SDeveloperToolsState s_State;

    struct SEntityDebugData
    {
        float m_X = 0.0f;
        float m_Z = 0.0f;
        float m_ScaleX = 1.0f;
        float m_ScaleZ = 1.0f;
        float m_RotationY = 0.0f;
        float m_MeshOffsetX = 0.0f;
        float m_MeshOffsetZ = 0.0f;
        float m_MeshHalfX = 0.5f;
        float m_MeshHalfZ = 0.5f;
        float m_ColliderOffsetX = 0.0f;
        float m_ColliderOffsetZ = 0.0f;
        float m_ColliderX = 0.5f;
        float m_ColliderZ = 0.5f;
        float m_ColliderRadius = 0.5f;
        float m_LightRange = 5.0f;
        bool m_HasCollider = false;
        bool m_ColliderRound = false;
        bool m_HasLight = false;
        bool m_HasMesh = false;
    };

    void AddFrameSample(std::array<float, FRAME_SAMPLE_COUNT>& aSamples, float value)
    {
        std::move(aSamples.begin() + 1, aSamples.end(), aSamples.begin());
        aSamples.back() = value;
    }

    void OnLoad(SPluginContext*)
    {
        s_State = {};
        s_State.m_ShowColliders = true;
        s_State.m_ShowLights = true;
        s_State.m_ShowMeshBounds = true;
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
        const float frameTimeMs = std::max(0.0f, pCtx->m_DeltaTime * 1000.0f);
        AddFrameSample(s_State.m_aFrameTimes, frameTimeMs);

        const auto elapsed = std::chrono::steady_clock::now() - startTime;
        const float pluginTimeMs = std::chrono::duration<float, std::milli>(elapsed).count();
        AddFrameSample(s_State.m_aPluginTimes, pluginTimeMs);
        s_State.m_FrameSampleCount = std::min(s_State.m_FrameSampleCount + 1, FRAME_SAMPLE_COUNT);
    }

    template <typename T>
    std::shared_ptr<T> GetComponentOfType(const CComponentManager& components, EComponentType type)
    {
        for (const auto& pComponent : components.GetAllComponents())
        {
            if (pComponent && pComponent->GetType() == type)
            {
                return std::static_pointer_cast<T>(pComponent);
            }
        }

        return nullptr;
    }

    SEntityDebugData ReadEntityDebugData(const CEntity& entity)
    {
        SEntityDebugData data;
        if (!entity.m_pComponents)
        {
            return data;
        }

        const std::shared_ptr<CTransformComponent> pTransform =
            GetComponentOfType<CTransformComponent>(*entity.m_pComponents, COMPONENT_TRANSFORM);
        if (pTransform)
        {
            data.m_X = pTransform->m_Position.x;
            data.m_Z = pTransform->m_Position.z;
            data.m_ScaleX = std::abs(pTransform->m_Scale.x);
            data.m_ScaleZ = std::abs(pTransform->m_Scale.z);
            data.m_RotationY = pTransform->m_Rotation.y * 0.0174532925f;
        }

        const std::shared_ptr<CCollisionComponent> pCollider =
            GetComponentOfType<CCollisionComponent>(*entity.m_pComponents, COMPONENT_COLLISION);
        if (pCollider && pCollider->m_Enabled)
        {
            const float colliderCenterX = pCollider->m_Center.x * data.m_ScaleX;
            const float colliderCenterZ = pCollider->m_Center.z * data.m_ScaleZ;
            const float cosRotation = std::cos(data.m_RotationY);
            const float sinRotation = std::sin(data.m_RotationY);
            data.m_HasCollider = true;
            data.m_ColliderOffsetX = colliderCenterX * cosRotation - colliderCenterZ * sinRotation;
            data.m_ColliderOffsetZ = colliderCenterX * sinRotation + colliderCenterZ * cosRotation;
            data.m_ColliderX = std::abs(pCollider->m_Size.x * data.m_ScaleX) * 0.5f;
            data.m_ColliderZ = std::abs(pCollider->m_Size.z * data.m_ScaleZ) * 0.5f;
            data.m_ColliderRadius = std::abs(pCollider->m_Radius) *
                std::max(data.m_ScaleX, data.m_ScaleZ);
            if (pCollider->m_ColliderType == COLLIDER_SPHERE ||
                pCollider->m_ColliderType == COLLIDER_CAPSULE)
            {
                data.m_ColliderX = data.m_ColliderRadius;
                data.m_ColliderZ = data.m_ColliderRadius;
                data.m_ColliderRound = true;
            }
        }

        const std::shared_ptr<CLightComponent> pLight =
            GetComponentOfType<CLightComponent>(*entity.m_pComponents, COMPONENT_LIGHT);
        if (pLight && pLight->m_Enabled && pLight->m_Light.m_Enabled)
        {
            data.m_HasLight = true;
            data.m_LightRange = std::max(0.1f, pLight->m_Light.m_Range);
        }

        const std::shared_ptr<CMeshComponent> pMesh =
            GetComponentOfType<CMeshComponent>(*entity.m_pComponents, COMPONENT_MESH);
        data.m_HasMesh = pMesh && pMesh->m_Enabled && pMesh->m_Model.meshes &&
            pMesh->m_Model.meshCount > 0;
        if (data.m_HasMesh)
        {
            float minX = 0.0f;
            float maxX = 0.0f;
            float minZ = 0.0f;
            float maxZ = 0.0f;
            bool hasVertices = false;
            for (int meshIndex = 0; meshIndex < pMesh->m_Model.meshCount; ++meshIndex)
            {
                const Mesh& mesh = pMesh->m_Model.meshes[meshIndex];
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
                const float localCenterX = (maxX + minX) * 0.5f * data.m_ScaleX;
                const float localCenterZ = (maxZ + minZ) * 0.5f * data.m_ScaleZ;
                const float localHalfX = (maxX - minX) * 0.5f * data.m_ScaleX;
                const float localHalfZ = (maxZ - minZ) * 0.5f * data.m_ScaleZ;
                const float cosRotation = std::abs(std::cos(data.m_RotationY));
                const float sinRotation = std::abs(std::sin(data.m_RotationY));
                data.m_MeshOffsetX = localCenterX * std::cos(data.m_RotationY) -
                    localCenterZ * std::sin(data.m_RotationY);
                data.m_MeshOffsetZ = localCenterX * std::sin(data.m_RotationY) +
                    localCenterZ * std::cos(data.m_RotationY);
                data.m_MeshHalfX = cosRotation * localHalfX + sinRotation * localHalfZ;
                data.m_MeshHalfZ = sinRotation * localHalfX + cosRotation * localHalfZ;
            }
        }
        return data;
    }

    void DrawSceneMap(SPluginContext* pCtx)
    {
        if (!pCtx->m_pScene || !pCtx->pfnUiBeginChild || !pCtx->pfnUiEndChild ||
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

            for (const CEntity& entity : pCtx->m_pScene->m_vEntities)
            {
                const SEntityDebugData data = ReadEntityDebugData(entity);
                const float extentX = std::max(data.m_HasMesh ? data.m_MeshHalfX : 0.0f, data.m_ColliderX);
                const float extentZ = std::max(data.m_HasMesh ? data.m_MeshHalfZ : 0.0f, data.m_ColliderZ);
                const float meshCenterX = data.m_X + data.m_MeshOffsetX;
                const float meshCenterZ = data.m_Z + data.m_MeshOffsetZ;
                const float colliderCenterX = data.m_X + data.m_ColliderOffsetX;
                const float colliderCenterZ = data.m_Z + data.m_ColliderOffsetZ;
                worldMinX = std::min(worldMinX, std::min(meshCenterX - extentX, colliderCenterX - extentX));
                worldMaxX = std::max(worldMaxX, std::max(meshCenterX + extentX, colliderCenterX + extentX));
                worldMinZ = std::min(worldMinZ, std::min(meshCenterZ - extentZ, colliderCenterZ - extentZ));
                worldMaxZ = std::max(worldMaxZ, std::max(meshCenterZ + extentZ, colliderCenterZ + extentZ));
                if (data.m_HasLight)
                {
                    worldMinX = std::min(worldMinX, data.m_X - data.m_LightRange);
                    worldMaxX = std::max(worldMaxX, data.m_X + data.m_LightRange);
                    worldMinZ = std::min(worldMinZ, data.m_Z - data.m_LightRange);
                    worldMaxZ = std::max(worldMaxZ, data.m_Z + data.m_LightRange);
                }
            }

            const float worldWidth = std::max(1.0f, worldMaxX - worldMinX);
            const float worldHeight = std::max(1.0f, worldMaxZ - worldMinZ);
            const float scale = std::max(0.01f, std::min((mapWidth - MAP_MARGIN * 2.0f) / worldWidth, (mapHeight - MAP_MARGIN * 2.0f) / worldHeight));
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

            for (int entityIndex = 0; entityIndex < static_cast<int>(pCtx->m_pScene->m_vEntities.size());
                 ++entityIndex)
            {
                const CEntity& entity = pCtx->m_pScene->m_vEntities[entityIndex];
                const SEntityDebugData data = ReadEntityDebugData(entity);
                const float pointX = mapX(data.m_X);
                const float pointY = mapY(data.m_Z);
                const float meshX = mapX(data.m_X + data.m_MeshOffsetX);
                const float meshY = mapY(data.m_Z + data.m_MeshOffsetZ);
                const float colliderX = mapX(data.m_X + data.m_ColliderOffsetX);
                const float colliderY = mapY(data.m_Z + data.m_ColliderOffsetZ);
                const bool isSelected = pCtx->m_pSelected && *pCtx->m_pSelected == entityIndex;

                if (s_State.m_ShowMeshBounds && data.m_HasMesh)
                {
                    const float halfX = std::max(2.0f, data.m_MeshHalfX * scale);
                    const float halfZ = std::max(2.0f, data.m_MeshHalfZ * scale);
                    pCtx->pfnUiDrawRect(meshX - halfX, meshY - halfZ, meshX + halfX,
                                        meshY + halfZ, 0.25f, 0.7f, 1.0f, 0.75f, 1.0f);
                }

                if (s_State.m_ShowColliders && data.m_HasCollider)
                {
                    if (data.m_ColliderRound)
                    {
                        pCtx->pfnUiDrawCircle(colliderX, colliderY,
                                              std::max(2.0f, data.m_ColliderRadius * scale),
                                              1.0f, 0.65f, 0.2f, 0.95f, 1.5f);
                    }
                    else
                    {
                        const float halfX = std::max(2.0f, data.m_ColliderX * scale);
                        const float halfZ = std::max(2.0f, data.m_ColliderZ * scale);
                        pCtx->pfnUiDrawRect(colliderX - halfX, colliderY - halfZ, colliderX + halfX,
                                            colliderY + halfZ, 1.0f, 0.65f, 0.2f, 0.95f, 1.5f);
                    }
                }

                if (s_State.m_ShowLights && data.m_HasLight)
                {
                    pCtx->pfnUiDrawCircle(pointX, pointY,
                                          std::clamp(data.m_LightRange * scale, 3.0f, mapWidth * 0.45f),
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
        if (s_State.m_FrameSampleCount == 0)
        {
            pCtx->pfnUiText("Waiting for frame samples...");
            return;
        }

        float frameTotal = 0.0f;
        float framePeak = 0.0f;
        float pluginTotal = 0.0f;
        for (int sampleIndex = FRAME_SAMPLE_COUNT - s_State.m_FrameSampleCount;
             sampleIndex < FRAME_SAMPLE_COUNT; ++sampleIndex)
        {
            frameTotal += s_State.m_aFrameTimes[sampleIndex];
            framePeak = std::max(framePeak, s_State.m_aFrameTimes[sampleIndex]);
            pluginTotal += s_State.m_aPluginTimes[sampleIndex];
        }

        const float averageFrameTime = frameTotal / static_cast<float>(s_State.m_FrameSampleCount);
        const float averagePluginTime = pluginTotal / static_cast<float>(s_State.m_FrameSampleCount);
        const float fps = averageFrameTime > 0.0f ? 1000.0f / averageFrameTime : 0.0f;
        char aSummary[192];
        std::snprintf(aSummary, sizeof(aSummary),
                      "Frame: %.2f ms avg, %.2f ms peak (%.1f FPS) | Plugin update: %.3f ms avg",
                      averageFrameTime, framePeak, fps, averagePluginTime);
        pCtx->pfnUiText(aSummary);

        if (pCtx->pfnUiPlotLinesEx)
        {
            pCtx->pfnUiPlotLinesEx("Frame time (ms)", s_State.m_aFrameTimes.data(),
                                   FRAME_SAMPLE_COUNT, 0, nullptr, 0.0f, 50.0f, 0.0f, 70.0f);
            pCtx->pfnUiPlotLinesEx("Plugin update (ms)", s_State.m_aPluginTimes.data(),
                                   FRAME_SAMPLE_COUNT, 0, nullptr, 0.0f, 2.0f, 0.0f, 55.0f);
        }
        else if (pCtx->pfnUiPlotLines)
        {
            pCtx->pfnUiPlotLines("Frame time (ms)", s_State.m_aFrameTimes.data(),
                                 FRAME_SAMPLE_COUNT, 0.0f, 50.0f);
        }

        pCtx->pfnUiText("GPU timings are not exposed to plugins.");
        if (pCtx->m_pThreadUsages && pCtx->m_ThreadUsageCount > 0 && pCtx->pfnUiSeparator)
        {
            pCtx->pfnUiSeparator();
        }
        if (pCtx->m_pThreadUsages && pCtx->m_ThreadUsageCount > 0)
        {
            pCtx->pfnUiText("Worker threads");
            for (int threadIndex = 0; threadIndex < pCtx->m_ThreadUsageCount; ++threadIndex)
            {
                const SPluginThreadUsage& thread = pCtx->m_pThreadUsages[threadIndex];
                char aLabel[160];
                std::snprintf(aLabel, sizeof(aLabel), "%s: %.1f%% | %s",
                              thread.m_pName ? thread.m_pName : "Unnamed thread",
                              thread.m_UtilizationPercent,
                              thread.m_pCurrentTask ? thread.m_pCurrentTask : "Idle");
                pCtx->pfnUiText(aLabel);
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
                pCtx->pfnUiCheckbox("Colliders", &s_State.m_ShowColliders);
                pCtx->pfnUiCheckbox("Lights", &s_State.m_ShowLights);
                pCtx->pfnUiCheckbox("Mesh bounds", &s_State.m_ShowMeshBounds);
                pCtx->pfnUiText("Schematic XZ view; rotation and hierarchy are not projected.");
                DrawSceneMap(pCtx);
            }

            if (pCtx->pfnUiCollapsingHeader &&
                pCtx->pfnUiCollapsingHeader("Logs and diagnostics"))
            {
                if (pCtx->pfnUiButton("Open debug log"))
                {
                    s_State.m_ShowDebugLog = true;
                }
                if (pCtx->pfnUiButton("Open ImGui metrics"))
                {
                    s_State.m_ShowMetrics = true;
                }
                pCtx->pfnUiText("This opens Dear ImGui's log; engine logs are not exposed to plugins.");
            }
        }
        pCtx->pfnUiEnd();

        if (s_State.m_ShowDebugLog && pCtx->pfnUiShowDebugLogWindow)
        {
            pCtx->pfnUiShowDebugLogWindow(&s_State.m_ShowDebugLog);
        }
        if (s_State.m_ShowMetrics && pCtx->pfnUiShowMetricsWindow)
        {
            pCtx->pfnUiShowMetricsWindow(&s_State.m_ShowMetrics);
        }
    }

    SPlugin s_Plugin{
        "DeveloperTools", "1.0.0",
        OnLoad, OnUnload, OnUpdate, OnDrawUI
    };

} // anonymous

PLUGIN_EXPORT SPlugin* GetPlugin()
{
    return &s_Plugin;
}
