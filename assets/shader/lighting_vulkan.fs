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

float unpackDepth24(vec3 rgb)
{
    return dot(rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0)) * SHADOW_DEPTH_RANGE;
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
    float tapSlopeBias = currentDepth * baseBias * (1.0 + 3.0 * slopeFactor) + 0.006;

    ivec2 mapSize = textureSize(shadowMaps[lightIndex], 0);
    vec2 texelCoord = projected.xy * vec2(mapSize) - vec2(0.5);
    ivec2 baseTexel = ivec2(floor(texelCoord));
    vec2 fracCoord = texelCoord - vec2(baseTexel);
    float shadow = 0.0;

    int radius = filterQuality == 0 ? 0 : filterQuality == 1 ? 1 : 2;
    int sampleWidth = radius * 2 + 1;
    float totalWeight = float(sampleWidth * sampleWidth);
    for (int x = -radius; x <= radius + 1; x++)
    {
        float wx = (x == -radius) ? (1.0 - fracCoord.x) : ((x == radius + 1) ? fracCoord.x : 1.0);
        for (int y = -radius; y <= radius + 1; y++)
        {
            float wy = (y == -radius) ? (1.0 - fracCoord.y) : ((y == radius + 1) ? fracCoord.y : 1.0);
            ivec2 texel = clamp(baseTexel + ivec2(x, y), ivec2(0), mapSize - 1);
            float closestDepth = unpackDepth24(texelFetch(shadowMaps[lightIndex], texel, 0).rgb);
            float tapDist = abs(float(x) - fracCoord.x) + abs(float(y) - fracCoord.y);
            float bias = baseDepthBias + tapSlopeBias * tapDist;
            shadow += (currentDepth - bias > closestDepth ? 1.0 : 0.0) * (wx * wy);
        }
    }

    float edgeFade = smoothstep(0.0, 0.05, projected.x) *
                     smoothstep(1.0, 0.95, projected.x) *
                     smoothstep(0.0, 0.05, projected.y) *
                     smoothstep(1.0, 0.95, projected.y);
    return (shadow / totalWeight) * edgeFade;
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

    vec3 color = texelColor.rgb * tint * (vec3(0.025) + lightAccum) + specular;
    vec4 result = vec4(color, texelColor.a * fragColor.a);
    result = pow(result, vec4(1.0 / 2.2));

    if (result.a < 0.1) discard;
    finalColor = result;
}
