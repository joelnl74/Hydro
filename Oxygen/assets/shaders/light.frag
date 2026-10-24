#version 450

// A 2D light, drawn into the view's light buffer (additive) as one quad covering the light's radius.
// Uses vertex.vert, like the sprite quads.
//
//   fragTexCoord  -1..1 across the quad, so its length is 0 at the light and 1 at the edge of the radius
//   fragColor     the light's colour times its intensity
//   texIndex      the number of bands: 0 = a smooth fade, 2 or more = that many visible steps
//                 (set from RendererSettings::LightFalloff and LightBands)
//
// This is a plain file: change the curve below and save, and the running editor picks it up
// (Render Settings > Shader hot reload).

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

	// Bright near the light and exactly 0 at the radius, so there is no hard edge.
	float d2 = distanceToLight * distanceToLight;
	float falloff = (1.0 - d2) * (1.0 - d2) / (1.0 + 6.0 * d2);

	float bands = texIndex;
	if (bands >= 2.0)
	{
		falloff = floor(falloff * bands + 0.5) / bands;
	}

	outColor = vec4(fragColor.rgb * falloff, 1.0);
}
