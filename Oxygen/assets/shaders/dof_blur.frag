#version 450

#extension GL_GOOGLE_include_directive : require

#include "dof_common.glsl"

// Depth of field, step 3 (#135): a gather blur at half size ("Bokeh depth of field in a single pass", Dennis
// Gustafsson, 2018). Samples spiral out from the pixel by the golden angle, each a little further (texel.w / radius),
// up to texel.z pixels; a sample counts where its own circle of confusion reaches back to the pixel. So a blurred
// foreground spreads over a sharp background, while a blurred background never spreads over a sharp foreground
// (a sample behind the pixel counts at most as far as the pixel's own blur). Alpha: how far a blurred foreground
// reaches over the pixel (full-size pixels), so the tonemap mixes the blur in there even where the pixel is sharp.

layout(set = 0, binding = 0) uniform sampler2D halfColor; // step 2: colour, alpha the signed CoC (full-size pixels)

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

const float GoldenAngle = 2.39996323;
const int MaxSamples = 128; // the CPU picks texel.w so the spiral reaches texel.z within this many

void main()
{
	vec4 center = texelFetch(halfColor, ivec2(gl_FragCoord.xy), 0);
	float centerCoc = center.a * 0.5; // in this pass's (half-size) pixels
	float centerSize = abs(centerCoc);

	vec3 color = center.rgb;
	float total = 1.0;
	float nearReach = 0.0;
	float radius = dof.texel.w;
	for (int i = 0; i < MaxSamples; i++)
	{
		if (radius > dof.texel.z)
		{
			break;
		}
		float angle = float(i) * GoldenAngle;
		vec4 s = textureLod(halfColor, fragUV + vec2(cos(angle), sin(angle)) * radius * dof.texel.xy, 0.0);
		float sampleCoc = s.a * 0.5;
		float sampleSize = abs(sampleCoc);
		if (sampleCoc > centerCoc)
		{
			sampleSize = min(sampleSize, centerSize * 2.0);
		}
		// 1 where the sample's blur covers this pixel, fading over a pixel at its edge.
		float covers = smoothstep(radius - 0.5, radius + 0.5, sampleSize);
		color += mix(color / total, s.rgb, covers);
		total += 1.0;
		if (sampleCoc < 0.0 && sampleCoc < centerCoc)
		{
			nearReach = max(nearReach, covers * sampleSize);
		}
		radius += dof.texel.w / radius;
	}
	outColor = vec4(color / total, nearReach * 2.0);
}
