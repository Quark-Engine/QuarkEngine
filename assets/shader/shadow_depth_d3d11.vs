cbuffer ShadowParams : register(b0)
{
    float4x4 mvp;
    float4x4 normalMatrix;
};

struct VSInput
{
    float4 aPosition : POSITION;
    float4 aNormal   : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 vNormal  : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(mvp, input.aPosition);
    output.vNormal  = normalize(mul((float3x3)normalMatrix, input.aNormal.xyz));
    return output;
}
