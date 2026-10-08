#include "application_scene_renderer.h"

#include "component.h"
#include "lighting.h"
#include "scene.h"
#include "engine/transform.h"

#include <cfloat>
namespace
{
void ExpandBoundsWithPoint(BoundingBox& bounds, const Vec3& point)
{
    bounds.min.x = fminf(bounds.min.x, point.x);
    bounds.min.y = fminf(bounds.min.y, point.y);
    bounds.min.z = fminf(bounds.min.z, point.z);
    bounds.max.x = fmaxf(bounds.max.x, point.x);
    bounds.max.y = fmaxf(bounds.max.y, point.y);
    bounds.max.z = fmaxf(bounds.max.z, point.z);
}

BoundingBox ComputeSceneBounds(const CScene& scene)
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

        CEntity& mutableEntity = const_cast<CEntity&>(entity);
        CMeshComponent* pMutableMesh = mutableEntity.GetMeshComponent();
        if (pMutableMesh->m_BoundsDirty)
        {
            pMutableMesh->m_CachedLocalBounds = GetModelBoundingBox(pMesh->m_Model);
            pMutableMesh->m_BoundsDirty = false;
        }
        BoundingBox localBounds = pMutableMesh->m_CachedLocalBounds;
        const int entityIndex = static_cast<int>(&entity - scene.m_vEntities.data());
        Mat4 transform = quark::ComposeMeshWorld(scene, entity);

        const Vec3 aCorners[8] = {
            { localBounds.min.x, localBounds.min.y, localBounds.min.z },
            { localBounds.max.x, localBounds.min.y, localBounds.min.z },
            { localBounds.min.x, localBounds.max.y, localBounds.min.z },
            { localBounds.max.x, localBounds.max.y, localBounds.min.z },
            { localBounds.min.x, localBounds.min.y, localBounds.max.z },
            { localBounds.max.x, localBounds.min.y, localBounds.max.z },
            { localBounds.min.x, localBounds.max.y, localBounds.max.z },
            { localBounds.max.x, localBounds.max.y, localBounds.max.z }
        };

        for (const Vec3& corner : aCorners)
        {
            ExpandBoundsWithPoint(bounds, Vec3(transform * corner));
        }
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

void SetShaderLightEnabled(Shader shader, int slot, bool enabled)
{
    int enabledLoc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", slot));
    int enabledValue = enabled ? 1 : 0;
    SetShaderValue(shader, enabledLoc, &enabledValue, SHADER_UNIFORM_INT);
}

void DisableAllShaderLights(Shader shader)
{
    for (int slot = 0; slot < QC_MAX_LIGHTS; slot++)
    {
        SetShaderLightEnabled(shader, slot, false);
    }
}

bool PrepareSceneLightUniforms(CScene& scene, CLightRegistry& lights, Shader shader, const Vec3& sceneCenter)
{
    DisableAllShaderLights(shader);

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
        const Mat4 worldTransform = quark::ComposeWorld(scene, entityIndex);
        const Vec3 worldPosition = Vec3(worldTransform * Vec3{0.0f, 0.0f, 0.0f});

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
    std::array<int, QC_MAX_LIGHTS>& aLightViewLocations,
    std::array<int, QC_MAX_LIGHTS>& aLightProjectionLocations)
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
            aRendered[pLight->m_Light.m_Id]) continue;

        const int slot = pLight->m_Light.m_Id;
        const int lightEntityIndex = static_cast<int>(&entity - scene.m_vEntities.data());
        const Mat4 lightTransform = quark::ComposeWorld(scene, lightEntityIndex);
        aShadowCameras[slot].position = Vec3(lightTransform * Vec3{0.0f, 0.0f, 0.0f});
        aShadowCameras[slot].target = pLight->m_Light.m_Target;
        if ((aShadowCameras[slot].position - aShadowCameras[slot].target).length() <= 0.000001f)
        {
            aShadowCameras[slot].target = sceneCenter;
            if ((aShadowCameras[slot].position - aShadowCameras[slot].target).length() <= 0.000001f)
            {
                aShadowCameras[slot].target = aShadowCameras[slot].position + Vec3{0.0f, -1.0f, 0.0f};
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
                pMesh->m_Model.meshCount <= 0 || !pMesh->m_Model.meshes) continue;

            const Mat4 entityTransform = quark::ComposeWorld(scene, sourceIndex) * pMesh->m_Model.transform;
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
        SetShaderValueMatrix(lightingShader, aLightViewLocations[slot], lightView.m);
        SetShaderValueMatrix(lightingShader, aLightProjectionLocations[slot], lightProjection.m);

        EndShaderMode();
        EndMode3D();
        EndTextureMode();
    }

    const Mat4 identity = Mat4::identity();
    for (int slot = 0; slot < QC_MAX_LIGHTS; ++slot)
    {
        if (aRendered[slot])
        {
            continue;
        }

        if (aShadowMaps[slot].id != 0)
        {
            BeginTextureMode(aShadowMaps[slot]);
            ClearBackground(WHITE);
            EndTextureMode();
        }

        SetShaderValueMatrix(lightingShader, aLightViewLocations[slot], identity.m);
        SetShaderValueMatrix(lightingShader, aLightProjectionLocations[slot], identity.m);
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
}

void CSceneRenderer::Initialize(bool vulkanBackend, int shadowMapSize, bool shadowsEnabled,
    float shadowBias, int shadowFilterQuality)
{
    m_LightingShader = LoadShader(vulkanBackend ? "assets/shader/vulkan_lighting.vs" : "assets/shader/lighting.vs",
        vulkanBackend ? "assets/shader/vulkan_lighting.fs" : "assets/shader/lighting.fs");
    m_ShadowShader = LoadShader(vulkanBackend ? "assets/shader/vulkan_shadow_depth.vs" : "assets/shader/shadow_depth.vs",
        vulkanBackend ? "assets/shader/vulkan_shadow_depth.fs" : "assets/shader/shadow_depth.fs");
    m_LightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(m_LightingShader, "viewPos");
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
        m_aLightViewLocations[i] = GetShaderLocation(m_LightingShader, TextFormat("lightViews[%i]", i));
        m_aLightProjectionLocations[i] = GetShaderLocation(m_LightingShader, TextFormat("lightProjections[%i]", i));
    }

    m_UseTexLoc = GetShaderLocation(m_LightingShader, "useTexture");
    m_AmbientLoc = GetShaderLocation(m_LightingShader, "ambient");
    m_EmissionColorLoc = GetShaderLocation(m_LightingShader, "emissionColor");
    m_EmissionPowerLoc = GetShaderLocation(m_LightingShader, "emissionPower");
    SetShaderValue(m_LightingShader, m_AmbientLoc, Vec4{0.025f, 0.025f, 0.025f, 1.0f});
    SetShaderValue(m_LightingShader, m_EmissionColorLoc, Vec3{0.0f, 0.0f, 0.0f});
    SetShaderValue(m_LightingShader, m_EmissionPowerLoc, 0.0f);
}

void CSceneRenderer::Update(CScene& scene, CLightRegistry& lights, const Vec3& cameraPosition,
    bool shadowsEnabled, float shadowBias, int shadowFilterQuality)
{
    const BoundingBox sceneBounds = ComputeSceneBounds(scene);
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
    PrepareSceneLightUniforms(scene, lights, m_LightingShader, sceneCenter);
    if (shadowsEnabled)
    {
        RenderSceneShadowMaps(scene, m_ShadowShader, m_aShadowMaps, m_aShadowCameras, sceneCenter,
            m_LightingShader, m_aLightViewLocations, m_aLightProjectionLocations);
    }
    AssignShadowMaps(scene, m_aShadowMaps, m_LightingShader);
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
    }
    pMesh->m_ShaderAssigned = true;
}

void CSceneRenderer::SetUseTexture(bool useTexture)
{
    int use = useTexture ? 1 : 0;
    SetShaderValue(m_LightingShader, m_UseTexLoc, &use, SHADER_UNIFORM_INT);
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
