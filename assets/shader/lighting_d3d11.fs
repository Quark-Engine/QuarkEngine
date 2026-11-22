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
    float4x4 model;
    float4x4 mvp;
    float4x4 normalMatrix;
    float4 colDiffuse;
    int useInstancing;
    int instanceBase;
    int2 _padInst;
    float4 postParams;
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

static const float2 POISSON_DISK[16] =
{
    float2(-0.94201624, -0.39906216), float2( 0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870), float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845), float2( 0.97484398,  0.75648379),
    float2( 0.44323325, -0.97511554), float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507), float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367), float2( 0.14383161, -0.14100790)
};

float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float InterleavedGradientNoise(float2 screenPos)
{
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

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

float SampleShadowBilinear(int index, float2 uv, int2 mapSize, float currentDepth, float bias)
{
    float2 texelCoord = uv * float2(mapSize) - 0.5;
    int2 baseTexel = int2(floor(texelCoord));
    float2 f = texelCoord - float2(baseTexel);

    int2 t00 = clamp(baseTexel + int2(0, 0), int2(0, 0), mapSize - 1);
    int2 t10 = clamp(baseTexel + int2(1, 0), int2(0, 0), mapSize - 1);
    int2 t01 = clamp(baseTexel + int2(0, 1), int2(0, 0), mapSize - 1);
    int2 t11 = clamp(baseTexel + int2(1, 1), int2(0, 0), mapSize - 1);

    float s00 = (currentDepth - bias > SampleShadowDepth(index, t00)) ? 1.0 : 0.0;
    float s10 = (currentDepth - bias > SampleShadowDepth(index, t10)) ? 1.0 : 0.0;
    float s01 = (currentDepth - bias > SampleShadowDepth(index, t01)) ? 1.0 : 0.0;
    float s11 = (currentDepth - bias > SampleShadowDepth(index, t11)) ? 1.0 : 0.0;

    return lerp(lerp(s00, s10, f.x), lerp(s01, s11, f.x), f.y);
}

float shadow_factor(int index, float3 fragPosition, float3 normal, float3 lightDir, float2 screenPos)
{
    if (shadowsEnabled == 0)
    {
        return 0.0;
    }

    float NdotL = saturate(dot(normal, lightDir));
    if (NdotL <= 0.0001)
    {
        return 1.0;
    }
    float2 texelSize = GetShadowTexelSize(index);
    float baseBias = max(shadowBias, 0.0005);

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
    float slopeFactor = 1.0 - NdotL;
    float baseDepthBias = currentDepth * baseBias * (1.2 + 3.5 * slopeFactor) + 0.012;
    float tapSlopeBias = currentDepth * baseBias * (0.9 + 2.5 * slopeFactor) + 0.005;
    int2 mapSize = int2(round(1.0 / texelSize));

    float shadow = 0.0;
    if (shadowFilterQuality <= 0)
    {
        shadow = SampleShadowBilinear(index, projected.xy, mapSize, currentDepth, baseDepthBias);
    }
    else
    {
        int tapCount = (shadowFilterQuality == 1) ? 8 : 16;
        float spreadTexels = (shadowFilterQuality == 1) ? 1.65 : 2.75;
        float angle = InterleavedGradientNoise(screenPos) * 6.2831853;
        float s = sin(angle);
        float c = cos(angle);

        [loop]
        for (int t = 0; t < tapCount; ++t)
        {
            float2 d = POISSON_DISK[t];
            float2 offset = float2(c * d.x - s * d.y, s * d.x + c * d.y) * spreadTexels;
            float tapBias = baseDepthBias + tapSlopeBias * length(offset);
            shadow += SampleShadowBilinear(index, projected.xy + offset * texelSize,
                                           mapSize, currentDepth, tapBias);
        }
        shadow /= float(tapCount);
    }

    float edgeFade = smoothstep(0.0, 0.05, projected.x) *
                     smoothstep(1.0, 0.95, projected.x) *
                     smoothstep(0.0, 0.05, projected.y) *
                     smoothstep(1.0, 0.95, projected.y);
    return shadow * edgeFade;
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
            float shadow = (shadowsEnabled == 1 && directLight > 0.0001)
                ? shadow_factor(i, input.fragPosition, normalize(input.fragNormal), lightDir, input.position.xy)
                : 0.0;
            float wrappedLight = clamp((dot(normal, lightDir) + 0.50) / 1.50, 0.0, 1.0);
            float directTerm = directLight * (1.0 - shadow);
            float wrapTerm = wrappedLight * 0.30 * (1.0 - shadow * clamp(directLight * 4.0, 0.0, 1.0));
            float diffuse = max(directTerm, wrapTerm);

            float3 bounce = lightColor * attenuation * 0.08;
            lightAccum += (lightColor * diffuse * attenuation) + bounce;

            if (directLight > 0.0)
            {
                float spec = pow(max(dot(viewDir, reflect(-lightDir, normal)), 0.0), 24.0);
                specularSum += spec * attenuation * 0.2 * lightColor * (1.0 - shadow);
            }
        }
    }

    float hemiFactor = normal.y * 0.5 + 0.5;
    float3 skyAmbient = lerp(float3(0.035, 0.035, 0.04), float3(0.075, 0.095, 0.13), hemiFactor);
    float3 envAmbient = max(ambient.rgb, skyAmbient * (postParams.w > 0.5 ? 1.0 : 0.45));

    float3 color = texelColor.rgb * input.fragColor.rgb * (envAmbient + lightAccum) + specularSum;
    color += emissionColor * max(emissionPower, 0.0);

    float exposure = postParams.x > 0.001 ? postParams.x : 1.05;
    float useToneMap = postParams.x > 0.001 ? postParams.y : 1.0;
    float bloomStrength = postParams.x > 0.001 ? max(postParams.z, 0.0) : 0.25;

    color *= exposure;
    float3 bloomExcess = max(color - float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0));
    color += bloomExcess * bloomStrength * 0.45;

    if (useToneMap > 0.5)
    {
        color = ACESFilm(color);
    }
    else
    {
        color = saturate(color);
    }

    float outAlpha = texelColor.a * input.fragColor.a;
    if (outAlpha < 0.01)
    {
        discard;
    }
    return float4(pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)), outAlpha);
}
