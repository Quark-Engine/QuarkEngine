#include "application_scene_renderer.h"

#include "component.h"
#include "lighting.h"
#include "models.h"
#include "scene.h"
#include "tex.h"
#include "engine/cpu_task_pool.h"
#include "engine/transform.h"

#include <cfloat>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
namespace
{
    float ElapsedMilliseconds(const std::chrono::steady_clock::time_point& start)
    {
        return std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - start)
            .count();
    }

    void ExpandBoundsWithPoint(BoundingBox& bounds, const Vec3& point);

    void HashBytes(std::uint64_t& hash, const void* pData, std::size_t size)
    {
        const auto* pBytes = static_cast<const unsigned char*>(pData);
        for (std::size_t i = 0; i < size; ++i)
        {
            hash ^= pBytes[i];
            hash *= 1099511628211ull;
        }
    }

    template <typename T>
    void HashValue(std::uint64_t& hash, const T& value)
    {
        HashBytes(hash, &value, sizeof(value));
    }

    std::uint64_t ComputeSceneGeometrySignature(const CScene& scene,
                                                const std::vector<SSceneEntityFrameData>& vFrameData)
    {
        std::uint64_t hash = 1469598103934665603ull;
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            const CEntity& entity = scene.m_vEntities[entityIndex];
            const CMeshComponent* pMesh = entity.GetMeshComponent();
            const CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pMesh || !pTransform)
            {
                continue;
            }

            HashValue(hash, entityIndex);
            HashValue(hash, pMesh->m_Enabled);
            HashValue(hash, pMesh->m_BoundsDirty);
            HashValue(hash, pMesh->m_Model.meshCount);
            HashValue(hash, pMesh->m_Model.meshes);
            HashValue(hash, pMesh->m_Model.transform.m);
            if (entityIndex < static_cast<int>(vFrameData.size()))
            {
                HashValue(hash, vFrameData[entityIndex].m_WorldTransform.m);
            }
        }
        return hash;
    }

    std::uint64_t CombineHash(std::uint64_t hash, const void* pData, std::size_t size)
    {
        HashBytes(hash, pData, size);
        return hash;
    }

    BoundingBox GetWorldBounds(CMeshComponent* pMesh, const Mat4& worldTransform)
    {
        if (pMesh->m_BoundsDirty)
        {
            pMesh->m_CachedLocalBounds = GetModelBoundingBox(pMesh->m_Model);
            pMesh->m_BoundsDirty = false;
        }

        const BoundingBox local = pMesh->m_CachedLocalBounds;
        const Vec3 aCorners[8] = {
            { local.min.x, local.min.y, local.min.z }, { local.max.x, local.min.y, local.min.z }, { local.min.x, local.max.y, local.min.z }, { local.max.x, local.max.y, local.min.z }, { local.min.x, local.min.y, local.max.z }, { local.max.x, local.min.y, local.max.z }, { local.min.x, local.max.y, local.max.z }, { local.max.x, local.max.y, local.max.z }
        };

        BoundingBox world = { Vec3{ FLT_MAX, FLT_MAX, FLT_MAX }, Vec3{ -FLT_MAX, -FLT_MAX, -FLT_MAX } };
        for (const Vec3& corner : aCorners)
        {
            const Vec3 point = Vec3(worldTransform * corner);
            ExpandBoundsWithPoint(world, point);
        }
        return world;
    }

    bool IsBoundsVisible(const BoundingBox& bounds, const Camera3D& camera, float aspectRatio)
    {
        const Vec3 center = (bounds.min + bounds.max) * 0.5f;
        const Vec3 half = (bounds.max - bounds.min) * 0.5f;
        const float radius = half.length();
        const Vec3 forwardVector = camera.target - camera.position;
        if (forwardVector.length() <= 0.000001f || camera.up.length() <= 0.000001f)
        {
            return true;
        }

        const Vec3 forward = forwardVector.normalized();
        const Vec3 rightVector = forward.cross(camera.up);
        if (rightVector.length() <= 0.000001f)
        {
            return true;
        }

        const Vec3 right = rightVector.normalized();
        const Vec3 up = right.cross(forward).normalized();
        const Vec3 relative = center - camera.position;
        const float depth = relative.dot(forward);

        constexpr float NEAR_PLANE = 0.01f;
        constexpr float FAR_PLANE = 1000.0f;
        if (depth + radius < NEAR_PLANE || depth - radius > FAR_PLANE)
        {
            return false;
        }

        const float safeAspect = std::max(aspectRatio, 0.01f);
        if (camera.projection == CAMERA_ORTHOGRAPHIC)
        {
            const float halfHeight = std::max(camera.fovy * 0.5f, 0.01f);
            const float halfWidth = halfHeight * safeAspect;
            if (std::fabs(relative.dot(up)) > halfHeight + radius)
            {
                return false;
            }
            if (std::fabs(relative.dot(right)) > halfWidth + radius)
            {
                return false;
            }
            return true;
        }

        const float tanHalfFov = tanf(camera.fovy * DEG2RAD * 0.5f);
        const float tanHalfHorizontalFov = tanHalfFov * safeAspect;
        const float verticalPlaneRadius = radius * sqrtf(1.0f + tanHalfFov * tanHalfFov);
        const float horizontalPlaneRadius = radius * sqrtf(1.0f + tanHalfHorizontalFov * tanHalfHorizontalFov);

        if (std::fabs(relative.dot(up)) - depth * tanHalfFov > verticalPlaneRadius)
        {
            return false;
        }
        if (std::fabs(relative.dot(right)) - depth * tanHalfHorizontalFov > horizontalPlaneRadius)
        {
            return false;
        }
        return true;
    }

    void ExpandBoundsWithPoint(BoundingBox& bounds, const Vec3& point)
    {
        bounds.min.x = fminf(bounds.min.x, point.x);
        bounds.min.y = fminf(bounds.min.y, point.y);
        bounds.min.z = fminf(bounds.min.z, point.z);
        bounds.max.x = fmaxf(bounds.max.x, point.x);
        bounds.max.y = fmaxf(bounds.max.y, point.y);
        bounds.max.z = fmaxf(bounds.max.z, point.z);
    }

    BoundingBox ComputeSceneBounds(const CScene& scene,
                                   const std::vector<SSceneEntityFrameData>& vFrameData)
    {
        BoundingBox bounds = {
            { FLT_MAX, FLT_MAX, FLT_MAX },
            { -FLT_MAX, -FLT_MAX, -FLT_MAX }
        };
        bool hasBounds = false;

        for (const auto& entity : scene.m_vEntities)
        {
            const CMeshComponent* pMesh = entity.GetMeshComponent();
            if (!pMesh || !pMesh->m_Enabled || pMesh->m_Model.meshCount <= 0 || !pMesh->m_Model.meshes)
            {
                continue;
            }

            const int entityIndex = static_cast<int>(&entity - scene.m_vEntities.data());
            if (entityIndex >= static_cast<int>(vFrameData.size()) || !vFrameData[entityIndex].m_HasMesh)
            {
                continue;
            }

            const BoundingBox& worldBounds = vFrameData[entityIndex].m_WorldBounds;
            ExpandBoundsWithPoint(bounds, worldBounds.min);
            ExpandBoundsWithPoint(bounds, worldBounds.max);
            hasBounds = true;
        }

        if (!hasBounds)
        {
            bounds.min = { -5.0f, -5.0f, -5.0f };
            bounds.max = { 5.0f, 5.0f, 5.0f };
        }

        return bounds;
    }

    void SetModelShader(Model& model, Shader& shader)
    {
        for (int i = 0; i < model.materialCount; i++)
        {
            model.materials[i].shader = &shader;
        }
    }

    void SetShaderLightEnabled(Shader shader, int location, bool enabled)
    {
        int enabledValue = enabled ? 1 : 0;
        SetShaderValue(shader, location, &enabledValue, SHADER_UNIFORM_INT);
    }

    void DisableAllShaderLights(Shader shader, const std::array<int, QC_MAX_LIGHTS>& aLocations)
    {
        for (int slot = 0; slot < QC_MAX_LIGHTS; slot++)
        {
            SetShaderLightEnabled(shader, aLocations[slot], false);
        }
    }

    bool PrepareSceneLightUniforms(CScene& scene, CLightRegistry& lights, Shader shader,
                                   const std::array<int, QC_MAX_LIGHTS>& aLightEnabledLocations, const Vec3& sceneCenter,
                                   const std::vector<SSceneEntityFrameData>& vFrameData)
    {
        DisableAllShaderLights(shader, aLightEnabledLocations);

        bool hasActiveSceneLight = false;
        for (int entityIndex = 0; entityIndex < static_cast<int>(scene.m_vEntities.size()); ++entityIndex)
        {
            CEntity& e = scene.m_vEntities[entityIndex];
            CLightComponent* pLight = e.GetLightComponent();
            CTransformComponent* pTransform = e.GetTransformComponent();
            if (!pLight || !pTransform)
            {
                continue;
            }

            if (!pLight->m_Enabled)
            {
                if (pLight->m_Created)
                {
                    lights.Free(pLight->m_Light.m_Id);
                    pLight->m_Created = false;
                    pLight->m_Light.m_Id = -1;
                }
                pLight->m_Light.m_Enabled = false;
                continue;
            }

            pLight->m_Light.m_Enabled = true;
            const Mat4& worldTransform = vFrameData[entityIndex].m_WorldTransform;
            const Vec3 worldPosition = Vec3(worldTransform * Vec3{ 0.0f, 0.0f, 0.0f });

            if (!pLight->m_Created)
            {
                int newId = lights.Allocate();
                if (newId != -1)
                {
                    pLight->m_Light.m_Id = newId;
                    pLight->m_Light.m_Light = CreateLightAtSlot(newId, pLight->m_Light.m_Light.type,
                                                                worldPosition, pLight->m_Light.m_Target, pLight->m_Light.m_Color, shader);
                    InitializeLightingUniformCache(pLight->m_Light, shader, newId);
                    pLight->m_Created = true;
                }
            }

            if (!pLight->m_Created || pLight->m_Light.m_Id == -1)
            {
                continue;
            }

            pLight->m_Light.m_Position = worldPosition;

            if (pLight->m_Light.m_Light.type == LIGHT_TYPE_DIRECTIONAL &&
                (pLight->m_Light.m_Position - pLight->m_Light.m_Target).length() <= 0.000001f)
            {
                pLight->m_Light.m_Target = sceneCenter;
            }

            pLight->m_Light.m_Light.position = pLight->m_Light.m_Position;
            pLight->m_Light.m_Light.target = pLight->m_Light.m_Target;
            pLight->m_Light.m_Light.color = pLight->m_Light.m_Color;
            UpdateLighting(shader, pLight->m_Light);
            hasActiveSceneLight = true;
        }

        return hasActiveSceneLight;
    }

    void RenderSceneShadowMaps(CScene& scene, Shader shadowShader,
                               std::array<RenderTexture2D, QC_MAX_LIGHTS>& aShadowMaps,
                               std::array<Camera3D, QC_MAX_LIGHTS>& aShadowCameras,
                               const Vec3& sceneCenter,
                               Shader lightingShader,
                               std::array<Mat4, QC_MAX_LIGHTS>& aCachedLightViews,
                               std::array<Mat4, QC_MAX_LIGHTS>& aCachedLightProjections,
                               std::array<int, QC_MAX_LIGHTS>& aLightViewLocations,
                               std::array<int, QC_MAX_LIGHTS>& aLightProjectionLocations,
                               const std::array<bool, QC_MAX_LIGHTS>& aRenderSlots,
                               const std::vector<SSceneEntityFrameData>& vFrameData)
    {
        std::array<bool, QC_MAX_LIGHTS> aRendered = {};
        Material shadowMaterial = {};
        shadowMaterial.shader = &shadowShader;

        for (auto& entity : scene.m_vEntities)
        {
            CLightComponent* pLight = entity.GetLightComponent();
            CTransformComponent* pTransform = entity.GetTransformComponent();
            if (!pLight || !pTransform || !pLight->m_Enabled || !pLight->m_Created ||
                pLight->m_Light.m_Id < 0 || pLight->m_Light.m_Id >= QC_MAX_LIGHTS ||
                aRendered[pLight->m_Light.m_Id])
            {
                continue;
            }

            const int slot = pLight->m_Light.m_Id;
            if (!aRenderSlots[slot])
            {
                continue;
            }
            const int lightEntityIndex = static_cast<int>(&entity - scene.m_vEntities.data());
            const Mat4& lightTransform = vFrameData[lightEntityIndex].m_WorldTransform;
            aShadowCameras[slot].position = Vec3(lightTransform * Vec3{ 0.0f, 0.0f, 0.0f });
            aShadowCameras[slot].target = pLight->m_Light.m_Target;
            if ((aShadowCameras[slot].position - aShadowCameras[slot].target).length() <= 0.000001f)
            {
                aShadowCameras[slot].target = sceneCenter;
                if ((aShadowCameras[slot].position - aShadowCameras[slot].target).length() <= 0.000001f)
                {
                    aShadowCameras[slot].target = aShadowCameras[slot].position + Vec3{ 0.0f, -1.0f, 0.0f };
                }
            }
            aRendered[slot] = true;

            BeginTextureMode(aShadowMaps[slot]);
            ClearBackground(WHITE);
            BeginMode3D(aShadowCameras[slot]);
            BeginShaderMode(shadowShader);

            Mat4 lightView;
            Mat4 lightProjection;
            for (int sourceIndex = 0; sourceIndex < static_cast<int>(scene.m_vEntities.size()); ++sourceIndex)
            {
                auto& source = scene.m_vEntities[sourceIndex];
                CMeshComponent* pMesh = source.GetMeshComponent();
                CTransformComponent* pSourceTransform = source.GetTransformComponent();
                if (!pMesh || !pMesh->m_Enabled || !pSourceTransform ||
                    pMesh->m_Model.meshCount <= 0 || !pMesh->m_Model.meshes)
                {
                    continue;
                }

                const Mat4& entityTransform = vFrameData[sourceIndex].m_MeshWorldTransform;
                if (!IsBoundsVisible(vFrameData[sourceIndex].m_WorldBounds, aShadowCameras[slot], 1.0f))
                {
                    continue;
                }
                const bool mirroredTransform = quark::HasNegativeDeterminant(entityTransform);
                if (mirroredTransform)
                {
                    DisableBackfaceCulling();
                }
                for (int meshIndex = 0; meshIndex < pMesh->m_Model.meshCount; ++meshIndex)
                {
                    DrawMesh(pMesh->m_Model.meshes[meshIndex], shadowMaterial, entityTransform);
                }
                if (mirroredTransform)
                {
                    EnableBackfaceCulling();
                }
            }

            for (int i = 0; i < 16; ++i)
            {
                lightView.m[i] = GetMatrixModelview()[i];
                lightProjection.m[i] = GetMatrixProjection()[i];
            }
            aCachedLightViews[slot] = lightView;
            aCachedLightProjections[slot] = lightProjection;
            SetShaderValueMatrix(lightingShader, aLightViewLocations[slot], lightView.m);
            SetShaderValueMatrix(lightingShader, aLightProjectionLocations[slot], lightProjection.m);

            EndShaderMode();
            EndMode3D();
            EndTextureMode();
        }
    }

    void AssignShadowMaps(CScene& scene,
                          const std::array<RenderTexture2D, QC_MAX_LIGHTS>& aShadowMaps,
                          Shader lightingShader)
    {
        for (auto& entity : scene.m_vEntities)
        {
            CMeshComponent* pMesh = entity.GetMeshComponent();
            if (!pMesh || !pMesh->m_Model.materials)
            {
                continue;
            }

            SetModelShader(pMesh->m_Model, lightingShader);
            for (int materialIndex = 0; materialIndex < pMesh->m_Model.materialCount; ++materialIndex)
            {
                Material& material = pMesh->m_Model.materials[materialIndex];
                if (!material.maps)
                {
                    continue;
                }
                for (int shadowIndex = 0; shadowIndex < QC_MAX_LIGHTS; ++shadowIndex)
                {
                    material.maps[MATERIAL_MAP_HEIGHT + shadowIndex].texture = aShadowMaps[shadowIndex].texture;
                }
            }
            pMesh->m_ShaderAssigned = true;
        }
    }
} // anonymous

void CSceneRenderer::Initialize(int shadowMapSize, bool shadowsEnabled,
                                float shadowBias, int shadowFilterQuality,
                                const std::filesystem::path& shaderDirectory)
{
    const std::filesystem::path shaderRoot = shaderDirectory.empty()
        ? std::filesystem::path("assets/shader")
        : shaderDirectory;
    m_VulkanBackend = GetCurrentBackend() == RendererType::Vulkan;
    const bool d3d11Backend = GetCurrentBackend() == RendererType::D3D11;
    const std::string lightingVertex = m_VulkanBackend
        ? "lighting_vulkan.vs"
        : (d3d11Backend ? "lighting_d3d11.vs" : "lighting.vs");
    const std::string lightingFragment = m_VulkanBackend
        ? "lighting_vulkan.fs"
        : (d3d11Backend ? "lighting_d3d11.fs" : "lighting.fs");
    const std::string shadowVertex = m_VulkanBackend
        ? "shadow_depth_vulkan.vs"
        : (d3d11Backend ? "shadow_depth_d3d11.vs" : "shadow_depth.vs");
    const std::string shadowFragment = m_VulkanBackend
        ? "shadow_depth_vulkan.fs"
        : (d3d11Backend ? "shadow_depth_d3d11.fs" : "shadow_depth.fs");
    m_LightingShader = LoadShader((shaderRoot / lightingVertex).string().c_str(),
                                  (shaderRoot / lightingFragment).string().c_str());
    m_ShadowShader = LoadShader((shaderRoot / shadowVertex).string().c_str(),
                                (shaderRoot / shadowFragment).string().c_str());
    m_LightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(m_LightingShader, "viewPos");
    m_GlobalParamsLoc = m_VulkanBackend
        ? GetShaderLocation(m_LightingShader, "lights[3].type")
        : -1;
    m_ShadowsEnabledLoc = GetShaderLocation(m_LightingShader, "shadowsEnabled");
    m_ShadowBiasLoc = GetShaderLocation(m_LightingShader, "shadowBias");
    m_ShadowFilterLoc = GetShaderLocation(m_LightingShader, "shadowFilterQuality");
    const int shadowsEnabledValue = shadowsEnabled ? 1 : 0;
    SetShaderValue(m_LightingShader, m_ShadowsEnabledLoc, &shadowsEnabledValue, SHADER_UNIFORM_INT);
    SetShaderValue(m_LightingShader, m_ShadowBiasLoc, shadowBias);
    SetShaderValue(m_LightingShader, m_ShadowFilterLoc, shadowFilterQuality);

    for (int i = 0; i < QC_MAX_LIGHTS; ++i)
    {
        m_aShadowMaps[i] = LoadRenderTexture(shadowMapSize, shadowMapSize);
        m_aShadowCameras[i] = CreateCamera3D();
        m_aShadowCameras[i].fovy = 55.0f;
        m_aCachedLightViews[i] = Mat4::identity();
        m_aCachedLightProjections[i] = Mat4::identity();
        m_aLightEnabledLocations[i] = GetShaderLocation(m_LightingShader,
                                                        TextFormat("lights[%i].enabled", i));
        m_aLightViewLocations[i] = GetShaderLocation(m_LightingShader, TextFormat("lightViews[%i]", i));
        m_aLightProjectionLocations[i] = GetShaderLocation(m_LightingShader, TextFormat("lightProjections[%i]", i));
    }

    m_UseTexLoc = GetShaderLocation(m_LightingShader, "useTexture");
    m_AmbientLoc = GetShaderLocation(m_LightingShader, "ambient");
    m_EmissionColorLoc = GetShaderLocation(m_LightingShader, "emissionColor");
    m_EmissionPowerLoc = GetShaderLocation(m_LightingShader, "emissionPower");
    SetShaderValue(m_LightingShader, m_AmbientLoc, Vec4{ 0.025f, 0.025f, 0.025f, 1.0f });
    SetShaderValue(m_LightingShader, m_EmissionColorLoc, Vec3{ 0.0f, 0.0f, 0.0f });
    SetShaderValue(m_LightingShader, m_EmissionPowerLoc, 0.0f);
    m_aShadowSignatures.fill(0);
    m_ShadowCacheValid = false;
    m_ShadowBindingsAssigned = false;
}

bool CSceneRenderer::IsMeshVisible(CMeshComponent* pMesh, const Mat4& worldTransform,
                                   const Camera3D& camera, float aspectRatio) const
{
    if (!pMesh || !pMesh->m_Enabled || pMesh->m_Model.meshCount <= 0 || !pMesh->m_Model.meshes)
    {
        return false;
    }
    return IsBoundsVisible(GetWorldBounds(pMesh, worldTransform), camera, aspectRatio);
}

void CSceneRenderer::PrepareFrameCache(CScene& scene)
{
    const int entityCount = static_cast<int>(scene.m_vEntities.size());
    std::atomic<bool> anyChanged{ false };
    if (static_cast<int>(m_vFrameData.size()) != entityCount)
    {
        m_vFrameData.assign(static_cast<std::size_t>(entityCount), SSceneEntityFrameData{});
        anyChanged.store(true, std::memory_order_relaxed);
    }

    const auto prepareRange = [&](int beginIndex, int endIndex)
    {
        bool localChanged = false;
        for (int entityIndex = beginIndex; entityIndex < endIndex; ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            SSceneEntityFrameData& frameData = m_vFrameData[entityIndex];
            const Mat4 newWorldTransform = quark::ComposeWorld(scene, entityIndex);
            if (std::memcmp(frameData.m_WorldTransform.m, newWorldTransform.m, sizeof(newWorldTransform.m)) != 0)
            {
                frameData.m_WorldTransform = newWorldTransform;
                localChanged = true;
            }

            CMeshComponent* pMesh = entity.GetMeshComponent();
            const bool hasMesh = pMesh && pMesh->m_Enabled && pMesh->m_Model.meshCount > 0 && pMesh->m_Model.meshes != nullptr;
            if (!hasMesh)
            {
                if (frameData.m_HasMesh)
                {
                    frameData.m_HasMesh = false;
                    localChanged = true;
                }
                continue;
            }

            const Mat4 newMeshWorldTransform = newWorldTransform * pMesh->m_Model.transform;
            const bool meshTransformChanged =
                !frameData.m_HasMesh ||
                pMesh->m_BoundsDirty ||
                std::memcmp(frameData.m_MeshWorldTransform.m, newMeshWorldTransform.m, sizeof(newMeshWorldTransform.m)) != 0;
            if (meshTransformChanged)
            {
                frameData.m_HasMesh = true;
                frameData.m_MeshWorldTransform = newMeshWorldTransform;
                frameData.m_WorldBounds = GetWorldBounds(pMesh, newMeshWorldTransform);
                localChanged = true;
            }
        }
        if (localChanged)
        {
            anyChanged.store(true, std::memory_order_relaxed);
        }
    };

    const int workerCount = static_cast<int>(CTaskPool::DefaultWorkerCount());
    if (m_pTaskPool != nullptr && entityCount >= 32 && workerCount > 1)
    {
        const int chunkCount = std::min(workerCount, (entityCount + 15) / 16);
        if (chunkCount > 1)
        {
            std::vector<std::future<void>> vFutures;
            vFutures.reserve(static_cast<std::size_t>(chunkCount - 1));
            for (int c = 1; c < chunkCount; ++c)
            {
                const int beginIdx = (c * entityCount) / chunkCount;
                const int endIdx = ((c + 1) * entityCount) / chunkCount;
                vFutures.push_back(m_pTaskPool->Submit("Prepare scene frame cache",
                                                       [&, beginIdx, endIdx]()
                                                       {
                                                           prepareRange(beginIdx, endIdx);
                                                       }));
            }

            prepareRange(0, entityCount / chunkCount);

            for (std::future<void>& future : vFutures)
            {
                future.get();
            }
            if (anyChanged.load(std::memory_order_relaxed))
            {
                scene.BumpTransformRevision();
            }
            return;
        }
    }

    prepareRange(0, entityCount);
    if (anyChanged.load(std::memory_order_relaxed))
    {
        scene.BumpTransformRevision();
    }
}

bool CSceneRenderer::IsMeshVisible(int entityIndex, const Camera3D& camera, float aspectRatio) const
{
    if (entityIndex < 0 || entityIndex >= static_cast<int>(m_vFrameData.size()) ||
        !m_vFrameData[entityIndex].m_HasMesh)
    {
        return false;
    }
    return IsBoundsVisible(m_vFrameData[entityIndex].m_WorldBounds, camera, aspectRatio);
}

const Mat4& CSceneRenderer::GetCachedWorldTransform(int entityIndex) const
{
    static const Mat4 s_Identity = Mat4::identity();
    if (entityIndex < 0 || entityIndex >= static_cast<int>(m_vFrameData.size()))
    {
        return s_Identity;
    }
    return m_vFrameData[entityIndex].m_WorldTransform;
}

const Mat4& CSceneRenderer::GetCachedMeshWorldTransform(int entityIndex) const
{
    static const Mat4 s_Identity = Mat4::identity();
    if (entityIndex < 0 || entityIndex >= static_cast<int>(m_vFrameData.size()) ||
        !m_vFrameData[entityIndex].m_HasMesh)
    {
        return s_Identity;
    }
    return m_vFrameData[entityIndex].m_MeshWorldTransform;
}

void CSceneRenderer::Update(CScene& scene, CLightRegistry& lights, const Vec3& cameraPosition,
                            bool shadowsEnabled, float shadowBias, int shadowFilterQuality)
{
    const auto updateStart = std::chrono::steady_clock::now();
    m_Timings = {};
    const std::uint64_t currentTransformRevision = scene.GetTransformRevision();
    std::uint64_t geometrySignature = m_LastGeometrySignature;
    BoundingBox sceneBounds = m_LastSceneBounds;
    if (!m_ShadowCacheValid ||
        currentTransformRevision != m_LastTransformRevision ||
        m_LastGeometrySignature == 0)
    {
        geometrySignature = ComputeSceneGeometrySignature(scene, m_vFrameData);
        sceneBounds = ComputeSceneBounds(scene, m_vFrameData);
        m_LastTransformRevision = currentTransformRevision;
        m_LastGeometrySignature = geometrySignature;
        m_LastSceneBounds = sceneBounds;
    }
    const Vec3 sceneCenter = {
        (sceneBounds.min.x + sceneBounds.max.x) * 0.5f,
        (sceneBounds.min.y + sceneBounds.max.y) * 0.5f,
        (sceneBounds.min.z + sceneBounds.max.z) * 0.5f
    };

    SetShaderValue(m_LightingShader, m_LightingShader.locs[SHADER_LOC_VECTOR_VIEW], &cameraPosition, SHADER_UNIFORM_VEC3);
    const int runtimeShadowsEnabled = shadowsEnabled ? 1 : 0;
    SetShaderValue(m_LightingShader, m_ShadowsEnabledLoc, &runtimeShadowsEnabled, SHADER_UNIFORM_INT);
    SetShaderValue(m_LightingShader, m_ShadowBiasLoc, shadowBias);
    SetShaderValue(m_LightingShader, m_ShadowFilterLoc, shadowFilterQuality);
    PrepareSceneLightUniforms(scene, lights, m_LightingShader, m_aLightEnabledLocations, sceneCenter,
                              m_vFrameData);
    m_ActiveLightCount = 0;
    for (const CEntity& entity : scene.m_vEntities)
    {
        if (const CLightComponent* pLight = entity.GetLightComponent(); pLight && pLight->m_Enabled)
        {
            ++m_ActiveLightCount;
        }
    }
    if (m_VulkanBackend)
    {
        WriteVulkanGlobals(scene, shadowsEnabled, shadowBias, shadowFilterQuality);
    }
    if (shadowsEnabled)
    {
        const auto shadowStart = std::chrono::steady_clock::now();
        std::array<bool, QC_MAX_LIGHTS> aRenderSlots = {};
        std::array<std::uint64_t, QC_MAX_LIGHTS> aNextSignatures = {};
        for (const auto& entity : scene.m_vEntities)
        {
            const CLightComponent* pLight = entity.GetLightComponent();
            if (!pLight || !pLight->m_Enabled || !pLight->m_Created ||
                pLight->m_Light.m_Id < 0 || pLight->m_Light.m_Id >= QC_MAX_LIGHTS)
            {
                continue;
            }

            const int slot = pLight->m_Light.m_Id;
            std::uint64_t signature = geometrySignature;
            signature = CombineHash(signature, &pLight->m_Light.m_Light.type,
                                    sizeof(pLight->m_Light.m_Light.type));
            signature = CombineHash(signature, &pLight->m_Light.m_Position, sizeof(Vec3));
            signature = CombineHash(signature, &pLight->m_Light.m_Target, sizeof(Vec3));
            aNextSignatures[slot] = signature;
        }

        for (int slot = 0; slot < QC_MAX_LIGHTS; ++slot)
        {
            aRenderSlots[slot] = aNextSignatures[slot] != 0 &&
                (!m_ShadowCacheValid || aNextSignatures[slot] != m_aShadowSignatures[slot]);
        }

        RenderSceneShadowMaps(scene, m_ShadowShader, m_aShadowMaps, m_aShadowCameras, sceneCenter,
                              m_LightingShader, m_aCachedLightViews, m_aCachedLightProjections,
                              m_aLightViewLocations,
                              m_aLightProjectionLocations, aRenderSlots,
                              m_vFrameData);
        m_Timings.m_ShadowPassMs = ElapsedMilliseconds(shadowStart);
        m_aShadowSignatures = aNextSignatures;
        m_ShadowCacheValid = true;
    }
    else
    {
        m_ShadowCacheValid = false;
    }

    if (shadowsEnabled)
    {
        for (int slot = 0; slot < QC_MAX_LIGHTS; ++slot)
        {
            if (m_aShadowSignatures[slot] == 0)
            {
                continue;
            }
            SetShaderValueMatrix(m_LightingShader, m_aLightViewLocations[slot],
                                 m_aCachedLightViews[slot].m);
            SetShaderValueMatrix(m_LightingShader, m_aLightProjectionLocations[slot],
                                 m_aCachedLightProjections[slot].m);
        }
    }

    if (!m_ShadowBindingsAssigned)
    {
        AssignShadowMaps(scene, m_aShadowMaps, m_LightingShader);
        m_ShadowBindingsAssigned = true;
    }
    m_Timings.m_UpdateMs = ElapsedMilliseconds(updateStart);
}

void CSceneRenderer::WriteVulkanGlobals(CScene& scene, bool shadowsEnabled,
                                        float shadowBias, int shadowFilterQuality)
{
    if (m_GlobalParamsLoc < 0)
    {
        return;
    }

    int lightType = 0;
    for (const auto& entity : scene.m_vEntities)
    {
        const CLightComponent* pLight = entity.GetLightComponent();
        if (pLight && pLight->m_Created && pLight->m_Light.m_Id == 3)
        {
            lightType = pLight->m_Light.m_Light.type;
            break;
        }
    }

    std::uint32_t globals = 0;
    globals |= shadowsEnabled ? 1u : 0u;
    globals |= (static_cast<std::uint32_t>(std::clamp(shadowFilterQuality, 0, 3)) & 3u) << 1;
    const std::uint32_t bias = static_cast<std::uint32_t>(
                                   std::lround(std::max(shadowBias, 0.0f) * 100000.0f)) &
        0xFFFFu;
    globals |= bias << 3;
    globals |= 1u << 20;

    const int packed = static_cast<int>(
        (globals << 2) | (static_cast<std::uint32_t>(lightType) & 3u));
    SetShaderValue(m_LightingShader, m_GlobalParamsLoc, &packed, SHADER_UNIFORM_INT);
}

void CSceneRenderer::BeginLightingPass()
{
    if (m_VulkanBackend && m_LightingShader.id != 0)
    {
        BeginShaderMode(m_LightingShader);
    }
}

void CSceneRenderer::EndLightingPass()
{
    if (m_VulkanBackend)
    {
        EndShaderMode();
    }
}

void CSceneRenderer::EnsureLightingShader(CMeshComponent* pMesh)
{
    if (!pMesh)
    {
        return;
    }

    const bool hasMaterials = pMesh->m_Model.materialCount > 0 && pMesh->m_Model.materials != nullptr;
    const bool shaderMissing = hasMaterials &&
        (pMesh->m_Model.materials[0].shader == nullptr ||
         pMesh->m_Model.materials[0].shader->id != m_LightingShader.id);

    if (!pMesh->m_ShaderAssigned || shaderMissing)
    {
        SetModelShader(pMesh->m_Model, m_LightingShader);
        for (int materialIndex = 0; materialIndex < pMesh->m_Model.materialCount; ++materialIndex)
        {
            Material& material = pMesh->m_Model.materials[materialIndex];
            if (!material.maps)
            {
                continue;
            }
            for (int shadowIndex = 0; shadowIndex < QC_MAX_LIGHTS; ++shadowIndex)
            {
                material.maps[MATERIAL_MAP_HEIGHT + shadowIndex].texture = m_aShadowMaps[shadowIndex].texture;
            }
        }
    }
    pMesh->m_ShaderAssigned = true;
}

void CSceneRenderer::SetUseTexture(bool useTexture)
{
    if (m_VulkanBackend)
    {
        return;
    }
    const int use = useTexture ? 1 : 0;
    if (m_LastUseTexState == use)
    {
        return;
    }
    m_LastUseTexState = use;
    SetShaderValue(m_LightingShader, m_UseTexLoc, &use, SHADER_UNIFORM_INT);
}

void CSceneRenderer::RenderSceneMeshes(CScene& scene, const Camera3D& camera, float aspectRatio,
                                       const CPreferences* pPreferences, bool drawEditorOverlays)
{
    m_vRenderCommands.clear();
    m_LastBatchStats = {};

    const int entityCount = static_cast<int>(scene.m_vEntities.size());
    for (int entityIndex = 0; entityIndex < entityCount; ++entityIndex)
    {
        CEntity& entity = scene.m_vEntities[entityIndex];
        CMeshComponent* pMeshComp = entity.GetMeshComponent();
        CTransformComponent* pTransform = entity.GetTransformComponent();
        CMaterialComponent* pMatComp = entity.GetMaterialComponent();
        if (!pMeshComp || !pMeshComp->m_Enabled || !pTransform || !pMatComp)
        {
            continue;
        }
        if (pMeshComp->m_Model.meshCount <= 0 || !pMeshComp->m_Model.meshes ||
            pMeshComp->m_Model.materialCount <= 0 || !pMeshComp->m_Model.materials)
        {
            continue;
        }
        if (!IsMeshVisible(entityIndex, camera, aspectRatio))
        {
            continue;
        }

        CEntityTextureService::RefreshEntityRenderState(entity);
        EnsureLightingShader(pMeshComp);
        ++m_LastBatchStats.m_VisibleMeshCount;

        const Mat4& meshWorldTransform = GetCachedMeshWorldTransform(entityIndex);
        const bool hasOverride = CMeshOverrideService::Has(entity);
        const bool canInstance = !hasOverride && !pMeshComp->m_MeshTrianglesDetached;
        const bool disableBackfaceCulling = hasOverride ||
            pMeshComp->m_MeshTrianglesDetached ||
            quark::HasNegativeDeterminant(meshWorldTransform);

        float distanceSq = 0.0f;
        if (entityIndex < static_cast<int>(m_vFrameData.size()) && m_vFrameData[entityIndex].m_HasMesh)
        {
            const BoundingBox& bounds = m_vFrameData[entityIndex].m_WorldBounds;
            const Vec3 center = (bounds.min + bounds.max) * 0.5f;
            const Vec3 delta = center - camera.position;
            distanceSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        }

        const bool hasComponentTex = (pMatComp->m_Texture.id != 0);
        const Color tintColor = pMatComp->m_Color;

        for (int meshIndex = 0; meshIndex < pMeshComp->m_Model.meshCount; ++meshIndex)
        {
            const Mesh& mesh = pMeshComp->m_Model.meshes[meshIndex];
            if (mesh.vertexCount <= 0)
            {
                continue;
            }
            m_LastBatchStats.m_TriangleCount += mesh.triangleCount;

            int materialIndex = 0;
            if (pMeshComp->m_Model.meshMaterial &&
                pMeshComp->m_Model.meshMaterial[meshIndex] >= 0 &&
                pMeshComp->m_Model.meshMaterial[meshIndex] < pMeshComp->m_Model.materialCount)
            {
                materialIndex = pMeshComp->m_Model.meshMaterial[meshIndex];
            }

            const Material& material = pMeshComp->m_Model.materials[materialIndex];
            const std::uint32_t shaderId = material.shader ? material.shader->id : m_LightingShader.id;

            std::uint32_t textureId = 0;
            bool hasTexture = hasComponentTex;
            Color baseAlbedo = WHITE;
            if (material.maps)
            {
                baseAlbedo = material.maps[MATERIAL_MAP_ALBEDO].color;
                if (material.maps[MATERIAL_MAP_ALBEDO].texture.valid ||
                    material.maps[MATERIAL_MAP_ALBEDO].texture.id != 0)
                {
                    textureId = material.maps[MATERIAL_MAP_ALBEDO].texture.id;
                    hasTexture = true;
                }
            }
            if (textureId == 0 && hasComponentTex)
            {
                textureId = pMatComp->m_Texture.id;
            }

            const Color effectiveColor = {
                static_cast<unsigned char>((static_cast<int>(baseAlbedo.r) * static_cast<int>(tintColor.r)) / 255),
                static_cast<unsigned char>((static_cast<int>(baseAlbedo.g) * static_cast<int>(tintColor.g)) / 255),
                static_cast<unsigned char>((static_cast<int>(baseAlbedo.b) * static_cast<int>(tintColor.b)) / 255),
                static_cast<unsigned char>((static_cast<int>(baseAlbedo.a) * static_cast<int>(tintColor.a)) / 255)
            };

            std::uint64_t materialId = 1469598103934665603ull;
            HashValue(materialId, shaderId);
            HashValue(materialId, textureId);
            HashValue(materialId, effectiveColor);
            if (material.maps)
            {
                for (int mapIdx = MATERIAL_MAP_METALNESS; mapIdx <= MATERIAL_MAP_EMISSION; ++mapIdx)
                {
                    HashValue(materialId, material.maps[mapIdx].texture.id);
                    HashValue(materialId, material.maps[mapIdx].value);
                }
            }

            std::uint64_t meshId = 1469598103934665603ull;
            if (canInstance)
            {
                if (!pMeshComp->m_AssetName.empty())
                {
                    HashBytes(meshId, pMeshComp->m_AssetName.data(), pMeshComp->m_AssetName.size());
                }
                else
                {
                    const int objType = static_cast<int>(pMeshComp->m_Type);
                    HashValue(meshId, objType);
                    HashValue(meshId, pMeshComp->m_Segments);
                }
                HashValue(meshId, meshIndex);
                HashValue(meshId, mesh.vertexCount);
                HashValue(meshId, mesh.triangleCount);
            }
            else
            {
                HashValue(meshId, entityIndex);
                HashValue(meshId, meshIndex);
                HashValue(meshId, mesh.vaoId);
            }

            SRenderCommand cmd = {};
            cmd.m_EntityIndex = entityIndex;
            cmd.m_MeshIndex = meshIndex;
            cmd.m_ShaderId = shaderId;
            cmd.m_MaterialId = materialId;
            cmd.m_TextureId = textureId;
            cmd.m_MeshId = meshId;
            cmd.m_HasTexture = hasTexture;
            cmd.m_DisableBackfaceCulling = disableBackfaceCulling;
            cmd.m_IsTransparent = (effectiveColor.a < 255);
            cmd.m_CanInstance = canInstance;
            cmd.m_DistanceSq = distanceSq;
            cmd.m_EffectiveColor = effectiveColor;
            cmd.m_Transform = meshWorldTransform;
            cmd.m_pMesh = &mesh;
            cmd.m_Material = material;
            m_vRenderCommands.push_back(cmd);
        }
    }

    std::sort(m_vRenderCommands.begin(), m_vRenderCommands.end(),
              [](const SRenderCommand& a, const SRenderCommand& b)
              {
                  if (a.m_IsTransparent != b.m_IsTransparent)
                  {
                      return !a.m_IsTransparent;
                  }
                  if (a.m_IsTransparent)
                  {
                      if (a.m_DistanceSq != b.m_DistanceSq)
                      {
                          return a.m_DistanceSq > b.m_DistanceSq;
                      }
                  }
                  if (a.m_ShaderId != b.m_ShaderId)
                  {
                      return a.m_ShaderId < b.m_ShaderId;
                  }
                  if (a.m_MaterialId != b.m_MaterialId)
                  {
                      return a.m_MaterialId < b.m_MaterialId;
                  }
                  if (a.m_TextureId != b.m_TextureId)
                  {
                      return a.m_TextureId < b.m_TextureId;
                  }
                  if (a.m_MeshId != b.m_MeshId)
                  {
                      return a.m_MeshId < b.m_MeshId;
                  }
                  if (a.m_DisableBackfaceCulling != b.m_DisableBackfaceCulling)
                  {
                      return !a.m_DisableBackfaceCulling;
                  }
                  return a.m_DistanceSq < b.m_DistanceSq;
              });

    m_LastBatchStats.m_SubmittedCommands = static_cast<int>(m_vRenderCommands.size());
    m_LastUseTexState = -1;
    bool backfaceCullingDisabled = false;

    BeginLightingPass();

    const size_t commandCount = m_vRenderCommands.size();
    size_t index = 0;
    while (index < commandCount)
    {
        const SRenderCommand& first = m_vRenderCommands[index];

        if (first.m_DisableBackfaceCulling != backfaceCullingDisabled)
        {
            if (first.m_DisableBackfaceCulling)
            {
                DisableBackfaceCulling();
            }
            else
            {
                EnableBackfaceCulling();
            }
            backfaceCullingDisabled = first.m_DisableBackfaceCulling;
        }

        SetUseTexture(first.m_HasTexture);

        size_t batchEnd = index + 1;
        if (first.m_CanInstance && !first.m_IsTransparent)
        {
            while (batchEnd < commandCount)
            {
                const SRenderCommand& next = m_vRenderCommands[batchEnd];
                if (!next.m_CanInstance ||
                    next.m_IsTransparent ||
                    next.m_ShaderId != first.m_ShaderId ||
                    next.m_MaterialId != first.m_MaterialId ||
                    next.m_TextureId != first.m_TextureId ||
                    next.m_MeshId != first.m_MeshId ||
                    next.m_DisableBackfaceCulling != first.m_DisableBackfaceCulling ||
                    next.m_HasTexture != first.m_HasTexture)
                {
                    break;
                }
                ++batchEnd;
            }
        }

        const size_t batchSize = batchEnd - index;
        Material batchMaterial = first.m_Material;
        Color savedAlbedoColor = WHITE;
        const bool hasMaps = (batchMaterial.maps != nullptr);
        if (hasMaps)
        {
            savedAlbedoColor = batchMaterial.maps[MATERIAL_MAP_ALBEDO].color;
            batchMaterial.maps[MATERIAL_MAP_ALBEDO].color = first.m_EffectiveColor;
        }

        if (batchSize > 1)
        {
            m_vInstanceTransforms.clear();
            m_vInstanceTransforms.reserve(batchSize);
            for (size_t i = index; i < batchEnd; ++i)
            {
                m_vInstanceTransforms.push_back(m_vRenderCommands[i].m_Transform);
            }
            DrawMeshInstanced(*first.m_pMesh, batchMaterial, m_vInstanceTransforms.data(),
                              static_cast<int>(m_vInstanceTransforms.size()));
            ++m_LastBatchStats.m_InstancedBatches;
            m_LastBatchStats.m_InstancedCount += static_cast<int>(batchSize);
        }
        else
        {
            DrawMesh(*first.m_pMesh, batchMaterial, first.m_Transform);
        }

        if (hasMaps)
        {
            batchMaterial.maps[MATERIAL_MAP_ALBEDO].color = savedAlbedoColor;
        }

        ++m_LastBatchStats.m_DrawCalls;
        index = batchEnd;
    }

    if (backfaceCullingDisabled)
    {
        EnableBackfaceCulling();
    }

    EndLightingPass();

    if (drawEditorOverlays && pPreferences &&
        (pPreferences->m_WireframeEnabled || pPreferences->m_ShowColliders))
    {
        const Color wireframeColor = {
            static_cast<unsigned char>(pPreferences->m_WireframeRed),
            static_cast<unsigned char>(pPreferences->m_WireframeGreen),
            static_cast<unsigned char>(pPreferences->m_WireframeBlue),
            255
        };
        for (int entityIndex = 0; entityIndex < entityCount; ++entityIndex)
        {
            CEntity& entity = scene.m_vEntities[entityIndex];
            CMeshComponent* pMeshComp = entity.GetMeshComponent();
            CTransformComponent* pTransform = entity.GetTransformComponent();
            CMaterialComponent* pMatComp = entity.GetMaterialComponent();
            if (!pMeshComp || !pMeshComp->m_Enabled || !pTransform)
            {
                continue;
            }
            if (!IsMeshVisible(entityIndex, camera, aspectRatio))
            {
                continue;
            }

            const Mat4& worldTransform = GetCachedWorldTransform(entityIndex);
            if (pPreferences->m_WireframeEnabled && pMatComp && pMatComp->m_OutlineColor.a > 0)
            {
                PushMatrix();
                MultMatrix(worldTransform);
                DrawModelWires(pMeshComp->m_Model, { 0, 0, 0 }, 1.0f, wireframeColor);
                PopMatrix();
            }
            if (pPreferences->m_ShowColliders)
            {
                DrawCollisionDebug(entity, worldTransform, *pPreferences);
            }
        }
    }
}

void CSceneRenderer::Unload()
{
    UnloadShaders();
    UnloadShadowMaps();
}

void CSceneRenderer::UnloadShaders()
{
    if (m_LightingShader.id != 0)
    {
        UnloadShader(m_LightingShader);
        m_LightingShader = Shader{};
    }

    if (m_ShadowShader.id != 0)
    {
        UnloadShader(m_ShadowShader);
        m_ShadowShader = Shader{};
    }
}

void CSceneRenderer::UnloadShadowMaps()
{
    for (auto& shadowMap : m_aShadowMaps)
    {
        if (shadowMap.id != 0)
        {
            UnloadRenderTexture(shadowMap);
            shadowMap = RenderTexture2D{};
        }
    }
}
