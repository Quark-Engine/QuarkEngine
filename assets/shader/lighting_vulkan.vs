#version 450

#define QUARK_VULKAN_GPU_MESH_TRANSFORM 1

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec2 aTexCoord0;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aNormal;
layout(location = 4) in vec4 aWorldPosition;
layout(location = 5) in vec4 aTangent;

struct InstanceTransform {
    mat4 modelViewProjection;
    mat4 model;
};

layout(std430, set = 0, binding = 11) readonly buffer InstanceTransforms {
    InstanceTransform instances[];
};

layout(push_constant) uniform MeshTransform {
    mat4 modelViewProjection;
    mat4 model;
    vec4 materialColor;
    int objectSpaceTransform;
    int useInstanceBuffer;
    int _padding1;
    int _padding2;
} meshTransform;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPosition;
layout(location = 4) out vec3 fragTangent;

void main()
{
    fragTexCoord = aTexCoord0;
    if (meshTransform.objectSpaceTransform != 0) {
        mat4 activeMvp = (meshTransform.useInstanceBuffer != 0)
            ? instances[gl_InstanceIndex].modelViewProjection
            : meshTransform.modelViewProjection;
        mat4 activeModel = (meshTransform.useInstanceBuffer != 0)
            ? instances[gl_InstanceIndex].model
            : meshTransform.model;

        gl_Position  = activeMvp * aPosition;
        fragColor    = aColor * meshTransform.materialColor;
        fragNormal   = normalize(mat3(activeModel) * aNormal.xyz);
        fragPosition = (activeModel * aPosition).xyz;
        fragTangent  = normalize(mat3(activeModel) * aTangent.xyz);
    } else {
        gl_Position  = aPosition;
        fragColor    = aColor;
        fragNormal   = normalize(aNormal.xyz);
        fragPosition = aWorldPosition.xyz;
        fragTangent  = normalize(aTangent.xyz);
    }
}
