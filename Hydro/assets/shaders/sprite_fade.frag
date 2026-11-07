#version 450

#extension GL_GOOGLE_include_directive : require

#include "sprite_common.glsl"
#include "fade_dither.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;
layout(location = 3) flat in int entityID;
layout(location = 6) flat in vec3 fragEmission; // see fragment.frag

layout(location = 0) out vec4 outColor;
layout(location = 1) out int outEntityID; // see fragment.frag

// sprite_cutout.frag for sprites being faded (SpriteDrawMode::FadedCutout: a cut-out sprite whose Opacity is below
// 1): cut out by the texture, screen-door faded by the quad's alpha. A game effect kept out of the main sprite
// shaders (see mesh_fade.frag).
void main()
{
	// The quad's alpha is the fade (SpriteRendererComponent::Color.a times Opacity, SceneRenderer): the texture's
	// alpha alone cuts out, the quad's dithers.
	vec4 texel = SampleTexture(int(texIndex), fragTexCoord);
	if (texel.a < 0.5 || FadeDiscards(fragColor.a, gl_FragCoord.xy))
	{
		discard;
	}
	vec4 color = vec4(texel.rgb * fragColor.rgb, 1.0);
	outColor = vec4(color.rgb + color.rgb * fragEmission, 1.0);
	outEntityID = entityID;
}
