// The Standard material's fragment stage as a function (mesh.frag, mesh_fade.frag); included, not compiled on its
// own. Needs lighting.glsl and normal_mapping.glsl included first.

// The Standard material (plan M4): albedo * tint, lit by the view's forward lighting when it has it
// (plan M6; unlit otherwise) with the normal map's normal when it has one, plus emission, then fog; cut out below
// AlphaCutoff.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;

// Must match MaterialUniform in Renderer3D.cpp (std140, 48 bytes).
layout(set = 1, binding = 0) uniform Material {
    vec4 tint;
    vec4 emission;   // rgb = colour * intensity
    vec2 tiling;     // repeats per metre
    float alphaCutoff;
    float normalStrength; // 0 = no normal map (binding 3 is the flat one)
} material;
layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D emissiveTexture;
layout(set = 1, binding = 3) uniform sampler2D normalTexture; // tangent space, RGBA (linear), green up

layout(location = 0) out vec4 outColor;
// The Editor view's entity-ID attachment; ignored in views without it.
layout(location = 1) out int outEntityID;

void ShadeStandardMesh()
{
	vec2 uv = fragUV * material.tiling;
	SurfaceDerivatives derivatives = GetSurfaceDerivatives(fragWorldPos, uv); // before the discard
	vec4 albedo = texture(albedoTexture, uv) * material.tint * fragColor;
	if (albedo.a < material.alphaCutoff)
	{
		discard;
	}
	vec3 emissive = texture(emissiveTexture, uv).rgb * material.emission.rgb;
	if (IsForwardLit() == false)
	{
		outColor = vec4(albedo.rgb + emissive, 1.0);
		outEntityID = entityID;
		return;
	}

	// A normal that faces away from the camera belongs to a back face seen through a cut-out: flip it.
	vec3 normal = normalize(fragNormal);
	if (gl_FrontFacing == false)
	{
		normal = -normal;
	}
	// Pixel snap (#136): light, shadow and fog at the centre of the albedo texel, so they step with the art's pixels.
	vec3 position = fragWorldPos;
	if (SnapsToTexels())
	{
		position += TexelCentreOffset(derivatives, uv, vec2(textureSize(albedoTexture, 0)));
	}
	// The shadow lookup keeps the surface's own normal (its offset is about the geometry); the light uses the map's.
	float sun = SunShadowSurface(position, normal);
	if (material.normalStrength > 0.0)
	{
		normal = PerturbNormal(normal, derivatives, texture(normalTexture, uv).rgb, material.normalStrength);
	}
	vec3 lit = albedo.rgb * ComputeLighting(position, normal, 0.5, sun) + emissive;
	outColor = vec4(ApplyFog(lit, position), 1.0);
	outEntityID = entityID;
}
