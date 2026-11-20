#define MAX_LIGHTS 4

struct Light
{
    int enabled;
    int type;
    float intensity;
    float range;
    float3 position;
    float spotAngle;
    float3 target;
    float _pad0;
    float4 color;
};

cbuffer LightingParams : register(b0)
{
    float4x4 lightViews[MAX_LIGHTS];
    float4x4 lightProjections[MAX_LIGHTS];
    Light lights[MAX_LIGHTS];
    float4 ambient;
    float3 viewPos;
    int useTexture;
    float3 emissionColor;
    float emissionPower;
    int shadowsEnabled;
    float shadowBias;
    int shadowFilterQuality;
    float _padGlobal;
    float4x4 model;
    float4x4 mvp;
    float4x4 normalMatrix;
    float4 colDiffuse;
    int useInstancing;
    int instanceBase;
    int2 _padInst;
};

struct InstanceTransform
{
    float4x4 model;
    float4x4 mvp;
    float4x4 normalMatrix;
};

StructuredBuffer<InstanceTransform> gInstances : register(t10);

struct VSInput
{
    float4 aPosition  : POSITION;
    float2 aTexCoord0 : TEXCOORD0;
    float4 aColor     : COLOR;
    float4 aNormal    : NORMAL;
    float4 aTangent   : TANGENT;
    uint instanceId   : SV_InstanceID;
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
    float4x4 activeModel  = model;
    float4x4 activeMvp    = mvp;
    float4x4 activeNormal = normalMatrix;
    if (useInstancing != 0)
    {
        InstanceTransform inst = gInstances[instanceBase + input.instanceId];
        activeModel  = inst.model;
        activeMvp    = inst.mvp;
        activeNormal = inst.normalMatrix;
    }

    VSOutput output;
    output.position     = mul(activeMvp, input.aPosition);
    output.fragTexCoord = input.aTexCoord0;
    output.fragColor    = input.aColor * colDiffuse;
    output.fragNormal   = normalize(mul((float3x3)activeNormal, input.aNormal.xyz));
    output.fragPosition = mul(activeModel, input.aPosition).xyz;
    output.fragTangent  = normalize(mul((float3x3)activeNormal, input.aTangent.xyz));
    return output;
}
