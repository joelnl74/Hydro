#version 450

// TEMPORARY(S1, removed in M4): the shared-depth spike's box. The unit box (QuadVertex layout; texture
// coordinates unused) moved by the push-constant transform, seen through the view's camera.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inTexIndex;

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

// Must match SpikeBoxPushConstants in Renderer2D.cpp (80 bytes).
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} push;

layout(location = 0) out vec4 fragColor;

void main()
{
	gl_Position = ubo.mvp * push.model * vec4(inPosition, 1.0);
	fragColor = inColor * push.color;
}
