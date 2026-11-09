#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"

// Sun shadow casters (R14): only depth is written. Pixels a cut-out sprite drops (alpha under 0.5, as
// sprite_cutout.frag) cast nothing, so leaves throw dappled shadows. Uses shadow_caster.vert.

layout(location = 0) in float fragAlpha;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;

void main()
{
	if (SampleTexture(int(texIndex), fragTexCoord).a * fragAlpha < 0.5)
	{
		discard;
	}
}
