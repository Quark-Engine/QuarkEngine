#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragTangent;
uniform sampler2D texture0;
uniform sampler2D normalMap;
uniform sampler2D shadowMaps[4];
uniform mat4 lightViews[4];
uniform mat4 lightProjections[4];
uniform vec4 colDiffuse;
uniform int useTexture;

out vec4 finalColor;

#define MAX_LIGHTS        4
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_AREA        3

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
    float intensity;
    float range;
    float spotAngle;
};

uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;
uniform vec3 emissionColor;
uniform float emissionPower;
uniform int shadowsEnabled;
uniform float shadowBias;
uniform int shadowFilterQuality;
uniform vec4 postParams;

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

float shadow_factor(int lightIndex, vec3 worldPosition, vec3 normal, vec3 lightDir)
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
    if (shadowFilterQuality <= 0)
    {
        shadow = sampleShadowBilinear(lightIndex, projected.xy, mapSize, currentDepth, baseDepthBias);
    }
    else
    {
        int tapCount = (shadowFilterQuality == 1) ? 8 : 16;
        float spreadTexels = (shadowFilterQuality == 1) ? 1.65 : 2.75;
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
    vec4 texelColor = vec4(1.0);

    if (useTexture == 1)
        texelColor = texture(texture0, fragTexCoord);

    vec4 tint       = colDiffuse * fragColor;
    vec3 normal     = normalize(fragNormal);
    vec3 tangent    = normalize(fragTangent);
    if (abs(dot(normal, tangent)) < 0.999f)
    {
        vec3 bitangent = normalize(cross(normal, tangent));
        vec3 mapNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        normal = normalize(mat3(tangent, bitangent, normal) * mapNormal);
    }
    vec3 viewD      = normalize(viewPos - fragPosition);
    vec3 lightAccum = vec3(0.0);
    vec3 specular   = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled != 1) continue;

        vec3 lightDir = vec3(0.0);
        float attenuation = 1.0;
        float lightEnergy = min(lights[i].intensity, 4.0);

        if (lights[i].type == LIGHT_DIRECTIONAL)
        {
            lightDir = normalize(lights[i].position - lights[i].target);
        }

        else if (lights[i].type == LIGHT_POINT)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);
            lightDir = toLight / dist;

            float range = max(lights[i].range, 0.001);
            attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation *= attenuation;
        }

        else if (lights[i].type == LIGHT_SPOT)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);

            lightDir = toLight / dist;
            float range = max(lights[i].range, 0.001);
            attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
            attenuation *= attenuation;

            vec3 spotDir = normalize(lights[i].position - lights[i].target);
            float theta = dot(lightDir, spotDir);
            float cutoff = cos(radians(lights[i].spotAngle));

            if (theta < cutoff)
                attenuation = 0.0;
            else
                attenuation *= smoothstep(cutoff, cutoff + 0.05, theta);
        }

        else if (lights[i].type == LIGHT_AREA)
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = max(length(toLight), 0.35);

            lightDir = toLight / dist;

            float range = max(lights[i].range, 0.001);
            attenuation = sqrt(clamp(1.0 - dist / range, 0.0, 1.0));
        }

        float directLight = max(dot(normal, lightDir), 0.0);
        float shadow = (shadowsEnabled == 1 && directLight > 0.0001)
            ? shadow_factor(i, fragPosition, normalize(fragNormal), lightDir)
            : 0.0;
        float wrappedLight = clamp((dot(normal, lightDir) + 0.50) / 1.50, 0.0, 1.0);
        float directTerm = directLight * (1.0 - shadow);
        float wrapTerm = wrappedLight * 0.30 * (1.0 - shadow * clamp(directLight * 4.0, 0.0, 1.0));
        float diffuse = max(directTerm, wrapTerm);

        vec3 bounce = lights[i].color.rgb * attenuation * lightEnergy * 0.08;
        lightAccum += (lights[i].color.rgb * diffuse * attenuation * lightEnergy) + bounce;

        if (directLight > 0.0)
        {
            float spec = pow(max(dot(viewD, reflect(-lightDir, normal)), 0.0), 24.0);
            specular  += spec * attenuation * lightEnergy * 0.2 * lights[i].color.rgb
                * (1.0 - shadow);
        }
    }

    float hemiFactor = normal.y * 0.5 + 0.5;
    vec3 skyAmbient = mix(vec3(0.035, 0.035, 0.04), vec3(0.075, 0.095, 0.13), hemiFactor);
    vec3 envAmbient = max(ambient.rgb, skyAmbient * (postParams.w > 0.5 ? 1.0 : 0.45));

    vec3 color = texelColor.rgb * tint.rgb * (envAmbient + lightAccum) + specular;
    color += emissionColor * emissionPower;

    float exposure = postParams.x > 0.001 ? postParams.x : 1.05;
    float useToneMap = postParams.x > 0.001 ? postParams.y : 1.0;
    float bloomStrength = postParams.x > 0.001 ? max(postParams.z, 0.0) : 0.25;

    color *= exposure;
    vec3 bloomExcess = max(color - vec3(1.0), vec3(0.0));
    color += bloomExcess * bloomStrength * 0.45;

    if (useToneMap > 0.5)
    {
        color = ACESFilm(color);
    }
    else
    {
        color = clamp(color, 0.0, 1.0);
    }

    float outAlpha = texelColor.a * tint.a;
    if (outAlpha < 0.1) discard;
    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), outAlpha);
}