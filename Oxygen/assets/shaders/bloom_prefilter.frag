#version 450

#extension GL_GOOGLE_include_directive : require

#include "bloom_common.glsl"

// Bloom, first pass (plan P2): the HDR scene colour -> level 0 (half the size), keeping only what is brighter
// than the threshold, with a soft knee so the glow fades in instead of switching on (no hard edges, no flicker
// when a pixel hovers at the threshold). params.z = threshold, params.w = knee (absolute, threshold * BloomKnee).

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
	// Huge values (a bad pixel, an infinity) would smear over the whole chain: clamp to half-float range first.
	vec3 color = min(Downsample13(fragUV, bloom.params.xy, true), vec3(65000.0));

	float threshold = bloom.params.z;
	float knee = bloom.params.w;
	float brightness = max(color.r, max(color.g, color.b));
	float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
	soft = soft * soft / (4.0 * knee + 0.00001);
	float contribution = max(soft, brightness - threshold) / max(brightness, 0.00001);
	outColor = vec4(color * max(contribution, 0.0), 1.0);
}
