cbuffer SkyboxParams : register(b0)
{
    float4x4 mvp;
    float4 sunDir;
    float4 postParams;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 fragDir  : NORMAL;
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

float4 main(PSInput input) : SV_TARGET
{
    float3 dir = normalize(input.fragDir);
    float3 sun = normalize(length(sunDir.xyz) > 0.001 ? sunDir.xyz : float3(0.4, 0.75, 0.5));

    float y = dir.y;
    float3 zenithColor  = float3(0.09, 0.21, 0.44);
    float3 horizonColor = float3(0.42, 0.58, 0.78);
    float3 groundColor  = float3(0.08, 0.085, 0.095);

    float3 skyColor;
    if (y >= 0.0)
    {
        float t = pow(saturate(y), 0.45);
        skyColor = lerp(horizonColor, zenithColor, t);
    }
    else
    {
        float t = pow(saturate(-y), 0.35);
        skyColor = lerp(horizonColor * 0.55, groundColor, t);
    }

    float sunDot = max(dot(dir, sun), 0.0);
    float horizonMask = smoothstep(-0.08, 0.12, y);
    float mieGlow = pow(sunDot, 10.0) * 0.28 + pow(sunDot, 96.0) * 0.65;
    float sunDisc = smoothstep(0.9985, 0.9996, sunDot) * 3.5;
    float3 sunColor = float3(1.0, 0.93, 0.82);
    skyColor += sunColor * (mieGlow + sunDisc) * horizonMask;

    float exposure = postParams.x > 0.001 ? postParams.x : 1.05;
    float useToneMap = postParams.y;
    float bloomStrength = max(postParams.z, 0.0);

    float3 color = skyColor * exposure;
    float3 bloomExcess = max(color - float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0));
    color += bloomExcess * bloomStrength * 0.35;

    if (useToneMap > 0.5)
    {
        color = ACESFilm(color);
    }
    else
    {
        color = saturate(color);
    }

    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return float4(color, 1.0);
}
