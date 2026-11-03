// Tangent-space normal maps without tangents in the vertices (mesh_standard.glsl, terrain.frag); included, not compiled
// on its own. The tangent frame of each pixel comes from how the world position and the UV change across the screen
// ("Followup: Normal Mapping Without Precomputed Tangents", Christian Schueler, 2013), so it follows any UV layout,
// mirrored UVs included, and the meshes, the importer and the terrain mesher need no tangents.
//
// Convention (MaterialData::Normal): red = +U (right in the image), green = -V (up in the image: glTF and OpenGL),
// blue = out of the surface; 0.5 is zero. scripts/game/textures/make_normal_maps.py writes them this way.

#include "surface_derivatives.glsl"

// The frame whose columns are the directions of +U, +V and the normal at the pixel, scaled alike.
mat3 CotangentFrame(vec3 normal, SurfaceDerivatives d)
{
	vec3 dp2perp = cross(d.positionY, normal);
	vec3 dp1perp = cross(normal, d.positionX);
	vec3 tangent = dp2perp * d.uvX.x + dp1perp * d.uvY.x;
	vec3 bitangent = dp2perp * d.uvX.y + dp1perp * d.uvY.y;

	float scale = max(dot(tangent, tangent), dot(bitangent, bitangent));
	if (scale <= 1e-20)
	{
		// UVs that don't change across the pixel (a stretched single texel): no frame, keep the normal.
		return mat3(vec3(0.0), vec3(0.0), normal);
	}
	float inverseLength = inversesqrt(scale);
	return mat3(tangent * inverseLength, bitangent * inverseLength, normal);
}

// A map's texel `encoded` (0..1 per channel) as a direction in the frame (+U, +V, normal), its slopes scaled by
// `strength` (0 = flat, 1 = as painted). Not normalised: mix several, then ApplyTangentNormal.
vec3 DecodeTangentNormal(vec3 encoded, float strength)
{
	vec3 mapped = encoded * 2.0 - 1.0;
	mapped.y = -mapped.y; // green is up the image (-V); the frame's second column is +V
	mapped.xy *= strength;
	return mapped;
}

// `normal` (unit, world space, facing the viewer) turned by `mapped` (DecodeTangentNormal).
vec3 ApplyTangentNormal(vec3 normal, SurfaceDerivatives d, vec3 mapped)
{
	vec3 bent = CotangentFrame(normal, d) * mapped;
	float lengthSquared = dot(bent, bent);
	return lengthSquared > 1e-12 ? bent * inversesqrt(lengthSquared) : normal;
}

// `normal` bent by the map's texel `encoded`, its slopes scaled by `strength`.
vec3 PerturbNormal(vec3 normal, SurfaceDerivatives d, vec3 encoded, float strength)
{
	return ApplyTangentNormal(normal, d, DecodeTangentNormal(encoded, strength));
}
