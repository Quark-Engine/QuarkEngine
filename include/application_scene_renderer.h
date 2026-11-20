#ifndef __APPLICATION_SCENE_RENDERER_H__
#define __APPLICATION_SCENE_RENDERER_H__

#include "QuarkCore/QuarkCore.hpp"
#include "QuarkCore/QuarkLights.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

class CScene;
class CLightRegistry;
class CMeshComponent;
class CPreferences;
class CTaskPool;

struct SSceneRendererTimings
{
    float m_UpdateMs = 0.0f;
    float m_ShadowPassMs = 0.0f;
};

struct SRenderBatchStats
{
    int m_SubmittedCommands = 0;
    int m_DrawCalls = 0;
    int m_InstancedBatches = 0;
    int m_InstancedCount = 0;
    int m_VisibleMeshCount = 0;
    int m_TriangleCount = 0;
    int m_ActiveLightCount = 0;
};

struct SRenderCommand
{
    int m_EntityIndex = -1;
    int m_MeshIndex = 0;
    std::uint32_t m_ShaderId = 0;
    std::uint64_t m_MaterialId = 0;
    std::uint32_t m_TextureId = 0;
    std::uint64_t m_MeshId = 0;
    bool m_HasTexture = false;
    bool m_DisableBackfaceCulling = false;
    bool m_IsTransparent = false;
    bool m_CanInstance = false;
    float m_DistanceSq = 0.0f;
    Color m_EffectiveColor = WHITE;
    Mat4 m_Transform = Mat4::identity();
    const Mesh* m_pMesh = nullptr;
    Material m_Material{};
};

struct SSceneEntityFrameData
{
    Mat4 m_WorldTransform{};
    Mat4 m_MeshWorldTransform{};
    BoundingBox m_WorldBounds{};
    bool m_HasMesh = false;
};

class CSceneRenderer
{
public:
    void Initialize(int shadowMapSize, bool shadowsEnabled,
                    float shadowBias, int shadowFilterQuality,
                    const std::filesystem::path& shaderDirectory = {});

    void Update(CScene& scene, CLightRegistry& lights, const Vec3& cameraPosition,
                bool shadowsEnabled, float shadowBias, int shadowFilterQuality);

    void PrepareFrameCache(CScene& scene);
    void SetTaskPool(CTaskPool& taskPool)
    {
        m_pTaskPool = &taskPool;
    }

    void EnsureLightingShader(CMeshComponent* pMesh);

    void RenderSceneMeshes(CScene& scene, const Camera3D& camera, float aspectRatio,
                           const CPreferences* pPreferences, bool drawEditorOverlays);

    bool IsMeshVisible(CMeshComponent* pMesh, const Mat4& worldTransform,
                       const Camera3D& camera, float aspectRatio) const;
    bool IsMeshVisible(int entityIndex, const Camera3D& camera, float aspectRatio) const;

    const Mat4& GetCachedWorldTransform(int entityIndex) const;
    const Mat4& GetCachedMeshWorldTransform(int entityIndex) const;

    void SetUseTexture(bool useTexture);

    void BeginLightingPass();
    void EndLightingPass();

    Shader GetLightingShader() const
    {
        return m_LightingShader;
    }
    SSceneRendererTimings GetTimings() const
    {
        return m_Timings;
    }
    SRenderBatchStats GetLastBatchStats() const
    {
        return m_LastBatchStats;
    }
    int GetActiveLightCount() const
    {
        return m_ActiveLightCount;
    }

    void Unload();

private:
    void UnloadShaders();
    void UnloadShadowMaps();
    void WriteVulkanGlobals(CScene& scene, bool shadowsEnabled,
                            float shadowBias, int shadowFilterQuality);

    Shader m_LightingShader{};
    Shader m_ShadowShader{};

    bool m_VulkanBackend = false;
    int m_GlobalParamsLoc = -1;
    int m_ShadowsEnabledLoc = -1;
    int m_ShadowBiasLoc = -1;
    int m_ShadowFilterLoc = -1;
    int m_UseTexLoc = -1;
    int m_LastUseTexState = -1;
    int m_AmbientLoc = -1;
    int m_EmissionColorLoc = -1;
    int m_EmissionPowerLoc = -1;
    std::array<int, QC_MAX_LIGHTS> m_aLightEnabledLocations{};

    std::array<RenderTexture2D, QC_MAX_LIGHTS> m_aShadowMaps{};
    std::array<Camera3D, QC_MAX_LIGHTS> m_aShadowCameras{};
    std::array<Mat4, QC_MAX_LIGHTS> m_aCachedLightViews{};
    std::array<Mat4, QC_MAX_LIGHTS> m_aCachedLightProjections{};
    std::array<int, QC_MAX_LIGHTS> m_aLightViewLocations{};
    std::array<int, QC_MAX_LIGHTS> m_aLightProjectionLocations{};
    std::array<std::uint64_t, QC_MAX_LIGHTS> m_aShadowSignatures{};
    std::uint64_t m_LastTransformRevision = 0;
    std::uint64_t m_LastGeometrySignature = 0;
    BoundingBox m_LastSceneBounds = { { -5.0f, -5.0f, -5.0f }, { 5.0f, 5.0f, 5.0f } };
    bool m_ShadowCacheValid = false;
    bool m_ShadowBindingsAssigned = false;
    std::vector<SSceneEntityFrameData> m_vFrameData;
    std::vector<SRenderCommand> m_vRenderCommands;
    std::vector<Matrix> m_vInstanceTransforms;
    SSceneRendererTimings m_Timings{};
    SRenderBatchStats m_LastBatchStats{};
    int m_ActiveLightCount = 0;
    CTaskPool* m_pTaskPool = nullptr;
};

#endif // __APPLICATION_SCENE_RENDERER_H__
