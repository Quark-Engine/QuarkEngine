#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform int useTexture;
uniform sampler2D shadowMap;
uniform mat4 lightVP;
uniform int shadowMapResolution;
uniform vec3 shadowLightDir;
uniform int shadowsEnabled;

out vec4 finalColor;

#define MAX_LIGHTS 4
#define LIGHT_POINT       0
#define LIGHT_DIRECTIONAL 1
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

float ShadowCalc(vec3 fragPos, vec3 normal) {
    if (shadowsEnabled != 1) return 0.0;

    vec4 fragPosLS = lightVP * vec4(fragPos, 1.0);
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 0.0;

    float bias = max(0.0002 * (1.0 - dot(normal, normalize(shadowLightDir))), 0.00002) + 0.00001;
    int shadowCounter = 0;
    vec2 texelSize = vec2(1.0 / float(shadowMapResolution));

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float sampleDepth = texture(shadowMap, proj.xy + texelSize * vec2(x, y)).r;
            if (proj.z - bias > sampleDepth) shadowCounter++;
        }
    }
    return float(shadowCounter) / 9.0;
}

void main() {
    vec4 texelColor = vec4(1.0);
    if (useTexture == 1)
        texelColor = texture(texture0, fragTexCoord);

    vec4 tint = colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 lightAccum = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (lights[i].enabled != 1) continue;

        vec3 lightDir;
        float attenuation = 1.0;

        if (lights[i].type == LIGHT_DIRECTIONAL) {
            lightDir = normalize(lights[i].position - lights[i].target);
            attenuation = 1.0;
        }
        else if (lights[i].type == LIGHT_POINT) {
            lightDir = normalize(lights[i].position - fragPosition);
            float dist = length(lights[i].position - fragPosition);
            attenuation = clamp(1.0 - (dist / lights[i].range), 0.0, 1.0);
            attenuation *= attenuation;
        }
        else if (lights[i].type == LIGHT_SPOT) {
            lightDir = normalize(lights[i].position - fragPosition);
            float dist = length(lights[i].position - fragPosition);
            attenuation = clamp(1.0 - (dist / lights[i].range), 0.0, 1.0);
            attenuation *= attenuation;

            vec3 spotDir = normalize(lights[i].position - lights[i].target);
            float theta = dot(lightDir, spotDir);
            float cutoff = cos(radians(lights[i].spotAngle));
            if (theta < cutoff) attenuation = 0.0;
            else attenuation *= smoothstep(cutoff, cutoff + 0.05, theta);
        }
        else if (lights[i].type == LIGHT_AREA) {
            lightDir = normalize(lights[i].position - fragPosition);
            float dist = length(lights[i].position - fragPosition);
            attenuation = clamp(1.0 - (dist / lights[i].range), 0.0, 1.0);
            attenuation = sqrt(attenuation);
        }

        float NdotL = max(dot(normal, lightDir), 0.0);
        lightAccum += lights[i].color.rgb * NdotL * attenuation * lights[i].intensity;

        if (NdotL > 0.0) {
            float spec = pow(max(dot(viewD, reflect(-lightDir, normal)), 0.0), 16.0);
            specular += spec * attenuation * lights[i].intensity * lights[i].color.rgb;
        }
    }

    float shadow = ShadowCalc(fragPosition, normal);
    vec3 ambientLight = ambient.rgb;
    vec3 color = texelColor.rgb * tint.rgb * (ambientLight + lightAccum * (1.0 - shadow * 0.8)) + specular * (1.0 - shadow);
    color += texelColor.rgb * tint.rgb * (ambientLight / 10.0);
    color += emissionColor * emissionPower;

    finalColor = vec4(color, texelColor.a * tint.a);
    finalColor = pow(finalColor, vec4(1.0 / 2.2));
    if (finalColor.a < 0.1) discard;
}
