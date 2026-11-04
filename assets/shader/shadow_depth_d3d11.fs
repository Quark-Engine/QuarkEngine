#define SHADOW_DEPTH_RANGE 1000.0

struct PSInput
{
    float4 position : SV_POSITION;
    float3 vNormal  : NORMAL;
};

float3 packDepth24(float value)
{
    value = clamp(value, 0.0, 0.99999);
    float3 enc = frac(value * float3(1.0, 255.0, 65025.0));
    enc -= enc.yzz * float3(1.0 / 255.0, 1.0 / 255.0, 0.0);
    return enc;
}

float4 main(PSInput input) : SV_TARGET
{
    float linearDepth = input.position.w / SHADOW_DEPTH_RANGE;
    return float4(packDepth24(linearDepth), 1.0);
}
