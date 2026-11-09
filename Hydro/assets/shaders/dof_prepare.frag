#version 450

#extension GL_GOOGLE_include_directive : require

#include "dof_common.glsl"

// Depth of field, step 2 (#135): the HDR scene colour at half size, with the circle of confusion in alpha, for the
// gather blur. Each output texel is the average of a 2 x 2 block; its CoC is the block's most negative one when any
// pixel is in front of the focus point (a foreground's blur spreads over what is behind it), else its largest.
// Behind the focus, the average is weighted by each pixel's CoC against the largest, so a sharp pixel in the block
// (the edge of an object in focus) adds nothing to the blurred background's colour and isn't smeared over it.

layout(set = 0, binding = 0) uniform sampler2D sceneColor;        // the view's size, read texel for texel
layout(set = 0, binding = 1) uniform sampler2D circleOfConfusion; // R16F, the view's size (step 1)

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 base = ivec2(gl_FragCoord.xy) * 2;
	ivec2 last = textureSize(sceneColor, 0) - 1;
	vec3 colors[4];
	float cocs[4];
	float nearest = 0.0;
	float farthest = 0.0;
	for (int i = 0; i < 4; i++)
	{
		ivec2 p = min(base + ivec2(i & 1, i >> 1), last);
		colors[i] = texelFetch(sceneColor, p, 0).rgb;
		cocs[i] = texelFetch(circleOfConfusion, p, 0).r;
		nearest = min(nearest, cocs[i]);
		farthest = max(farthest, cocs[i]);
	}
	// In front, or all about as sharp: the plain average. Behind: by CoC (the farthest pixel weighs 1, so total >= 1).
	bool weighted = nearest >= 0.0 && farthest > 0.5;
	vec3 color = vec3(0.0);
	float total = 0.0;
	for (int i = 0; i < 4; i++)
	{
		float weight = weighted ? clamp(cocs[i] / farthest, 0.0, 1.0) : 1.0;
		color += colors[i] * weight;
		total += weight;
	}
	outColor = vec4(color / total, nearest < 0.0 ? nearest : farthest);
}
