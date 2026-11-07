#version 450

#extension GL_GOOGLE_include_directive : require

#include "bloom_common.glsl"

// Bloom, up the chain (plan P2): a smaller level, read with a 3 x 3 tent filter (params.z = its radius in that
// level's texels: larger spreads the glow further), added onto the level above it (the pipeline blends additively).

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
	vec2 d = bloom.params.xy * bloom.params.z;
	vec3 sum = texture(source, fragUV).rgb * 4.0;
	sum += (texture(source, fragUV + vec2(-d.x, 0.0)).rgb + texture(source, fragUV + vec2(d.x, 0.0)).rgb
		+ texture(source, fragUV + vec2(0.0, -d.y)).rgb + texture(source, fragUV + vec2(0.0, d.y)).rgb) * 2.0;
	sum += texture(source, fragUV + vec2(-d.x, -d.y)).rgb + texture(source, fragUV + vec2(d.x, -d.y)).rgb
		+ texture(source, fragUV + vec2(-d.x, d.y)).rgb + texture(source, fragUV + vec2(d.x, d.y)).rgb;
	outColor = vec4(sum / 16.0, 1.0);
}
