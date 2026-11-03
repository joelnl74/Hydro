// Screen-door fading (the game's camera occluder fade): a drawn thing at opacity a keeps a of its pixels in a fixed
// 4 x 4 ordered pattern, so it fades without sorting or blending and its depth stays right for what is behind it.
// Used only by the fade variants (mesh_fade.frag, sprite_fade*.frag), which the renderers pick for draws with an
// opacity below 1; the main shaders don't include it. Included, not compiled on its own.

float FadeDitherThreshold(vec2 pixel)
{
	const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	ivec2 p = ivec2(mod(pixel, 4.0));
	return (bayer[p.y * 4 + p.x] + 0.5) / 16.0;
}

// True when this pixel is one of those the fade leaves out.
bool FadeDiscards(float opacity, vec2 pixel)
{
	return opacity < 1.0 && opacity <= FadeDitherThreshold(pixel);
}
