#ifndef __APPLICATION_SCENE_RENDERER_H__
#define __APPLICATION_SCENE_RENDERER_H__

#include "QuarkCore/QuarkCore.hpp"
#include "QuarkCore/QuarkLights.hpp"

#include <array>

class CScene;
class CLightRegistry;
class CMeshComponent;

class CSceneRenderer
{
public:
    void Initialize(bool vulkanBackend, int shadowMapSize, bool shadowsEnabled,
        float shadowBias, int shadowFilterQuality);

    void Update(CScene& scene, CLightRegistry& lights, const qc::Vec3& cameraPosition,
        bool shadowsEnabled, float shadowBias, int shadowFilterQuality);

    void EnsureLightingShader(CMeshComponent* pMesh);

    void SetUseTexture(bool useTexture);

    qc::Shader GetLightingShader() const { return m_LightingShader; }

    void Unload();

private:
    void UnloadShaders();
    void UnloadShadowMaps();

    qc::Shader m_LightingShader{};
    qc::Shader m_ShadowShader{};

    int m_ShadowsEnabledLoc = -1;
    int m_ShadowBiasLoc = -1;
    int m_ShadowFilterLoc = -1;
    int m_UseTexLoc = -1;
    int m_AmbientLoc = -1;
    int m_EmissionColorLoc = -1;
    int m_EmissionPowerLoc = -1;

    std::array<qc::RenderTexture2D, QC_MAX_LIGHTS> m_aShadowMaps{};
    std::array<qc::Camera3D, QC_MAX_LIGHTS> m_aShadowCameras{};
    std::array<int, QC_MAX_LIGHTS> m_aLightViewLocations{};
    std::array<int, QC_MAX_LIGHTS> m_aLightProjectionLocations{};
};

#endif // __APPLICATION_SCENE_RENDERER_H__