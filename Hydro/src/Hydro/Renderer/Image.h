#pragma once
#include <cstdint>

namespace Hydro
{
	enum class ImageFormat
	{
		None = 0,
		RGB,
		RGBA,
		RGBA16F,
		RGBA32F,
		RG32F,

		SRGB,

		R32I, // one signed 32-bit integer per pixel (entity IDs)

		DEPTH32F,
		DEPTH24STENCIL8,

		// Defaults
		Depth = DEPTH24STENCIL8
	};

	enum class ImageUsage
	{
		None = 0,
		Texture,
		Attachment
	};

	enum class TextureWrap
	{
		None = 0,
		Clamp,
		Repeat
	};

	enum class TextureFilter
	{
		None = 0,
		Linear,
		Nearest
	};

	enum class TextureType
	{
		None = 0,
		Texture2D,
		TextureCube
	};

	// How a texture is sampled. Whether it is sRGB is part of what the image is
	// (ImageSpecification::Format = ImageFormat::SRGB or RGBA), not of how it is used.
	struct TextureProperties
	{
		TextureWrap SamplerWrap = TextureWrap::Repeat;
		TextureFilter SamplerFilter = TextureFilter::Linear;
		// Not applied yet: textures have a single mip level.
		bool GenerateMips = false;
	};

	struct ImageSpecification
	{
		ImageFormat Format = ImageFormat::RGBA;
		ImageUsage Usage = ImageUsage::Texture;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Mips = 1;
		uint32_t Layers = 1;
	};
}
