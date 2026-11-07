#version 450

// The Tonemap pass of HDR views (post-processing plan P1; Renderer2D::RecordTonemap): the HDR scene colour,
// times the scene's exposure, through the highlight roll-off of Khronos PBR Neutral, into the view's sRGB colour. Uses
// composite.vert's fullscreen triangle. The depth of field's blur is mixed in first (#135), then the bloom added (plan P2),
// then the grade (#137: white balance and contrast before the roll-off, lift / gamma / gain and saturation after it) and
// the vignette. Linear in, linear out: the sRGB attachment encodes it (no gamma here).

// The scene colour (RGBA16F, the view's size), read texel for texel, and the bloom (plan P2: level 0 of the chain,
// half the size, filtered up; the scene colour again, unread, without bloom).
layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D bloomColor;
// Depth of field (#135): the signed circle of confusion (R16F, the view's size, read texel for texel) and the blur
// (half the size, filtered up; alpha: how far a blurred foreground reaches over the pixel). The scene colour again,
// unread, without it.
layout(set = 0, binding = 2) uniform sampler2D circleOfConfusion;
layout(set = 0, binding = 3) uniform sampler2D depthOfFieldColor;
// The depth of field's half-size colour (step 2), read only for its alpha: each half-size texel's signed circle of
// confusion, so the blur is filtered up only from texels at the pixel's own depth (no sharp edge's colour on a
// blurred background).
layout(set = 0, binding = 4) uniform sampler2D depthOfFieldHalf;

// Must match TonemapPushConstants in VulkanGPUData.h (std430, 96 bytes: six vec4s).
layout(push_constant) uniform TonemapPushConstants {
    vec4 params;       // x: 2^exposure, y: bloom intensity (0 = no bloom), z: 1 = mix in the depth of field, w: 1 = grade
    vec4 vignette;     // x: intensity (0 = no vignette), y: radius (where the darkening starts: 0 = the centre, 1 = the
                       // corners), z: softness (over how much further it reaches its full darkness)
    vec4 whiteBalance; // rgb: white balance multiplier, w: contrast
    vec4 lift;         // rgb: lift, w: saturation
    vec4 inverseGamma; // rgb: 1 / gamma
    vec4 gain;         // rgb: gain
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

const vec3 Luma = vec3(0.2126, 0.7152, 0.0722); // Rec. 709, linear

// The depth of field's blur at this pixel, filtered up from half size by depth: the four half-size texels around it
// weighted bilinearly and by how close their circle of confusion is to the pixel's (half the weight per pixel of
// difference), so a sharp object's texels don't bleed into the blurred background beside it (#135).
vec3 DepthAwareBlur(float pixelCoc)
{
	vec2 size = vec2(textureSize(depthOfFieldColor, 0));
	vec2 position = fragUV * size - 0.5;
	ivec2 base = ivec2(floor(position));
	vec2 f = position - vec2(base);
	ivec2 last = ivec2(size) - 1;

	vec3 color = vec3(0.0);
	float total = 0.0;
	for (int i = 0; i < 4; i++)
	{
		ivec2 offset = ivec2(i & 1, i >> 1);
		ivec2 texel = clamp(base + offset, ivec2(0), last);
		float bilinear = (offset.x == 1 ? f.x : 1.0 - f.x) * (offset.y == 1 ? f.y : 1.0 - f.y);
		float texelCoc = texelFetch(depthOfFieldHalf, texel, 0).a;
		float weight = bilinear * (exp2(-abs(texelCoc - pixelCoc)) + 1e-5);
		color += texelFetch(depthOfFieldColor, texel, 0).rgb * weight;
		total += weight;
	}
	return color / max(total, 1e-6);
}

// White balance and contrast, on the exposed HDR colour (before the roll-off). Contrast is a slope in stops around
// middle grey (0.18), so darks and lights spread without clipping.
vec3 GradeHdr(vec3 color)
{
	color *= tonemap.whiteBalance.rgb;
	float contrast = tonemap.whiteBalance.w;
	if (contrast != 1.0)
	{
		color = 0.18 * pow(max(color, vec3(1e-6)) / 0.18, vec3(contrast));
	}
	return color;
}

// Lift, gamma, gain and saturation, on the rolled-off colour (0..1).
vec3 GradeDisplay(vec3 color)
{
	color = tonemap.gain.rgb * (color + tonemap.lift.rgb * (1.0 - color));
	color = pow(max(color, vec3(0.0)), tonemap.inverseGamma.rgb);
	float luma = dot(color, Luma);
	return max(mix(vec3(luma), color, tonemap.lift.w), vec3(0.0));
}

void main()
{
	vec3 hdr = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0).rgb;
	// The blur first, so the glow and the tonemap treat it like the rest of the scene. Sharp up to about a pixel of
	// blur, all blur from about three (the half-size image is a little soft itself). A blurred foreground reaching
	// over the pixel (alpha) is filtered up plainly, so it still spreads over what is behind it; the pixel's own blur
	// by depth.
	if (tonemap.params.z > 0.5)
	{
		float coc = texelFetch(circleOfConfusion, ivec2(gl_FragCoord.xy), 0).r;
		vec4 plain = texture(depthOfFieldColor, fragUV);
		vec3 blurred = mix(DepthAwareBlur(coc), plain.rgb, smoothstep(1.0, 3.0, plain.a));
		hdr = mix(hdr, blurred, smoothstep(1.0, 3.0, max(abs(coc), plain.a)));
	}
	// The glow is added to the scene before exposure and the tonemap, in linear HDR, so it rolls off with it.
	if (tonemap.params.y > 0.0)
	{
		hdr += texture(bloomColor, fragUV).rgb * tonemap.params.y;
	}
	vec3 exposed = max(hdr * tonemap.params.x, vec3(0.0));
	bool grade = tonemap.params.w > 0.5;
	if (grade)
	{
		exposed = GradeHdr(exposed);
	}
	vec3 color = NeutralHighlightRolloff(exposed);
	if (grade)
	{
		color = GradeDisplay(color);
	}
	// The vignette (Nov 3, 2026), after the roll-off so it darkens what is seen, glow included. The distance from the
	// centre is measured in the frame's own shape (an ellipse in a wide view): 0 at the centre, about 0.71 at the
	// middle of an edge, 1 in the corners.
	if (tonemap.vignette.x > 0.0)
	{
		float d = length((fragUV - 0.5) * 2.0) * 0.70710678;
		float darken = smoothstep(tonemap.vignette.y, tonemap.vignette.y + tonemap.vignette.z, d);
		color *= 1.0 - tonemap.vignette.x * darken;
	}
	outColor = vec4(color, 1.0);
}
