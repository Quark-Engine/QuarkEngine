#define MAX_LIGHTS 4
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHT_AREA 3

Texture2D texture0   : register(t0);
Texture2D shadowMap0 : register(t1);
Texture2D shadowMap1 : register(t2);
Texture2D shadowMap2 : register(t3);
Texture2D shadowMap3 : register(t4);
Texture2D normalMap  : register(t6);
SamplerState mainSampler : register(s0);

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
};

struct PSInput
{
    float4 position     : SV_POSITION;
    float2 fragTexCoord : TEXCOORD0;
    float4 fragColor    : COLOR;
    float3 fragNormal   : NORMAL;
    float3 fragPosition : TEXCOORD1;
    float3 fragTangent  : TANGENT;
};

float UnpackShadowDepth(float4 rgba)
{
    const float4 bitShift = float4(1.0, 1.0 / 256.0, 1.0 / 65536.0, 1.0 / 16777216.0);
    return dot(rgba, bitShift);
}

float4 SampleShadowMap(int index, float2 uv)
{
    if (index == 0) return shadowMap0.SampleLevel(mainSampler, uv, 0.0);
    if (index == 1) return shadowMap1.SampleLevel(mainSampler, uv, 0.0);
    if (index == 2) return shadowMap2.SampleLevel(mainSampler, uv, 0.0);
    return shadowMap3.SampleLevel(mainSampler, uv, 0.0);
}

float2 GetShadowTexelSize(int index)
{
    uint w = 1024;
    uint h = 1024;
    if (index == 0) shadowMap0.GetDimensions(w, h);
    else if (index == 1) shadowMap1.GetDimensions(w, h);
    else if (index == 2) shadowMap2.GetDimensions(w, h);
    else shadowMap3.GetDimensions(w, h);
    return 1.0 / max(float2(float(w), float(h)), float2(1.0, 1.0));
}

float shadow_factor(int index, float3 fragPosition, float3 normal, float3 lightDir)
{
    if (shadowsEnabled == 0)
    {
        return 1.0;
    }

    float4 lightClip = mul(lightProjections[index], mul(lightViews[index], float4(fragPosition, 1.0)));
    if (lightClip.w <= 0.0)
    {
        return 1.0;
    }

    float3 projected = lightClip.xyz / lightClip.w;
    projected.x = projected.x * 0.5 + 0.5;
    projected.y = -projected.y * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0 ||
        projected.z <= 0.0 || projected.z >= 1.0)
    {
        return 1.0;
    }

    float2 texelSize = GetShadowTexelSize(index);
    float baseBias = max(shadowBias, 0.00001);
    float bias = max(baseBias * (1.0 - dot(normal, lightDir)), baseBias * 0.35);
    int filterRadius = clamp(shadowFilterQuality, 0, 2);
    if (filterRadius == 0)
    {
        float closestDepth = UnpackShadowDepth(SampleShadowMap(index, projected.xy));
        return (projected.z - bias > closestDepth) ? 0.22 : 1.0;
    }

    float shadow = 0.0;
    int samples = 0;
    [loop]
    for (int x = -2; x <= 2; ++x)
    {
        [loop]
        for (int y = -2; y <= 2; ++y)
        {
            if (abs(x) > filterRadius || abs(y) > filterRadius)
            {
                continue;
            }
            float2 offset = float2(float(x), float(y)) * texelSize;
            float closestDepth = UnpackShadowDepth(SampleShadowMap(index, projected.xy + offset));
            shadow += (projected.z - bias > closestDepth) ? 0.22 : 1.0;
            samples++;
        }
    }

    return shadow / float(max(samples, 1));
}

float3 perturb_normal(float3 geometricNormal, float3 tangent, float2 uv)
{
    float3 n = normalize(geometricNormal);
    float3 t = tangent - n * dot(n, tangent);
    if (length(t) <= 1e-5)
    {
        return n;
    }
    t = normalize(t);
    float3 b = normalize(cross(n, t));
    float3 mapNormal = normalMap.Sample(mainSampler, uv).xyz * 2.0 - 1.0;
    float3 worldNormal = normalize(t * mapNormal.x + b * mapNormal.y + n * mapNormal.z);
    return dot(worldNormal, n) < 0.0 ? n : worldNormal;
}

float4 main(PSInput input) : SV_TARGET
{
    float4 texelColor = texture0.Sample(mainSampler, input.fragTexCoord);
    float4 baseColor = texelColor * input.fragColor;
    if (baseColor.a <= 0.01)
    {
        discard;
    }

    float3 normal = perturb_normal(input.fragNormal, input.fragTangent, input.fragTexCoord);
    float3 viewDir = normalize(viewPos - input.fragPosition);

    float3 totalLight = ambient.rgb * 0.28;
    float3 specularSum = float3(0.0, 0.0, 0.0);

    [unroll]
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            float3 lightDir = float3(0.0, 0.0, 0.0);
            float attenuation = 1.0;
            float range = max(lights[i].range, 0.001);

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                lightDir = -normalize(lights[i].target - lights[i].position);
            }
            else
            {
                float3 toLight = lights[i].position - input.fragPosition;
                float dist = length(toLight);
                lightDir = toLight / max(dist, 0.0001);
                float rangeFactor = clamp(1.0 - dist / range, 0.0, 1.0);
                attenuation = (rangeFactor * rangeFactor) /
                    (1.0 + 0.09 * dist + 0.032 * dist * dist);

                if (lights[i].type == LIGHT_SPOT)
                {
                    float3 spotDir = normalize(lights[i].target - lights[i].position);
                    float theta = dot(lightDir, -spotDir);
                    float outerCutoff = cos(radians(clamp(lights[i].spotAngle, 1.0, 89.0)));
                    float innerCutoff = cos(radians(max(1.0, lights[i].spotAngle * 0.8)));
                    float epsilon = max(innerCutoff - outerCutoff, 0.0001);
                    float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
                    attenuation *= spotIntensity;
                }
                else if (lights[i].type == LIGHT_AREA)
                {
                    attenuation = rangeFactor;
                }
            }

            float NdotL = max(dot(normal, lightDir), 0.0);
            float wrapped = max((dot(normal, lightDir) + 0.2) / 1.2, 0.0);
            float diffuseStrength = (lights[i].type == LIGHT_AREA)
                ? lerp(NdotL, wrapped, 0.65)
                : lerp(NdotL, wrapped, 0.25);

            float3 lightColor = lights[i].color.rgb * max(lights[i].intensity, 0.0) * attenuation;
            float shadow = shadow_factor(i, input.fragPosition, normal, lightDir);
            float3 diffuse = lightColor * diffuseStrength * shadow;

            float bounceStrength = max(dot(-normal, lightDir), 0.0) * 0.025;
            float3 bounce = lightColor * bounceStrength;

            float3 reflectDir = reflect(-lightDir, normal);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 28.0);
            float3 specular = lightColor * spec * 0.12 * shadow;

            totalLight += diffuse + bounce;
            specularSum += specular;
        }
    }

    float3 litColor = baseColor.rgb * totalLight + specularSum +
        emissionColor * max(emissionPower, 0.0);
    litColor = pow(max(litColor, float3(0.0, 0.0, 0.0)), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return float4(litColor, baseColor.a);
}
