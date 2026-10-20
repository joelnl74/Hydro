#version 450

// Renderer2D compiles this shader with HY_TEXTURE_SLOTS = its texture slots per draw: 32, or 16 on
// a GPU that allows fewer than 32 samplers per stage (some macOS Metal drivers). Built on its own
// (glslc at build time) it uses 32. Slot 0 is always a 1x1 white texture, so untextured quads are
// drawn with their colour alone.
#ifndef HY_TEXTURE_SLOTS
#define HY_TEXTURE_SLOTS 32
#endif

layout(binding = 1) uniform sampler2D inTextures[HY_TEXTURE_SLOTS];

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in float texIndex;

layout(location = 0) out vec4 outColor;

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
#if HY_TEXTURE_SLOTS > 16
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
#endif
	}

	return textureGrad(inTextures[0], uv, dx, dy);
}

void main()
{
	outColor = SampleTexture(int(texIndex), fragTexCoord) * fragColor;
}
