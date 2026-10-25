// How a light fades with distance, shared by the 2D light buffer (light.frag) and forward lighting
// (lighting.glsl), so a lantern reaches as far in both. Included, not compiled on its own.
//
//   d      distance from the light divided by its radius: 0 at the light, 1 at the edge
//   bands  0 = a smooth fade, 2 or more = that many visible steps (RendererSettings::LightFalloff, LightBands)
//
// Bright near the light and exactly 0 at the radius, so there is no hard edge.
float LightFalloff(float d, float bands)
{
	if (d >= 1.0)
	{
		return 0.0;
	}
	float d2 = d * d;
	float falloff = (1.0 - d2) * (1.0 - d2) / (1.0 + 6.0 * d2);
	if (bands >= 2.0)
	{
		falloff = floor(falloff * bands + 0.5) / bands;
	}
	return falloff;
}
