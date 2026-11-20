#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in mat4 aInstanceModel;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform mat4 normalMatrix;
uniform int useInstancing;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec3 fragTangent;

void main()
{
    mat4 activeModel = (useInstancing != 0) ? aInstanceModel : model;
    mat3 activeNormal = (useInstancing != 0) ? transpose(inverse(mat3(activeModel))) : mat3(normalMatrix);

    fragPosition = vec3(activeModel * vec4(aPosition, 1.0));
    fragTexCoord = aTexCoord0;
    fragColor    = vec4(1.0);
    fragNormal   = normalize(activeNormal * aNormal);
    fragTangent  = normalize(activeNormal * aTangent);

    gl_Position = projection * view * vec4(fragPosition, 1.0);
}
