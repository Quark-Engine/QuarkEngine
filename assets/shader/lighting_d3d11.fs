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

#define SHADOW_DEPTH_RANGE 1000.0

float UnpackDepth24(float3 rgb)
{
    return dot(rgb, float3(1.0, 1.0 / 255.0, 1.0 / 65025.0)) * SHADOW_DEPTH_RANGE;
}

float SampleShadowDepth(int index, int2 texel)
{
    int3 location = int3(texel, 0);
    if (index == 0) return UnpackDepth24(shadowMap0.Load(location).rgb);
    if (index == 1) return UnpackDepth24(shadowMap1.Load(location).rgb);
    if (index == 2) return UnpackDepth24(shadowMap2.Load(location).rgb);
    return UnpackDepth24(shadowMap3.Load(location).rgb);
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
        return 0.0;
    }

    float NdotL = saturate(dot(normal, lightDir));
    float2 texelSize = GetShadowTexelSize(index);

    float4 lightClip = mul(lightProjections[index], mul(lightViews[index], float4(fragPosition, 1.0)));
    if (lightClip.w <= 0.0)
    {
        return 0.0;
    }

    float3 projected = lightClip.xyz / lightClip.w;
    projected.x = projected.x * 0.5 + 0.5;
    projected.y = -projected.y * 0.5 + 0.5;
    if (projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0)
    {
        return 0.0;
    }

    float currentDepth = lightClip.w;
    float bias = currentDepth * max(shadowBias, 0.0005) * (1.0 + 2.0 * (1.0 - NdotL)) + 0.01;
    int2 mapSize = int2(round(1.0 / texelSize));
    int2 baseTexel = int2(projected.xy * float2(mapSize));
    int radius = clamp(shadowFilterQuality, 0, 2);
    int sampleWidth = radius * 2 + 1;
    int samples = sampleWidth * sampleWidth;

    float shadow = 0.0;
    [loop]
    for (int x = -radius; x <= radius; ++x)
    {
        [loop]
        for (int y = -radius; y <= radius; ++y)
        {
            int2 texel = clamp(baseTexel + int2(x, y), int2(0, 0), mapSize - 1);
            float closestDepth = SampleShadowDepth(index, texel);
            shadow += (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
        }
    }

    return shadow / float(samples);
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

    float3 normal = perturb_normal(input.fragNormal, input.fragTangent, input.fragTexCoord);
    float3 viewDir = normalize(viewPos - input.fragPosition);

    float3 lightAccum = float3(0.0, 0.0, 0.0);
    float3 specularSum = float3(0.0, 0.0, 0.0);

    [unroll]
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            int lightType = lights[i].type;
            float3 lightDir = float3(0.0, 0.0, 0.0);
            float attenuation = 1.0;
            float lightEnergy = clamp(lights[i].intensity, 0.0, 4.0);
            float3 lightColor = lights[i].color.rgb * lightEnergy;

            if (lightType == LIGHT_DIRECTIONAL)
            {
                lightDir = normalize(lights[i].position - lights[i].target);
            }
            else if (lightType == LIGHT_POINT)
            {
                float3 toLight = lights[i].position - input.fragPosition;
                float dist = max(length(toLight), 0.35);
                lightDir = toLight / dist;

                float range = max(lights[i].range, 0.001);
                attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
                attenuation *= attenuation;
            }
            else if (lightType == LIGHT_SPOT)
            {
                float3 toLight = lights[i].position - input.fragPosition;
                float dist = max(length(toLight), 0.35);
                lightDir = toLight / dist;

                float range = max(lights[i].range, 0.001);
                attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
                attenuation *= attenuation;

                float3 spotDir = normalize(lights[i].position - lights[i].target);
                float theta = dot(lightDir, spotDir);
                float cutoff = cos(radians(lights[i].spotAngle));

                if (theta < cutoff)
                {
                    attenuation = 0.0;
                }
                else
                {
                    attenuation *= smoothstep(cutoff, cutoff + 0.05, theta);
                }
            }
            else if (lightType == LIGHT_AREA)
            {
                float3 toLight = lights[i].position - input.fragPosition;
                float dist = max(length(toLight), 0.35);
                lightDir = toLight / dist;

                float range = max(lights[i].range, 0.001);
                attenuation = sqrt(clamp(1.0 - dist / range, 0.0, 1.0));
            }

            float directLight = max(dot(normal, lightDir), 0.0);
            float shadow = (shadowsEnabled == 1) ? shadow_factor(i, input.fragPosition, normal, lightDir) : 0.0;
            float wrappedLight = clamp((dot(normal, lightDir) + 0.28) / 1.28, 0.0, 1.0);
            float diffuse = max(directLight, wrappedLight * 0.25);

            float3 bounce = lightColor * attenuation * 0.08;
            lightAccum += (lightColor * diffuse * attenuation) * (1.0 - shadow) + bounce;

            if (directLight > 0.0)
            {
                float spec = pow(max(dot(viewDir, reflect(-lightDir, normal)), 0.0), 24.0);
                specularSum += spec * attenuation * 0.2 * lightColor * (1.0 - shadow);
            }
        }
    }

    float3 color = texelColor.rgb * input.fragColor.rgb * (ambient.rgb + lightAccum) + specularSum;
    color += emissionColor * max(emissionPower, 0.0);
    float4 result = float4(color, texelColor.a * input.fragColor.a);
    result = pow(max(result, float4(0.0, 0.0, 0.0, 0.0)), float4(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    if (result.a < 0.01)
    {
        discard;
    }
    return result;
}
