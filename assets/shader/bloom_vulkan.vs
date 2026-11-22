#version 450

layout(push_constant) uniform ScreenData {
    vec2 screenSize;
} screen;

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

void main()
{
    vec2 ndc = (aPosition / screen.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, ndc.y, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
