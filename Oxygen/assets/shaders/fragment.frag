#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;
layout(location = 3) flat in int entityID;

layout(location = 0) out vec4 outColor;
// The Editor view's entity-ID attachment (R32_SINT, never blended). Views without it ignore this output,
// and pipelines for quads that aren't pickable don't write it (write mask 0).
layout(location = 1) out int outEntityID;

void main()
{
	vec4 color = SampleTexture(int(texIndex), fragTexCoord) * fragColor;
	// Alpha exactly 0 adds nothing under alpha blending, so dropping the pixel leaves the colour as it was;
	// it only keeps the pixel from writing its entity ID, so clicks pass through a sprite's empty parts.
	if (color.a == 0.0)
	{
		discard;
	}
	outColor = color;
	outEntityID = entityID;
}
