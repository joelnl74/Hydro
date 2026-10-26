#version 450

// Mesh shadow casters (R14): only depth is written; pixels the Standard material cuts out (mesh.frag's
// alphaCutoff) cast nothing. Uses mesh_shadow.vert.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

// Must match MaterialUniform in Renderer3D.cpp (std140, 48 bytes), as in mesh.frag.
layout(set = 1, binding = 0) uniform Material {
    vec4 tint;
    vec4 emission;
    vec2 tiling;
    float alphaCutoff;
    float padding;
} material;
layout(set = 1, binding = 1) uniform sampler2D albedoTexture;

void main()
{
	if (material.alphaCutoff > 0.0)
	{
		vec4 albedo = texture(albedoTexture, fragUV * material.tiling) * material.tint * fragColor;
		if (albedo.a < material.alphaCutoff)
		{
			discard;
		}
	}
}
