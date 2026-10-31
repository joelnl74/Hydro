#version 450

#extension GL_GOOGLE_include_directive : require

#include "dof_common.glsl"

// Depth of field, step 2 (#135): the HDR scene colour at half size, with the circle of confusion in alpha, for the
// gather blur. Each output texel is the average of a 2 x 2 block; its CoC is the block's most negative one when any
// pixel is in front of the focus point (a foreground's blur spreads over what is behind it), else its largest.

layout(set = 0, binding = 0) uniform sampler2D sceneColor;        // the view's size, read texel for texel
layout(set = 0, binding = 1) uniform sampler2D circleOfConfusion; // R16F, the view's size (step 1)

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 base = ivec2(gl_FragCoord.xy) * 2;
	ivec2 last = textureSize(sceneColor, 0) - 1;
	vec3 color = vec3(0.0);
	float nearest = 0.0;
	float farthest = 0.0;
	for (int i = 0; i < 4; i++)
	{
		ivec2 p = min(base + ivec2(i & 1, i >> 1), last);
		color += texelFetch(sceneColor, p, 0).rgb;
		float coc = texelFetch(circleOfConfusion, p, 0).r;
		nearest = min(nearest, coc);
		farthest = max(farthest, coc);
	}
	outColor = vec4(color * 0.25, nearest < 0.0 ? nearest : farthest);
}
