// How a surface's position and UV change across the screen, and what that gives: the tangent frame of a normal map
// (normal_mapping.glsl) and the offset to the centre of the texel a pixel samples (pixel stability, #136). Included,
// not compiled on its own; safe to include twice.
#ifndef SURFACE_DERIVATIVES_GLSL
#define SURFACE_DERIVATIVES_GLSL

// How the position and the UV change to the next pixel across and down. Take them at the top of main, before any
// discard or early return: derivatives are only defined while the whole 2 x 2 quad still runs.
struct SurfaceDerivatives
{
	vec3 positionX;
	vec3 positionY;
	vec2 uvX;
	vec2 uvY;
};

SurfaceDerivatives GetSurfaceDerivatives(vec3 position, vec2 uv)
{
	SurfaceDerivatives d;
	d.positionX = dFdx(position);
	d.positionY = dFdy(position);
	d.uvX = dFdx(uv);
	d.uvY = dFdy(uv);
	return d;
}

// How far the position moves (in the position's space) from this pixel to the centre of the texel it samples, on
// the plane the derivatives describe. `texelsPerUv`: the texture's size in texels per unit of `uv` (textureSize for
// UVs 0..1). Lighting and shadows taken at position + this offset are constant across a texel, so a pixel-art
// texture is lit texel by texel (like its albedo, which point sampling already keeps whole) and a shadow edge or a
// light's falloff steps along the texture's own pixels instead of crawling across them as the camera moves. A
// texel smaller than a screen pixel moves the position less than a pixel; UVs that don't change across the pixel
// (a stretched single texel) give no offset.
vec3 TexelCentreOffset(SurfaceDerivatives d, vec2 uv, vec2 texelsPerUv)
{
	vec2 texel = uv * texelsPerUv;
	vec2 toCentre = (floor(texel) + 0.5 - texel) / texelsPerUv; // in UV
	// The screen step (x, y) that moves the UV by toCentre: toCentre = uvX * x + uvY * y.
	float determinant = d.uvX.x * d.uvY.y - d.uvX.y * d.uvY.x;
	if (abs(determinant) < 1e-14)
	{
		return vec3(0.0);
	}
	float x = (toCentre.x * d.uvY.y - toCentre.y * d.uvY.x) / determinant;
	float y = (d.uvX.x * toCentre.y - d.uvX.y * toCentre.x) / determinant;
	return d.positionX * x + d.positionY * y;
}

#endif
