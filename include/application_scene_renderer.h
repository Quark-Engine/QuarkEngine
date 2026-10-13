#ifndef __APPLICATION_SCENE_RENDERER_H__
#define __APPLICATION_SCENE_RENDERER_H__

#include "QuarkCore/QuarkCore.hpp"
#include "QuarkCore/QuarkLights.hpp"

#include <array>
#include <cstdint>

class CScene;
class CLightRegistry;
class CMeshComponent;

struct SSceneRendererTimings
{
    float updateMs = 0.0f;
    float shadowPassMs = 0.0f;
};

class CSceneRenderer
{
public:
    void Initialize(bool vulkanBackend, int shadowMapSize, bool shadowsEnabled,
        float shadowBias, int shadowFilterQuality);

    void Update(CScene& scene, CLightRegistry& lights, const Vec3& cameraPosition,
        bool shadowsEnabled, float shadowBias, int shadowFilterQuality);

    void EnsureLightingShader(CMeshComponent* pMesh);

    bool IsMeshVisible(CMeshComponent* pMesh, const Mat4& worldTransform,
        const Camera3D& camera, float aspectRatio) const;

    void SetUseTexture(bool useTexture);

    Shader GetLightingShader() const { return m_LightingShader; }
    SSceneRendererTimings GetTimings() const { return m_Timings; }

    void Unload();

private:
    void UnloadShaders();
    void UnloadShadowMaps();

    Shader m_LightingShader{};
    Shader m_ShadowShader{};

    int m_ShadowsEnabledLoc = -1;
    int m_ShadowBiasLoc = -1;
    int m_ShadowFilterLoc = -1;
    int m_UseTexLoc = -1;
    int m_AmbientLoc = -1;
    int m_EmissionColorLoc = -1;
    int m_EmissionPowerLoc = -1;
    std::array<int, QC_MAX_LIGHTS> m_aLightEnabledLocations{};

    std::array<RenderTexture2D, QC_MAX_LIGHTS> m_aShadowMaps{};
    std::array<Camera3D, QC_MAX_LIGHTS> m_aShadowCameras{};
    std::array<int, QC_MAX_LIGHTS> m_aLightViewLocations{};
    std::array<int, QC_MAX_LIGHTS> m_aLightProjectionLocations{};
    std::array<std::uint64_t, QC_MAX_LIGHTS> m_aShadowSignatures{};
    bool m_ShadowCacheValid = false;
    bool m_ShadowBindingsAssigned = false;
    SSceneRendererTimings m_Timings{};
};

#endif // __APPLICATION_SCENE_RENDERER_H__
