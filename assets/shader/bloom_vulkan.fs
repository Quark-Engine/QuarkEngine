#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

vec3 ExtractBright(vec3 color, float threshold)
{
    float brightness = max(color.r, max(color.g, color.b));
    float knee = 0.25;
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / max(4.0 * knee, 0.0001);
    float contribution = max(soft, brightness - threshold) / max(brightness, 0.0001);
    return color * max(contribution, 0.0);
}

void main()
{
    vec2 uv = clamp(vTexCoord, vec2(0.0), vec2(1.0));
    vec2 texSize = max(vec2(textureSize(texture0, 0)), vec2(1.0));
    vec2 texel = 1.0 / texSize;
    float threshold = 0.72;

    if (vColor.a < 0.75)
    {
        vec2 o = texel * 1.25;
        vec3 sum = ExtractBright(texture(texture0, uv).rgb, threshold) * 0.24;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2(-o.x,  0.0), 0.0, 1.0)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2( o.x,  0.0), 0.0, 1.0)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2( 0.0, -o.y), 0.0, 1.0)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2( 0.0,  o.y), 0.0, 1.0)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2(-o.x, -o.y), 0.0, 1.0)).rgb, threshold) * 0.07;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2( o.x, -o.y), 0.0, 1.0)).rgb, threshold) * 0.07;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2(-o.x,  o.y), 0.0, 1.0)).rgb, threshold) * 0.07;
        sum += ExtractBright(texture(texture0, clamp(uv + vec2( o.x,  o.y), 0.0, 1.0)).rgb, threshold) * 0.07;
        fragColor = vec4(sum * vColor.rgb, 1.0);
    }
    else
    {
        vec2 r1 = texel * 1.5;
        vec2 r2 = texel * 3.25;
        vec3 sum = texture(texture0, uv).rgb * 0.20;
        sum += texture(texture0, clamp(uv + vec2(-r1.x, -r1.y), 0.0, 1.0)).rgb * 0.10;
        sum += texture(texture0, clamp(uv + vec2( r1.x, -r1.y), 0.0, 1.0)).rgb * 0.10;
        sum += texture(texture0, clamp(uv + vec2(-r1.x,  r1.y), 0.0, 1.0)).rgb * 0.10;
        sum += texture(texture0, clamp(uv + vec2( r1.x,  r1.y), 0.0, 1.0)).rgb * 0.10;
        sum += texture(texture0, clamp(uv + vec2(-r2.x,  0.0), 0.0, 1.0)).rgb * 0.06;
        sum += texture(texture0, clamp(uv + vec2( r2.x,  0.0), 0.0, 1.0)).rgb * 0.06;
        sum += texture(texture0, clamp(uv + vec2( 0.0, -r2.y), 0.0, 1.0)).rgb * 0.06;
        sum += texture(texture0, clamp(uv + vec2( 0.0,  r2.y), 0.0, 1.0)).rgb * 0.06;
        sum += texture(texture0, clamp(uv + vec2(-r2.x, -r2.y) * 0.707, 0.0, 1.0)).rgb * 0.04;
        sum += texture(texture0, clamp(uv + vec2( r2.x, -r2.y) * 0.707, 0.0, 1.0)).rgb * 0.04;
        sum += texture(texture0, clamp(uv + vec2(-r2.x,  r2.y) * 0.707, 0.0, 1.0)).rgb * 0.04;
        sum += texture(texture0, clamp(uv + vec2( r2.x,  r2.y) * 0.707, 0.0, 1.0)).rgb * 0.04;
        fragColor = vec4(sum * vColor.rgb, 1.0);
    }
}
