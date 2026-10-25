#version 450

// Static meshes (Renderer3D, plan M4): MeshVertex, the view's camera in set 0, the object in push constants.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

// Set 0 is the view's (the quad shader's layout); only the camera is read here.
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} camera;

// Must match ObjectPushConstants in VulkanGPUData.h (std430, 80 bytes).
layout(push_constant) uniform ObjectPushConstants {
    mat4 transform;
    int entityID;
    uint materialIndex;
    uvec2 padding;
} object;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec3 fragNormal; // world space, for forward lighting (mesh.frag)
layout(location = 3) flat out int entityID;
layout(location = 4) out vec3 fragWorldPos; // for forward lighting and fog (mesh.frag)

void main()
{
	vec4 world = object.transform * vec4(inPosition, 1.0);
	gl_Position = camera.mvp * world;
	fragWorldPos = world.xyz;
	fragUV = inUV;
	fragColor = inColor;
	fragNormal = normalize(mat3(object.transform) * inNormal);
	entityID = object.entityID;
}
