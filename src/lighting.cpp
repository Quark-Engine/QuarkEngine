#include "lighting.h"
#include <cstring>

using namespace qc;

void UpdateLighting(Shader shader, CLightState& l)
{
    l.m_Light.position = l.m_Position;
    l.m_Light.target   = l.m_Target;
    l.m_Light.color    = l.m_Color;
    l.m_Light.enabled  = l.m_Enabled;

    int enabledInt = l.m_Enabled ? 1 : 0;
    int type = l.m_Light.type;
    float aPosition[3] = { l.m_Position.x, l.m_Position.y, l.m_Position.z };
    float aTarget[3] = { l.m_Target.x, l.m_Target.y, l.m_Target.z };
    float aColor[4] = {l.m_Color.r / 255.f, l.m_Color.g / 255.f, l.m_Color.b / 255.f, l.m_Color.a / 255.f};

    SetShaderValue(shader, l.m_Light.enabledLoc, &enabledInt, SHADER_UNIFORM_INT);
    SetShaderValue(shader, l.m_Light.typeLoc, &type, SHADER_UNIFORM_INT);
    SetShaderValue(shader, l.m_Light.positionLoc, aPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, l.m_Light.targetLoc, aTarget, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, l.m_Light.colorLoc, aColor, SHADER_UNIFORM_VEC4);

    float intensity = l.m_Intensity;
    float range = l.m_Range;

    l.m_Light.attenuation = 1.0f / (range * range > 0.0001f ? range * range : 0.0001f);
    if (l.m_Light.attenuationLoc >= 0)
    {
        SetShaderValue(shader, l.m_Light.attenuationLoc, &l.m_Light.attenuation, SHADER_UNIFORM_FLOAT);
    }

    int intensityLoc = GetShaderLocation(shader, TextFormat("lights[%i].intensity", l.m_Id));
    int rangeLoc = GetShaderLocation(shader, TextFormat("lights[%i].range", l.m_Id));

    SetShaderValue(shader, intensityLoc, &intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, rangeLoc, &range, SHADER_UNIFORM_FLOAT);

    if (l.m_SpotAngleLoc == -1)
    {
        l.m_SpotAngleLoc = GetShaderLocation(shader, TextFormat("lights[%i].spotAngle", l.m_Id));
    }

    SetShaderValue(shader, l.m_SpotAngleLoc, &l.m_SpotAngle, SHADER_UNIFORM_FLOAT);
}

int CLightRegistry::Allocate()
{
    for (int id = 0; id < QC_MAX_LIGHTS; id++)
    {
        if (!m_aUsed[id])
        {
            m_aUsed[id] = true;
            return id;
        }
    }

    return INVALID_ID;
}

void CLightRegistry::Free(int id)
{
    if (id >= 0 && id < QC_MAX_LIGHTS)
    {
        m_aUsed[id] = false;
    }
}

void CLightRegistry::Reset()
{
    m_aUsed.fill(false);
}

bool CLightRegistry::IsAllocated(int id) const
{
    if (id < 0 || id >= QC_MAX_LIGHTS)
    {
        return false;
    }
    return m_aUsed[id];
}

int CLightRegistry::AllocatedCount() const
{
    int count = 0;
    for (int id = 0; id < QC_MAX_LIGHTS; id++)
    {
        if (m_aUsed[id])
        {
            count++;
        }
    }
    return count;
}

CLightState CreateLighting(Vec3 pos, Color color)
{
    CLightState l  = {};
    l.m_Position   = pos;
    l.m_Target     = Vec3(0,0,0);
    l.m_Color      = color;
    l.m_Enabled    = true;
    l.m_Light.type = LIGHT_TYPE_POINT;
    l.m_Intensity  = 1.0f;
    l.m_Range      = 5.0f;
    l.m_SpotAngle  = 30.0f;
    return l;
}

Light CreateLightAtSlot(int slot, int type, Vec3 position, Vec3 target, Color color, Shader shader)
{
    Light light       = { 0 };
    light.enabled     = true;
    light.type        = type;
    light.position    = position;
    light.target      = target;
    light.color       = color;
    light.enabledLoc  = GetShaderLocation(shader, TextFormat("lights[%i].enabled",  slot));
    light.typeLoc     = GetShaderLocation(shader, TextFormat("lights[%i].type",     slot));
    light.positionLoc = GetShaderLocation(shader, TextFormat("lights[%i].position", slot));
    light.targetLoc   = GetShaderLocation(shader, TextFormat("lights[%i].target",   slot));
    light.colorLoc    = GetShaderLocation(shader, TextFormat("lights[%i].color",    slot));
    return light;
}

void InitializeLightingUniformCache(CLightState& lighting, Shader shader, int slot)
{
    lighting.m_IntensityLoc  = GetShaderLocation(shader, TextFormat("lights[%i].intensity", slot));
    lighting.m_RangeLoc      = GetShaderLocation(shader, TextFormat("lights[%i].range", slot));
    lighting.m_SpotAngleLoc = GetShaderLocation(shader, TextFormat("lights[%i].spotAngle", slot));
}

