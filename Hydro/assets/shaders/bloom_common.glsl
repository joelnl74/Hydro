// Shared by the bloom passes (post-processing plan P2; Renderer2D::RecordBloom), all on composite.vert's
// fullscreen triangle into one level of the chain. Included, not compiled on its own.

// The image this pass reads (the scene colour, or a level of the chain), linear filtered, clamped.
layout(set = 0, binding = 0) uniform sampler2D source;

// Must match BloomPushConstants in VulkanGPUData.h (std430, 16 bytes). xy = one texel of `source` in UV,
// z and w depend on the pass.
layout(push_constant) uniform BloomPushConstants {
    vec4 params;
} bloom;

// The 13-tap downsample of "Next generation post processing in Call of Duty: Advanced Warfare" (Jimenez, 2014):
// five overlapping 2 x 2 boxes, each read with one bilinear tap per corner. `karis`: each box is weighted by
// 1 / (1 + its brightness), so a single very bright pixel can't flicker into a big blotch (fireflies).
vec3 Downsample13(vec2 uv, vec2 texel, bool karis)
{
	vec3 a = texture(source, uv + texel * vec2(-2.0,  2.0)).rgb;
	vec3 b = texture(source, uv + texel * vec2( 0.0,  2.0)).rgb;
	vec3 c = texture(source, uv + texel * vec2( 2.0,  2.0)).rgb;
	vec3 d = texture(source, uv + texel * vec2(-2.0,  0.0)).rgb;
	vec3 e = texture(source, uv).rgb;
	vec3 f = texture(source, uv + texel * vec2( 2.0,  0.0)).rgb;
	vec3 g = texture(source, uv + texel * vec2(-2.0, -2.0)).rgb;
	vec3 h = texture(source, uv + texel * vec2( 0.0, -2.0)).rgb;
	vec3 i = texture(source, uv + texel * vec2( 2.0, -2.0)).rgb;
	vec3 j = texture(source, uv + texel * vec2(-1.0,  1.0)).rgb;
	vec3 k = texture(source, uv + texel * vec2( 1.0,  1.0)).rgb;
	vec3 l = texture(source, uv + texel * vec2(-1.0, -1.0)).rgb;
	vec3 m = texture(source, uv + texel * vec2( 1.0, -1.0)).rgb;

	vec3 boxes[5] = vec3[5]((j + k + l + m) * 0.25, (a + b + d + e) * 0.25, (b + c + e + f) * 0.25, (d + e + g + h) * 0.25, (e + f + h + i) * 0.25);
	const float weights[5] = float[5](0.5, 0.125, 0.125, 0.125, 0.125);

	vec3 sum = vec3(0.0);
	float total = 0.0;
	for (int n = 0; n < 5; n++)
	{
		float w = weights[n];
		if (karis)
		{
			float luma = dot(boxes[n], vec3(0.2126, 0.7152, 0.0722));
			w /= 1.0 + luma;
		}
		sum += boxes[n] * w;
		total += w;
	}
	return sum / total;
}
