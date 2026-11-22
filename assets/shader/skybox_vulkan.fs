#version 450

layout(set = 0, binding = 4) uniform SkyboxBlock {
    vec4 sunDir;
    vec4 postParams;
};

layout(location = 0) in vec3 fragDir;
layout(location = 0) out vec4 finalColor;

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 dir = normalize(fragDir);
    vec3 sun = normalize(length(sunDir.xyz) > 0.001 ? sunDir.xyz : vec3(0.4, 0.75, 0.5));

    float y = dir.y;
    vec3 zenithColor  = vec3(0.09, 0.21, 0.44);
    vec3 horizonColor = vec3(0.42, 0.58, 0.78);
    vec3 groundColor  = vec3(0.08, 0.085, 0.095);

    vec3 skyColor;
    if (y >= 0.0)
    {
        float t = pow(clamp(y, 0.0, 1.0), 0.45);
        skyColor = mix(horizonColor, zenithColor, t);
    }
    else
    {
        float t = pow(clamp(-y, 0.0, 1.0), 0.35);
        skyColor = mix(horizonColor * 0.55, groundColor, t);
    }

    float sunDot = max(dot(dir, sun), 0.0);
    float horizonMask = smoothstep(-0.08, 0.12, y);
    float mieGlow = pow(sunDot, 10.0) * 0.28 + pow(sunDot, 96.0) * 0.65;
    float sunDisc = smoothstep(0.9985, 0.9996, sunDot) * 3.5;
    vec3 sunColor = vec3(1.0, 0.93, 0.82);
    skyColor += sunColor * (mieGlow + sunDisc) * horizonMask;

    float exposure = postParams.x > 0.001 ? postParams.x : 1.05;
    float useToneMap = postParams.y;
    float bloomStrength = max(postParams.z, 0.0);

    vec3 color = skyColor * exposure;
    vec3 bloomExcess = max(color - vec3(1.0), vec3(0.0));
    color += bloomExcess * bloomStrength * 0.35;

    if (useToneMap > 0.5)
    {
        color = ACESFilm(color);
    }
    else
    {
        color = clamp(color, 0.0, 1.0);
    }

    color = pow(color, vec3(1.0 / 2.2));
    finalColor = vec4(color, 1.0);
}
