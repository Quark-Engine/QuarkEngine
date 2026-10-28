#version 450

layout(location = 0) in vec3 vNormal;

layout(location = 0) out vec4 finalColor;

void main()
{
    float depth = gl_FragCoord.z;
    vec3 normal = normalize(vNormal);
    finalColor = vec4(depth, normal.xy * 0.5 + 0.5, 1.0);
}
