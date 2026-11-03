struct VSInput
{
    float4 aPosition      : POSITION;
    float4 aNormal        : NORMAL;
    float4 aWorldPosition : WORLD_POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 vNormal  : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = input.aPosition;
    output.vNormal  = normalize(input.aNormal.xyz);
    return output;
}
