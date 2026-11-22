cbuffer Params : register(b0)
{
    float4 bloomParams;
};

Texture2D textureMap : register(t0);
SamplerState textureSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
};

float3 ExtractBright(float3 color, float threshold)
{
    float brightness = max(color.r, max(color.g, color.b));
    float knee = 0.25;
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / max(4.0 * knee, 0.0001);
    float contribution = max(soft, brightness - threshold) / max(brightness, 0.0001);
    return color * max(contribution, 0.0);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = saturate(input.texCoord);
    float2 texel = bloomParams.xy;
    float threshold = bloomParams.z;
    float mode = bloomParams.w;

    if (mode < 0.5)
    {
        float2 o = texel * 1.25;
        float3 sum = ExtractBright(textureMap.Sample(textureSampler, uv).rgb, threshold) * 0.24;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2(-o.x,  0.0))).rgb, threshold) * 0.12;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2( o.x,  0.0))).rgb, threshold) * 0.12;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2( 0.0, -o.y))).rgb, threshold) * 0.12;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2( 0.0,  o.y))).rgb, threshold) * 0.12;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2(-o.x, -o.y))).rgb, threshold) * 0.07;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2( o.x, -o.y))).rgb, threshold) * 0.07;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2(-o.x,  o.y))).rgb, threshold) * 0.07;
        sum += ExtractBright(textureMap.Sample(textureSampler, saturate(uv + float2( o.x,  o.y))).rgb, threshold) * 0.07;
        return float4(sum * input.color.rgb, 1.0);
    }
    else
    {
        float2 r1 = texel * 1.5;
        float2 r2 = texel * 3.25;
        float3 sum = textureMap.Sample(textureSampler, uv).rgb * 0.20;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2(-r1.x, -r1.y))).rgb * 0.10;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( r1.x, -r1.y))).rgb * 0.10;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2(-r1.x,  r1.y))).rgb * 0.10;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( r1.x,  r1.y))).rgb * 0.10;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2(-r2.x,  0.0))).rgb * 0.06;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( r2.x,  0.0))).rgb * 0.06;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( 0.0, -r2.y))).rgb * 0.06;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( 0.0,  r2.y))).rgb * 0.06;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2(-r2.x, -r2.y) * 0.707)).rgb * 0.04;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( r2.x, -r2.y) * 0.707)).rgb * 0.04;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2(-r2.x,  r2.y) * 0.707)).rgb * 0.04;
        sum += textureMap.Sample(textureSampler, saturate(uv + float2( r2.x,  r2.y) * 0.707)).rgb * 0.04;
        return float4(sum * input.color.rgb, 1.0);
    }
}
