struct PSInput
{
    float4 position : SV_POSITION;
    float3 vNormal  : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    float depth = saturate(input.position.z);
    return float4(depth, depth, depth, 1.0);
}
