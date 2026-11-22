#version 450

struct Light {
    vec3  position;
    vec3  target;
    vec4  color;
    float attenuation;
    int   enabled;
    int   type;
};

layout(set = 0, binding = 1) uniform sampler2D albedo;
layout(set = 0, binding = 2) uniform sampler2D shadowMaps[4];
layout(set = 0, binding = 3) uniform ShadowMatrices {
    mat4 lightViews[4];
    mat4 lightProjections[4];
};
layout(set = 0, binding = 4) uniform LightsBlock {
    vec4  ambient;
    vec4  colDiffuse;
    vec4  viewPos;
    Light lights[4];
};
layout(set = 0, binding = 6) uniform sampler2D normalMap;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPosition;
layout(location = 4) in vec3 fragTangent;

layout(location = 0) out vec4 finalColor;

#define MAX_LIGHTS        4
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_AREA        3

#define GLOBAL_SHADOW_BIAS_SCALE 100000.0

vec3 decodeLightColor(int lightIndex)
{
    return lights[lightIndex].color.rgb;
}

float decodeLightRange(int lightIndex)
{
    float packed = lights[lightIndex].attenuation;
    float rangePart = floor(packed * 0.001);
    return max(rangePart - 1.0, 0.001);
}

float decodeSpotAngle(int lightIndex)
{
    float packed = lights[lightIndex].attenuation;
    float rangePart = floor(packed * 0.001);
    return packed - rangePart * 1000.0;
}

#define SHADOW_DEPTH_RANGE 1000.0

const vec2 POISSON_DISK[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float InterleavedGradientNoise(vec2 screenPos)
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

float unpackDepth24(vec3 rgb)
{
    return dot(rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0)) * SHADOW_DEPTH_RANGE;
}

float sampleShadowBilinear(int lightIndex, vec2 uv, ivec2 mapSize, float currentDepth, float bias)
{
    vec2 texelCoord = uv * vec2(mapSize) - vec2(0.5);
    ivec2 baseTexel = ivec2(floor(texelCoord));
    vec2 f = texelCoord - vec2(baseTexel);

    ivec2 t00 = clamp(baseTexel + ivec2(0, 0), ivec2(0), mapSize - 1);
    ivec2 t10 = clamp(baseTexel + ivec2(1, 0), ivec2(0), mapSize - 1);
    ivec2 t01 = clamp(baseTexel + ivec2(0, 1), ivec2(0), mapSize - 1);
    ivec2 t11 = clamp(baseTexel + ivec2(1, 1), ivec2(0), mapSize - 1);

    float s00 = currentDepth - bias > unpackDepth24(texelFetch(shadowMaps[lightIndex], t00, 0).rgb) ? 1.0 : 0.0;
    float s10 = currentDepth - bias > unpackDepth24(texelFetch(shadowMaps[lightIndex], t10, 0).rgb) ? 1.0 : 0.0;
    float s01 = currentDepth - bias > unpackDepth24(texelFetch(shadowMaps[lightIndex], t01, 0).rgb) ? 1.0 : 0.0;
    float s11 = currentDepth - bias > unpackDepth24(texelFetch(shadowMaps[lightIndex], t11, 0).rgb) ? 1.0 : 0.0;

    return mix(mix(s00, s10, f.x), mix(s01, s11, f.x), f.y);
}

float shadow_factor(int lightIndex, vec3 worldPosition, vec3 normal, vec3 lightDir,
    float shadowBias, int filterQuality)
{
    float NdotL = clamp(dot(normal, lightDir), 0.0, 1.0);
    if (NdotL <= 0.0001) return 1.0;
    float baseBias = max(shadowBias, 0.0005);

    vec4 lightSpacePosition = lightProjections[lightIndex] * lightViews[lightIndex]
        * vec4(worldPosition, 1.0);
    if (lightSpacePosition.w <= 0.0) return 0.0;

    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected.xy = projected.xy * 0.5 + 0.5;

    if (projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0)
        return 0.0;

    float currentDepth = lightSpacePosition.w;
    float slopeFactor = 1.0 - NdotL;
    float baseDepthBias = currentDepth * baseBias * (1.2 + 3.5 * slopeFactor) + 0.012;
    float tapSlopeBias = currentDepth * baseBias * (0.9 + 2.5 * slopeFactor) + 0.005;

    ivec2 mapSize = textureSize(shadowMaps[lightIndex], 0);
    vec2 texelSize = 1.0 / max(vec2(mapSize), vec2(1.0));

    float shadow = 0.0;
    if (filterQuality <= 0)
    {
        shadow = sampleShadowBilinear(lightIndex, projected.xy, mapSize, currentDepth, baseDepthBias);
    }
    else
    {
        int tapCount = (filterQuality == 1) ? 8 : 16;
        float spreadTexels = (filterQuality == 1) ? 1.65 : 2.75;
        float angle = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;
        float s = sin(angle);
        float c = cos(angle);
        mat2 rot = mat2(c, -s, s, c);

        for (int t = 0; t < 16; ++t)
        {
            if (t >= tapCount) break;
            vec2 offset = (rot * POISSON_DISK[t]) * spreadTexels;
            float tapBias = baseDepthBias + tapSlopeBias * length(offset);
            shadow += sampleShadowBilinear(lightIndex, projected.xy + offset * texelSize,
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

void main()
{
    int globalFlags = lights[3].type >> 2;
    bool shadowsEnabled = (globalFlags & 1) != 0;
    int shadowFilterQuality = (globalFlags >> 1) & 3;
    float shadowBias = float((globalFlags >> 3) & 0xFFFF) / GLOBAL_SHADOW_BIAS_SCALE;

    vec4 texelColor = texture(albedo, fragTexCoord);

    vec3 tint    = fragColor.rgb;
    vec3 normal  = normalize(fragNormal);
    vec3 tangent = normalize(fragTangent);
    if (abs(dot(normal, tangent)) < 0.999f)
    {
        vec3 bitangent = normalize(cross(normal, tangent));
        vec3 mapNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        normal = normalize(mat3(tangent, bitangent, normal) * mapNormal);
    }

    vec3 viewD      = normalize(viewPos.xyz - fragPosition);
    vec3 lightAccum = vec3(0.0);
    vec3 specular   = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled != 1) continue;

        int lightType = lights[i].type & 3;
        vec3 lightDir = vec3(0.0);
        float attenuation = 1.0;
        vec3 lightColor = decodeLightColor(i);

        if (lightType == LIGHT_DIRECTIONAL)
        {
            lightDir = normalize(lights[i].position - lights[i].target);
        }
        else if (lightType == LIGHT_POINT)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);
            lightDir = toLight / dist;

            float range = decodeLightRange(i);
            attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation *= attenuation;
        }
        else if (lightType == LIGHT_SPOT)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);

            lightDir = toLight / dist;
            float range = decodeLightRange(i);
            attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation *= attenuation;

            vec3 spotDir = normalize(lights[i].position - lights[i].target);
            float theta = dot(lightDir, spotDir);
            float cutoff = cos(radians(decodeSpotAngle(i)));

            if (theta < cutoff)
                attenuation = 0.0;
            else
                attenuation *= smoothstep(cutoff, cutoff + 0.05, theta);
        }
        else if (lightType == LIGHT_AREA)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);

            lightDir = toLight / dist;

            float range = decodeLightRange(i);
            attenuation = sqrt(clamp(1.0 - dist / range, 0.0, 1.0));
        }

        float directLight = max(dot(normal, lightDir), 0.0);
        float shadow = (shadowsEnabled && directLight > 0.0001)
            ? shadow_factor(i, fragPosition, normalize(fragNormal), lightDir,
                shadowBias, shadowFilterQuality) : 0.0;
        float wrappedLight = clamp((dot(normal, lightDir) + 0.50) / 1.50, 0.0, 1.0);
        float directTerm = directLight * (1.0 - shadow);
        float wrapTerm = wrappedLight * 0.30 * (1.0 - shadow * clamp(directLight * 4.0, 0.0, 1.0));
        float diffuse = max(directTerm, wrapTerm);

        vec3 bounce = lightColor * attenuation * 0.08;
        lightAccum += (lightColor * diffuse * attenuation) + bounce;

        if (directLight > 0.0)
        {
            float spec = pow(max(dot(viewD, reflect(-lightDir, normal)), 0.0), 24.0);
            specular += spec * attenuation * 0.2 * lightColor * (1.0 - shadow);
        }
    }

    float hemiFactor = normal.y * 0.5 + 0.5;
    vec3 skyAmbient = mix(vec3(0.035, 0.035, 0.04), vec3(0.075, 0.095, 0.13), hemiFactor);
    vec3 envAmbient = max(vec3(0.025), skyAmbient * 0.85);

    vec3 color = texelColor.rgb * tint * (envAmbient + lightAccum) + specular;

    float packedExposure = viewPos.w;
    bool useToneMap = packedExposure >= 0.0;
    float exposure = abs(packedExposure) > 0.001 ? abs(packedExposure) : 1.05;
    float bloomStrength = ambient.a > 0.0 ? ambient.a : 0.25;

    color *= exposure;
    vec3 bloomExcess = max(color - vec3(1.0), vec3(0.0));
    color += bloomExcess * bloomStrength * 0.45;

    if (useToneMap)
    {
        color = ACESFilm(color);
    }
    else
    {
        color = clamp(color, 0.0, 1.0);
    }

    float outAlpha = texelColor.a * fragColor.a;
    if (outAlpha < 0.1) discard;
    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), outAlpha);
}
