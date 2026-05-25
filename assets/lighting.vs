#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform mat4 normalMatrix;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    fragPosition = vec3(model * vec4(aPosition, 1.0));
    fragTexCoord = aTexCoord0;
    fragColor = vec4(1.0);
    fragNormal = normalize((normalMatrix * vec4(aNormal, 0.0)).xyz);
    gl_Position = projection * view * vec4(fragPosition, 1.0);
}
