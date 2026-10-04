#ifndef __LIGHTING_H__
#define __LIGHTING_H__
#include "QuarkCore/QuarkCore.hpp"
#include "QuarkCore/QuarkLights.hpp"

#include <array>
#include <string>

enum ELightType
{
    LIGHT_TYPE_DIRECTIONAL = 0,
    LIGHT_TYPE_POINT       = 1,
    LIGHT_TYPE_SPOT        = 2,
    LIGHT_TYPE_AREA        = 3
};

class CLightState
{
public:
    int m_Id = -1;

    qc::Light m_Light;
    qc::Vec3 m_Position;
    qc::Vec3 m_Target;
    qc::Vec3 m_Rotation;

    qc::Color m_Color = qc::WHITE;
    bool m_Enabled = true;

    float m_SpotAngle = 30.0f;
    int m_SpotAngleLoc = -1;
    int m_IntensityLoc = -1;
    int m_RangeLoc = -1;

    float m_Intensity = 1.0f;
    float m_Range = 5.0f;
};

CLightState CreateLighting(qc::Vec3 pos, qc::Color color);
qc::Light CreateLightAtSlot(int slot, int type, qc::Vec3 position, qc::Vec3 target, qc::Color color, qc::Shader shader);
void InitializeLightingUniformCache(CLightState& lighting, qc::Shader shader, int slot);
void UpdateLighting(qc::Shader shader, CLightState& lighting);

class CLightRegistry
{
public:
    static constexpr int INVALID_ID = -1;

    int Allocate();
    void Free(int id);
    void Reset();

    bool IsAllocated(int id) const;
    int AllocatedCount() const;

private:
    std::array<bool, QC_MAX_LIGHTS> m_aUsed = {};
};

#endif // __LIGHTING_H__
