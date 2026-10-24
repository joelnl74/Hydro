#include "hypch.h"
#include "TextureImporter.h"
#include "TextureImportSerializer.h"
#include "SpriteSlicer.h"

#include "Hydro/Renderer/Texture2D.h"

// stb_image's implementation is compiled here, once for the whole engine (VulkanImage.cpp and
// SpriteSlicer.cpp use it too). Vendored code: its warnings are not ours.
#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#include <fstream>
#include <iterator>

namespace Hydro
{
	// Pixels of exactly the key colour become fully transparent (and black, so linear filtering
	// doesn't pull the key colour into the sprite's edge).
	static void ApplyColorKey(uint8_t* pixels, size_t pixelCount, const glm::uvec3& key)
	{
		for (size_t i = 0; i < pixelCount; i++)
		{
			uint8_t* pixel = pixels + i * 4;
			if (pixel[0] == key.r && pixel[1] == key.g && pixel[2] == key.b)
			{
				pixel[0] = 0;
				pixel[1] = 0;
				pixel[2] = 0;
				pixel[3] = 0;
			}
		}
	}

	int64_t TextureImporter::GetImportConfigVersion(const std::filesystem::path& absolutePath)
	{
		std::error_code error;
		const std::filesystem::path importPath = Texture2D::GetImportFilePath(absolutePath);
		if (std::filesystem::exists(importPath, error) == false)
		{
			return 0;
		}

		return (int64_t)std::filesystem::last_write_time(importPath, error).time_since_epoch().count();
	}

	bool TextureImporter::Import(const std::filesystem::path& absolutePath, const TextureImportConfig& fallback, ImportedTexture& imported)
	{
		imported = {};

		// The import config next to the file decides how it is imported; without one, the fallback is used.
		TextureImportConfig config = fallback;
		std::error_code error;
		const std::filesystem::path importPath = Texture2D::GetImportFilePath(absolutePath);
		if (std::filesystem::exists(importPath, error))
		{
			TextureImportConfig fromFile;
			if (TextureImportSerializer::Deserialize(importPath, fromFile))
			{
				config = fromFile;
			}
		}

		// Read the bytes ourselves (std::ifstream takes any path), then decode from memory.
		std::ifstream file(absolutePath, std::ios::binary);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("TextureImporter: can't open '{}'", absolutePath.string());
			return false;
		}
		const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* decoded = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &width, &height, &channels, STBI_rgb_alpha);
		if (decoded == nullptr)
		{
			HY_CORE_ERROR("TextureImporter: can't decode '{}': {}", absolutePath.string(), stbi_failure_reason());
			return false;
		}

		imported.Pixels.assign(decoded, decoded + (size_t)width * (size_t)height * 4);
		stbi_image_free(decoded);

		// A sheet whose .hymeta has slicing settings but no sprite list is sliced now (written by hand,
		// or Mode: Auto). The sprites aren't written back; the file stays as it is.
		if (config.Mode == SpriteMode::Multiple && config.Sprites.empty() && config.Slicing.Mode != SpriteSlicingMode::Manual)
		{
			if (config.Slicing.Mode == SpriteSlicingMode::Auto &&
				SpriteSlicer::DetectGrid(imported.Pixels.data(), (uint32_t)width, (uint32_t)height, config, config.Slicing) == false)
			{
				HY_CORE_WARN("TextureImporter: '{}': no sprite grid found; using the whole image", absolutePath.string());
			}
			config.Sprites = SpriteSlicer::SliceGrid((uint32_t)width, (uint32_t)height, config.Slicing, absolutePath.stem().u8string());
		}

		if (config.UseColorKey)
		{
			ApplyColorKey(imported.Pixels.data(), (size_t)width * (size_t)height, config.ColorKey);
		}

		imported.Specification.Width = (uint32_t)width;
		imported.Specification.Height = (uint32_t)height;
		imported.Specification.Format = config.Format;
		imported.Specification.Usage = ImageUsage::Texture;
		imported.Config = config;
		return true;
	}
}
