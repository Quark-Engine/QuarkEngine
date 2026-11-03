struct PSInput
{
    float4 position : SV_POSITION;
    float3 vNormal  : NORMAL;
};

float4 PackShadowDepth(float depth)
{
    const float4 bitShift = float4(1.0, 256.0, 65536.0, 16777216.0);
    const float4 bitMask = float4(1.0 / 256.0, 1.0 / 256.0, 1.0 / 256.0, 0.0);
    float4 rgba = frac(depth * bitShift);
    rgba -= rgba.yzww * bitMask;
    return rgba;
}

float4 main(PSInput input) : SV_TARGET
{
    return PackShadowDepth(saturate(input.position.z));
}
