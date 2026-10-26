#version 450

#extension GL_GOOGLE_include_directive : require

#include "frame_lighting.glsl"

// Mesh shadow casters (R14, Renderer3D::RecordShadowCasters): MeshVertex seen from the sun
// (lighting.shadowMatrix), the object in push constants as in mesh.vert. UV and colour go on for the
// material's alpha cut-off.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

// Must match ObjectPushConstants in VulkanGPUData.h (std430, 80 bytes).
layout(push_constant) uniform ObjectPushConstants {
    mat4 transform;
    int entityID;
    uint materialIndex;
    uvec2 padding;
} object;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

void main()
{
	gl_Position = lighting.shadowMatrix * (object.transform * vec4(inPosition, 1.0));
	fragUV = inUV;
	fragColor = inColor;
}
