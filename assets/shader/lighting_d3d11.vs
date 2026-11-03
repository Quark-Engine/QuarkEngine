struct VSInput
{
    float4 aPosition      : POSITION;
    float2 aTexCoord0     : TEXCOORD0;
    float4 aColor         : COLOR;
    float4 aNormal        : NORMAL;
    float4 aWorldPosition : WORLD_POSITION;
    float4 aTangent       : TANGENT;
};

struct VSOutput
{
    float4 position     : SV_POSITION;
    float2 fragTexCoord : TEXCOORD0;
    float4 fragColor    : COLOR;
    float3 fragNormal   : NORMAL;
    float3 fragPosition : TEXCOORD1;
    float3 fragTangent  : TANGENT;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position     = input.aPosition;
    output.fragTexCoord = input.aTexCoord0;
    output.fragColor    = input.aColor;
    output.fragNormal   = normalize(input.aNormal.xyz);
    output.fragPosition = input.aWorldPosition.xyz;
    output.fragTangent  = normalize(input.aTangent.xyz);
    return output;
}
