#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec4 fragPosLightSpace[4];

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform int useTexture;

uniform sampler2D shadowMap[4];
uniform int shadowMapActive[4];

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

float sampleShadow(sampler2D map, vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float bias = mix(0.005, 0.0005, cosTheta);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(map, 0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(map, proj.xy + vec2(x, y) * texelSize).r;
            shadow += (proj.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }

    shadow /= 9.0;
    return 1.0 - shadow;
}

void main()
{
    vec4 texelColor = vec4(1.0);

    if (useTexture == 1)
        texelColor = texture(texture0, fragTexCoord);

    vec4 tint       = colDiffuse * fragColor;
    vec3 normal     = normalize(fragNormal);
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

        float shadowFactor = 1.0;

        if (shadowMapActive[i] == 1)
            shadowFactor = sampleShadow(shadowMap[i], fragPosLightSpace[i], normal, lightDir);

        float directLight = max(dot(normal, lightDir), 0.0);
        float wrappedLight = clamp((dot(normal, lightDir) + 0.28) / 1.28, 0.0, 1.0);
        float diffuse = max(directLight, wrappedLight * 0.25);

        vec3 bounce = lights[i].color.rgb * attenuation * lightEnergy * 0.08;
        lightAccum += (lights[i].color.rgb * diffuse * attenuation * lightEnergy * shadowFactor) + bounce;

        if (directLight > 0.0)
        {
            float spec = pow(max(dot(viewD, reflect(-lightDir, normal)), 0.0), 24.0);
            specular  += spec * attenuation * lightEnergy * shadowFactor * 0.2 * lights[i].color.rgb;
        }
    }

    vec3 color = texelColor.rgb * tint.rgb * (ambient.rgb + lightAccum) + specular;
    color += emissionColor * emissionPower;

    finalColor = vec4(color, texelColor.a * tint.a);
    finalColor = pow(finalColor, vec4(1.0 / 2.2));

    if (finalColor.a < 0.1) discard;
}