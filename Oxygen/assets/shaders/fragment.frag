#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = SampleTexture(int(texIndex), fragTexCoord) * fragColor;
}
