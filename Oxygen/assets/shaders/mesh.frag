#version 450

// The Standard material (plan M4), unlit: albedo * tint + emission, cut out below AlphaCutoff.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) flat in int entityID;

// Must match MaterialUniform in Renderer3D.cpp (std140, 48 bytes).
layout(set = 1, binding = 0) uniform Material {
    vec4 tint;
    vec4 emission;   // rgb = colour * intensity
    vec2 tiling;     // repeats per metre
    float alphaCutoff;
    float padding;
} material;
layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D emissiveTexture;

layout(location = 0) out vec4 outColor;
// The Editor view's entity-ID attachment; ignored in views without it.
layout(location = 1) out int outEntityID;

void main()
{
	vec2 uv = fragUV * material.tiling;
	vec4 albedo = texture(albedoTexture, uv) * material.tint * fragColor;
	if (albedo.a < material.alphaCutoff)
	{
		discard;
	}
	vec3 emissive = texture(emissiveTexture, uv).rgb * material.emission.rgb;
	outColor = vec4(albedo.rgb + emissive, 1.0);
	outEntityID = entityID;
}
