#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;

layout(location = 0) out vec4 outColor;

// Cut-out sprites (shared depth): drawn with depth test and write, so a pixel either covers what is behind
// it or is discarded before it writes depth. Pixel art has hard alpha edges; 0.5 keeps soft ones tidy.
void main()
{
	vec4 color = SampleTexture(int(texIndex), fragTexCoord) * fragColor;
	if (color.a < 0.5)
	{
		discard;
	}
	outColor = color;
}
