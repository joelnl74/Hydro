#version 450

// The Tonemap pass of HDR views (post-processing plan P1; Renderer2D::RecordTonemap): the HDR scene colour,
// times the scene's exposure, through the highlight roll-off of Khronos PBR Neutral, into the view's sRGB colour. Uses
// composite.vert's fullscreen triangle. The depth of field's blur is mixed in first (#135), then the bloom added (plan P2). Linear in, linear out: the sRGB attachment encodes it (no gamma here).

// The scene colour (RGBA16F, the view's size), read texel for texel, and the bloom (plan P2: level 0 of the chain,
// half the size, filtered up; the scene colour again, unread, without bloom).
layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D bloomColor;
// Depth of field (#135): the signed circle of confusion (R16F, the view's size, read texel for texel) and the blur
// (half the size, filtered up; alpha: how far a blurred foreground reaches over the pixel). The scene colour again,
// unread, without it.
layout(set = 0, binding = 2) uniform sampler2D circleOfConfusion;
layout(set = 0, binding = 3) uniform sampler2D depthOfFieldColor;

// Must match TonemapPushConstants in VulkanGPUData.h (std430, 16 bytes). Padding as three floats: a vec3 would
// be aligned to 16 bytes and make the block 28 bytes long.
layout(push_constant) uniform TonemapPushConstants {
    float exposureScale;  // 2^exposure
    float bloomIntensity; // 0 = no bloom
    float depthOfField;   // 1 = mix in the depth of field's blur
    float padding0;
} tonemap;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// The highlight compression of Khronos PBR Neutral (https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral):
// identity below startCompression, so everything that isn't bright looks exactly as without HDR and the palette is
// kept; brighter values roll off towards white without changing hue much. Khronos' toe (subtracting up to 0.04
// in the darks) is left out on purpose: it darkened and saturated the dusk and night scenes by almost half
// (measured on the Shrine Glade, Oct 27, 2026), and a pixel-art palette is authored for the untouched darks.
vec3 NeutralHighlightRolloff(vec3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
	{
		return color;
	}

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return mix(color, vec3(newPeak), g);
}

void main()
{
	vec3 hdr = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0).rgb;
	// The blur first, so the glow and the tonemap treat it like the rest of the scene. Sharp up to about a pixel of
	// blur, all blur from about three (the half-size image is a little soft itself).
	if (tonemap.depthOfField > 0.5)
	{
		float coc = abs(texelFetch(circleOfConfusion, ivec2(gl_FragCoord.xy), 0).r);
		vec4 blurred = texture(depthOfFieldColor, fragUV);
		hdr = mix(hdr, blurred.rgb, smoothstep(1.0, 3.0, max(coc, blurred.a)));
	}
	// The glow is added to the scene before exposure and the tonemap, in linear HDR, so it rolls off with it.
	if (tonemap.bloomIntensity > 0.0)
	{
		hdr += texture(bloomColor, fragUV).rgb * tonemap.bloomIntensity;
	}
	vec3 exposed = max(hdr * tonemap.exposureScale, vec3(0.0));
	outColor = vec4(NeutralHighlightRolloff(exposed), 1.0);
}
