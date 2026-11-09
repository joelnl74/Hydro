#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"
#include "fade_dither.glsl"
#include "lighting.glsl"
#include "surface_derivatives.glsl"

// sprite_fade.frag under forward lighting (sprite_cutout_lit.frag with the screen-door fade). Uses vertex.vert.

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;
layout(location = 5) in vec3 fragShadowPos;
layout(location = 6) flat in vec3 fragEmission;

layout(location = 0) out vec4 outColor;
layout(location = 1) out int outEntityID; // see fragment.frag

const float SpriteWrap = 1.5; // see fragment_lit.frag

void main()
{
	// Before the discard: how the positions change across the screen, for the pixel snap.
	SurfaceDerivatives surface = GetSurfaceDerivatives(fragWorldPos, fragTexCoord);
	SurfaceDerivatives shadowSurface = GetSurfaceDerivatives(fragShadowPos, fragTexCoord);
	// The quad's alpha is the fade (SpriteRendererComponent::Color.a times Opacity, SceneRenderer): the texture's
	// alpha alone cuts out, the quad's dithers.
	vec4 texel = SampleTexture(int(texIndex), fragTexCoord);
	if (texel.a < 0.5 || FadeDiscards(fragColor.a, gl_FragCoord.xy))
	{
		discard;
	}
	vec4 color = vec4(texel.rgb * fragColor.rgb, 1.0);
	// Pixel snap (#136): light, shadow and fog at the centre of the texel, so they step with the sprite's pixels.
	vec3 position = fragWorldPos;
	vec3 shadowPosition = fragShadowPos;
	if (SnapsToTexels())
	{
		vec2 size = TextureSize(int(texIndex));
		position += TexelCentreOffset(surface, fragTexCoord, size);
		shadowPosition += TexelCentreOffset(shadowSurface, fragTexCoord, size);
	}
	vec3 lit = color.rgb * ComputeLighting(position, QuadNormal(fragWorldPos), SpriteWrap, SunShadowSprite(shadowPosition));
	// Emission (plan P3) after the lighting, so a flame isn't darkened by night or shadow; fogged like the rest.
	lit += color.rgb * fragEmission;
	outColor = vec4(ApplyFog(lit, position), 1.0);
	outEntityID = entityID;
}
