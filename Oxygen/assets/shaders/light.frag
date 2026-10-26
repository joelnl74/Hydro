#version 450

// A 2D light, drawn into the view's light buffer (additive) as one quad covering the light's radius.
// Uses vertex.vert, like the sprite quads.
//
//   fragTexCoord  -1..1 across the quad, so its length is 0 at the light and 1 at the edge of the radius
//   fragColor     the light's colour times its intensity
//   texIndex      the number of bands: 0 = a smooth fade, 2 or more = that many visible steps
//                 (set from RendererSettings::LightFalloff and LightBands)
//
// This is a plain file: change the curve (light_falloff.glsl) and save, and the running editor picks it up
// (Render Settings > Shader hot reload).

#extension GL_GOOGLE_include_directive : require

#include "light_falloff.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;

layout(location = 0) out vec4 outColor;

void main()
{
	float distanceToLight = length(fragTexCoord);
	if (distanceToLight >= 1.0)
	{
		// The corners of the quad outside the circle.
		discard;
	}

	// The curve is in light_falloff.glsl (shared with forward lighting).
	float falloff = LightFalloff(distanceToLight, texIndex);

	outColor = vec4(fragColor.rgb * falloff, 1.0);
}
