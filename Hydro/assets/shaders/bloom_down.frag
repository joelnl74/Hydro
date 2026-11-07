#version 450

#extension GL_GOOGLE_include_directive : require

#include "bloom_common.glsl"

// Bloom, down the chain (plan P2): one level -> the next, half the size, with the 13-tap filter.

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(Downsample13(fragUV, bloom.params.xy, false), 1.0);
}
