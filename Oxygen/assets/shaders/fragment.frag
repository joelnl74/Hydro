#version 450

layout (binding = 1) uniform sampler2D inTextures[32];

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float texIndex;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(inTextures[int(texIndex)], fragTexCoord);
	outColor *= vec4(fragColor.x, fragColor.y, fragColor.z, 1);
}
