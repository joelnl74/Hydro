#version 450

// A 2D blob shadow, drawn into the view's light buffer after the lights (BlendMode::Darken: the light
// buffer is multiplied by 1 - the alpha written here). Uses vertex.vert, like the sprite quads and lights.
//
//   fragTexCoord  -1..1 across the quad, so its length is 0 at the centre and 1 at the edge of the ellipse
//   fragColor.a   the shadow's strength: 0 = none, 1 = fully dark at the centre
//
// This is a plain file: change the curve below and save, and the running editor picks it up
// (Render Settings > Shader hot reload).

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;

layout(location = 0) out vec4 outColor;

void main()
{
	float distanceToCentre = length(fragTexCoord);
	if (distanceToCentre >= 1.0)
	{
		// The corners of the quad outside the ellipse.
		discard;
	}

	// Darkest in the middle, exactly 0 at the edge, so there is no hard outline. For a harder shadow,
	// raise the exponent or quantise `softness` like light.frag does with its bands.
	float softness = 1.0 - smoothstep(0.0, 1.0, distanceToCentre);

	outColor = vec4(0.0, 0.0, 0.0, fragColor.a * softness);
}
