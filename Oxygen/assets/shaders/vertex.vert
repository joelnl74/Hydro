#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inTexIndex;
// The quad's entity (-1 = none), for the Editor view's entity-ID attachment.
layout(location = 4) in int inEntityID;

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
// Same for all four corners of a quad; flat so it is never interpolated.
layout(location = 2) flat out float texIndex;
// An integer: never interpolated.
layout(location = 3) flat out int entityID;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
	fragTexCoord = inTexCoord;
	texIndex = inTexIndex;
	entityID = inEntityID;
}
