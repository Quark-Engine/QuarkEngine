#version 450

#define SHADOW_DEPTH_RANGE 1000.0

layout(location = 0) in vec3 vNormal;

layout(location = 0) out vec4 finalColor;

vec3 packDepth24(float value)
{
    value = clamp(value, 0.0, 0.99999);
    vec3 enc = fract(value * vec3(1.0, 255.0, 65025.0));
    enc -= enc.yzz * vec3(1.0 / 255.0, 1.0 / 255.0, 0.0);
    return enc;
}

void main()
{
    float linearDepth = (1.0 / gl_FragCoord.w) / SHADOW_DEPTH_RANGE;
    finalColor = vec4(packDepth24(linearDepth), 1.0);
}
