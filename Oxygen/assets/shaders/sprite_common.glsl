// The quad shaders' texture array and how they sample it (fragment.frag, sprite_cutout.frag).
// Included, not compiled on its own.

// Must match Renderer2D's MaxTextureSlots (= VulkanDescriptorBuilder::MaxImageArraySize).
// Slot 0 is always a 1x1 white texture, so untextured quads are drawn with their colour alone.
layout(binding = 1) uniform sampler2D inTextures[32];

// Quads in one draw call use different slots, so the index isn't the same for the whole draw.
// Indexing a sampler array with such a value needs descriptor indexing (nonuniformEXT), which the
// device doesn't enable; a switch with constant indices is always valid. Derivatives are taken
// before the switch (textureGrad), so mip/filter selection stays correct at quad edges.
vec4 SampleTexture(int index, vec2 uv)
{
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);

	switch (index)
	{
		case 0: return textureGrad(inTextures[0], uv, dx, dy);
		case 1: return textureGrad(inTextures[1], uv, dx, dy);
		case 2: return textureGrad(inTextures[2], uv, dx, dy);
		case 3: return textureGrad(inTextures[3], uv, dx, dy);
		case 4: return textureGrad(inTextures[4], uv, dx, dy);
		case 5: return textureGrad(inTextures[5], uv, dx, dy);
		case 6: return textureGrad(inTextures[6], uv, dx, dy);
		case 7: return textureGrad(inTextures[7], uv, dx, dy);
		case 8: return textureGrad(inTextures[8], uv, dx, dy);
		case 9: return textureGrad(inTextures[9], uv, dx, dy);
		case 10: return textureGrad(inTextures[10], uv, dx, dy);
		case 11: return textureGrad(inTextures[11], uv, dx, dy);
		case 12: return textureGrad(inTextures[12], uv, dx, dy);
		case 13: return textureGrad(inTextures[13], uv, dx, dy);
		case 14: return textureGrad(inTextures[14], uv, dx, dy);
		case 15: return textureGrad(inTextures[15], uv, dx, dy);
		case 16: return textureGrad(inTextures[16], uv, dx, dy);
		case 17: return textureGrad(inTextures[17], uv, dx, dy);
		case 18: return textureGrad(inTextures[18], uv, dx, dy);
		case 19: return textureGrad(inTextures[19], uv, dx, dy);
		case 20: return textureGrad(inTextures[20], uv, dx, dy);
		case 21: return textureGrad(inTextures[21], uv, dx, dy);
		case 22: return textureGrad(inTextures[22], uv, dx, dy);
		case 23: return textureGrad(inTextures[23], uv, dx, dy);
		case 24: return textureGrad(inTextures[24], uv, dx, dy);
		case 25: return textureGrad(inTextures[25], uv, dx, dy);
		case 26: return textureGrad(inTextures[26], uv, dx, dy);
		case 27: return textureGrad(inTextures[27], uv, dx, dy);
		case 28: return textureGrad(inTextures[28], uv, dx, dy);
		case 29: return textureGrad(inTextures[29], uv, dx, dy);
		case 30: return textureGrad(inTextures[30], uv, dx, dy);
		case 31: return textureGrad(inTextures[31], uv, dx, dy);
	}

	return textureGrad(inTextures[0], uv, dx, dy);
}

// The texture's size in texels (level 0), for the texel snap of the lit sprite shaders (TexelCentreOffset, #136).
vec2 TextureSize(int index)
{
	switch (index)
	{
		case 0: return vec2(textureSize(inTextures[0], 0));
		case 1: return vec2(textureSize(inTextures[1], 0));
		case 2: return vec2(textureSize(inTextures[2], 0));
		case 3: return vec2(textureSize(inTextures[3], 0));
		case 4: return vec2(textureSize(inTextures[4], 0));
		case 5: return vec2(textureSize(inTextures[5], 0));
		case 6: return vec2(textureSize(inTextures[6], 0));
		case 7: return vec2(textureSize(inTextures[7], 0));
		case 8: return vec2(textureSize(inTextures[8], 0));
		case 9: return vec2(textureSize(inTextures[9], 0));
		case 10: return vec2(textureSize(inTextures[10], 0));
		case 11: return vec2(textureSize(inTextures[11], 0));
		case 12: return vec2(textureSize(inTextures[12], 0));
		case 13: return vec2(textureSize(inTextures[13], 0));
		case 14: return vec2(textureSize(inTextures[14], 0));
		case 15: return vec2(textureSize(inTextures[15], 0));
		case 16: return vec2(textureSize(inTextures[16], 0));
		case 17: return vec2(textureSize(inTextures[17], 0));
		case 18: return vec2(textureSize(inTextures[18], 0));
		case 19: return vec2(textureSize(inTextures[19], 0));
		case 20: return vec2(textureSize(inTextures[20], 0));
		case 21: return vec2(textureSize(inTextures[21], 0));
		case 22: return vec2(textureSize(inTextures[22], 0));
		case 23: return vec2(textureSize(inTextures[23], 0));
		case 24: return vec2(textureSize(inTextures[24], 0));
		case 25: return vec2(textureSize(inTextures[25], 0));
		case 26: return vec2(textureSize(inTextures[26], 0));
		case 27: return vec2(textureSize(inTextures[27], 0));
		case 28: return vec2(textureSize(inTextures[28], 0));
		case 29: return vec2(textureSize(inTextures[29], 0));
		case 30: return vec2(textureSize(inTextures[30], 0));
		case 31: return vec2(textureSize(inTextures[31], 0));
	}

	return vec2(textureSize(inTextures[0], 0));
}
