#version 450

#define QUARK_VULKAN_GPU_MESH_TRANSFORM 1

layout(location = 0) in vec4 aPosition;
layout(location = 3) in vec4 aNormal;

layout(push_constant) uniform MeshTransform {
    mat4 modelViewProjection;
    mat4 model;
    vec4 materialColor;
    int objectSpaceTransform;
    int _padding0;
    int _padding1;
    int _padding2;
} meshTransform;

layout(location = 0) out vec3 vNormal;

void main()
{
    if (meshTransform.objectSpaceTransform != 0) {
        gl_Position = meshTransform.modelViewProjection * aPosition;
        vNormal = normalize(mat3(meshTransform.model) * aNormal.xyz);
    } else {
        gl_Position = aPosition;
        vNormal = normalize(aNormal.xyz);
    }
}
