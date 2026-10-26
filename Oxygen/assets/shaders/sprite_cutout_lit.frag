#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"
#include "lighting.glsl"

// sprite_cutout.frag under forward lighting (plan M6): standing cut-out sprites lit and fogged.
// Uses vertex.vert.

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;
layout(location = 5) in vec3 fragShadowPos;

layout(location = 0) out vec4 outColor;
layout(location = 1) out int outEntityID; // see fragment.frag

const float SpriteWrap = 1.5; // see fragment_lit.frag

void main()
{
	vec4 color = SampleTexture(int(texIndex), fragTexCoord) * fragColor;
	if (color.a < 0.5)
	{
		discard;
	}
	vec3 lit = color.rgb * ComputeLighting(fragWorldPos, QuadNormal(fragWorldPos), SpriteWrap, SunShadowSprite(fragShadowPos));
	outColor = vec4(ApplyFog(lit, fragWorldPos), 1.0);
	outEntityID = entityID;
}
