#version 330 core

in vec2 vTexCoord;
in vec4 vColor;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec4 bloomParams;

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
    vec2 texel = bloomParams.xy;
    float threshold = bloomParams.z;
    float mode = bloomParams.w;

    if (mode < 0.5)
    {
        vec2 o = texel * 1.25;
        vec3 sum = ExtractBright(texture(uTexture, vTexCoord).rgb, threshold) * 0.24;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2(-o.x,  0.0)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2( o.x,  0.0)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2( 0.0, -o.y)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2( 0.0,  o.y)).rgb, threshold) * 0.12;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2(-o.x, -o.y)).rgb, threshold) * 0.07;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2( o.x, -o.y)).rgb, threshold) * 0.07;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2(-o.x,  o.y)).rgb, threshold) * 0.07;
        sum += ExtractBright(texture(uTexture, vTexCoord + vec2( o.x,  o.y)).rgb, threshold) * 0.07;
        fragColor = vec4(sum * vColor.rgb, 1.0);
    }
    else
    {
        vec2 r1 = texel * 1.5;
        vec2 r2 = texel * 3.25;
        vec3 sum = texture(uTexture, vTexCoord).rgb * 0.20;
        sum += texture(uTexture, vTexCoord + vec2(-r1.x, -r1.y)).rgb * 0.10;
        sum += texture(uTexture, vTexCoord + vec2( r1.x, -r1.y)).rgb * 0.10;
        sum += texture(uTexture, vTexCoord + vec2(-r1.x,  r1.y)).rgb * 0.10;
        sum += texture(uTexture, vTexCoord + vec2( r1.x,  r1.y)).rgb * 0.10;
        sum += texture(uTexture, vTexCoord + vec2(-r2.x,  0.0)).rgb * 0.06;
        sum += texture(uTexture, vTexCoord + vec2( r2.x,  0.0)).rgb * 0.06;
        sum += texture(uTexture, vTexCoord + vec2( 0.0, -r2.y)).rgb * 0.06;
        sum += texture(uTexture, vTexCoord + vec2( 0.0,  r2.y)).rgb * 0.06;
        sum += texture(uTexture, vTexCoord + vec2(-r2.x, -r2.y) * 0.707).rgb * 0.04;
        sum += texture(uTexture, vTexCoord + vec2( r2.x, -r2.y) * 0.707).rgb * 0.04;
        sum += texture(uTexture, vTexCoord + vec2(-r2.x,  r2.y) * 0.707).rgb * 0.04;
        sum += texture(uTexture, vTexCoord + vec2( r2.x,  r2.y) * 0.707).rgb * 0.04;
        fragColor = vec4(sum * vColor.rgb, 1.0);
    }
}
