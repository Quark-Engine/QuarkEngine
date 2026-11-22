cbuffer SkyboxParams : register(b0)
{
    float4x4 mvp;
    float4 sunDir;
    float4 postParams;
};

struct VSInput
{
    float4 aPosition : POSITION;
    float4 aNormal   : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 fragDir  : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(mvp, input.aPosition);
    output.fragDir  = normalize(input.aNormal.xyz);
    return output;
}
