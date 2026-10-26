#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"
#include "lighting.glsl"

// fragment.frag under forward lighting (plan M6): translucent and decal sprites lit by the view's sun,
// ambient and point/spot lights, then fogged. Uses vertex.vert.

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;
layout(location = 5) in vec3 fragShadowPos;
layout(location = 6) flat in vec3 fragEmission;

layout(location = 0) out vec4 outColor;
layout(location = 1) out int outEntityID; // see fragment.frag

// Sprites are paper: lit from behind they still show (the wrap), so a character walking away from the
// sun isn't a black cut-out.
const float SpriteWrap = 1.5;

void main()
{
	vec4 color = SampleTexture(int(texIndex), fragTexCoord) * fragColor;
	if (color.a == 0.0)
	{
		discard;
	}
	vec3 lit = color.rgb * ComputeLighting(fragWorldPos, QuadNormal(fragWorldPos), SpriteWrap, SunShadowSprite(fragShadowPos));
	// Emission (plan P3) after the lighting, so a flame isn't darkened by night or shadow; fogged like the rest.
	lit += color.rgb * fragEmission;
	outColor = vec4(ApplyFog(lit, fragWorldPos), color.a);
	outEntityID = entityID;
}
