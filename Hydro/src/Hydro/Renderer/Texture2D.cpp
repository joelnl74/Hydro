#include "hypch.h"
#include "Texture2D.h"
#include "TextureImportSerializer.h"
#include "SpriteSlicer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"

// stb_image's implementation is compiled here, once for the whole engine (VulkanImage.cpp uses it
// too). Vendored code: its warnings are not ours.
#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#include <string>
#include <unordered_map>

namespace Hydro
{
	// Textures loaded from files that are still alive, so many sprites using one file share one texture.
	// Weak: the cache never keeps a texture (or GPU memory) alive by itself.
	// TODO(assets): replaced by the AssetManager's handle -> asset table.
	static std::unordered_map<std::string, std::weak_ptr<Texture2D>> s_LoadedTextures;

	// The .hymeta's modification time is part of the key, so a re-saved import config re-imports
	// (sprites still holding the old texture keep it until they are given the new one).
	static std::string MakeCacheKey(const std::filesystem::path& path, const TextureProperties& properties, ImageFormat format, int64_t importFileTime)
	{
		return path.generic_u8string() + "|" + std::to_string((int)format) + "|" +
			std::to_string((int)properties.SamplerFilter) + "|" + std::to_string((int)properties.SamplerWrap) + "|" +
			std::to_string(importFileTime);
	}

	// Pixels of exactly the key colour become fully transparent (and black, so linear filtering
	// doesn't pull the key colour into the sprite's edge).
	static void ApplyColorKey(stbi_uc* pixels, size_t pixelCount, const glm::uvec3& key)
	{
		for (size_t i = 0; i < pixelCount; i++)
		{
			stbi_uc* pixel = pixels + i * 4;
			if (pixel[0] == key.r && pixel[1] == key.g && pixel[2] == key.b)
			{
				pixel[0] = 0;
				pixel[1] = 0;
				pixel[2] = 0;
				pixel[3] = 0;
			}
		}
	}

	Texture2D::Texture2D(const ImageSpecification& specification, const void* pixels, const TextureProperties& properties)
		: m_Specification(specification), m_Properties(properties), m_Data(CreateScope<Texture2DData>())
	{
		HY_CORE_ASSERT(specification.Usage == ImageUsage::Texture, "Texture2D: Usage must be ImageUsage::Texture");
		m_Specification.Usage = ImageUsage::Texture;
		m_Specification.Mips = 1;
		m_Specification.Layers = 1;

		m_Data->Image = CreateScope<VulkanImage>(m_Specification, pixels, m_Properties);

		TextureImportConfig config;
		config.Format = m_Specification.Format;
		config.Properties = m_Properties;
		SetImportConfig(config);
	}

	Texture2D::~Texture2D() = default;

	const Texture2DData& GetTexture2DData(const Texture2D& texture)
	{
		return *texture.m_Data;
	}

	Ref<Texture2D> Texture2D::LoadFromFile(const std::filesystem::path& path, const TextureProperties& properties, ImageFormat format)
	{
		std::error_code error;
		std::filesystem::path absolutePath = std::filesystem::absolute(path, error).lexically_normal();
		if (error)
		{
			absolutePath = path;
		}

		// The import config next to the file decides how it is imported; without one, the caller's
		// properties and format are used.
		TextureImportConfig config;
		config.Format = format;
		config.Properties = properties;
		int64_t importFileTime = 0;
		const std::filesystem::path importPath = GetImportFilePath(absolutePath);
		if (std::filesystem::exists(importPath, error))
		{
			TextureImportConfig imported;
			if (TextureImportSerializer::Deserialize(importPath, imported))
			{
				config = imported;
				importFileTime = (int64_t)std::filesystem::last_write_time(importPath, error).time_since_epoch().count();
			}
		}

		const std::string key = MakeCacheKey(absolutePath, config.Properties, config.Format, importFileTime);
		if (auto found = s_LoadedTextures.find(key); found != s_LoadedTextures.end())
		{
			if (Ref<Texture2D> texture = found->second.lock())
			{
				return texture;
			}
		}

		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load(absolutePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (pixels == nullptr)
		{
			HY_CORE_ERROR("Texture2D: can't load '{}': {}", absolutePath.string(), stbi_failure_reason());
			return nullptr;
		}

		// A sheet whose .hymeta has slicing settings but no sprite list is sliced now (written by hand,
		// or Mode: Auto). The sprites aren't written back; the file stays as it is.
		if (config.Mode == SpriteMode::Multiple && config.Sprites.empty() && config.Slicing.Mode != SpriteSlicingMode::Manual)
		{
			if (config.Slicing.Mode == SpriteSlicingMode::Auto &&
				SpriteSlicer::DetectGrid(pixels, (uint32_t)width, (uint32_t)height, config, config.Slicing) == false)
			{
				HY_CORE_WARN("Texture2D: '{}': no sprite grid found; using the whole image", absolutePath.string());
			}
			config.Sprites = SpriteSlicer::SliceGrid((uint32_t)width, (uint32_t)height, config.Slicing, absolutePath.stem().u8string());
		}

		if (config.UseColorKey)
		{
			ApplyColorKey(pixels, (size_t)width * (size_t)height, config.ColorKey);
		}

		ImageSpecification specification;
		specification.Width = (uint32_t)width;
		specification.Height = (uint32_t)height;
		specification.Format = config.Format;
		specification.Usage = ImageUsage::Texture;

		Ref<Texture2D> texture = CreateRef<Texture2D>(specification, pixels, config.Properties);
		stbi_image_free(pixels);
		texture->m_Path = absolutePath;
		AssetManager::BindHandle(*texture, absolutePath);
		texture->SetImportConfig(config);

		// Drop entries whose textures are gone, then remember this one.
		for (auto it = s_LoadedTextures.begin(); it != s_LoadedTextures.end();)
		{
			if (it->second.expired())
			{
				it = s_LoadedTextures.erase(it);
			}
			else
			{
				++it;
			}
		}
		s_LoadedTextures[key] = texture;

		return texture;
	}

	bool Texture2D::IsSupportedFile(const std::filesystem::path& path)
	{
		return GetAssetTypeFromExtension(path) == AssetType::Texture2D;
	}

	std::filesystem::path Texture2D::GetImportFilePath(const std::filesystem::path& imagePath)
	{
		std::filesystem::path importPath = imagePath;
		importPath += TextureImportSerializer::FileExtension;
		return importPath;
	}

	void Texture2D::SetImportConfig(const TextureImportConfig& config)
	{
		m_ImportConfig = config;
		m_Sprites.clear();

		if (config.Mode == SpriteMode::Multiple)
		{
			// Keep only sprites that lie inside the image, so UVs stay in 0..1.
			for (const SpriteRegion& sprite : config.Sprites)
			{
				const bool inside = sprite.Size.x > 0 && sprite.Size.y > 0 &&
					(uint64_t)sprite.Position.x + sprite.Size.x <= m_Specification.Width &&
					(uint64_t)sprite.Position.y + sprite.Size.y <= m_Specification.Height;
				if (inside)
				{
					m_Sprites.push_back(sprite);
				}
				else
				{
					HY_CORE_WARN("Texture2D: '{}': sprite '{}' lies outside the {}x{} image; skipped",
						m_Path.string(), sprite.Name, m_Specification.Width, m_Specification.Height);
				}
			}
		}

		if (m_Sprites.empty())
		{
			SpriteRegion whole;
			whole.Name = m_Path.empty() ? std::string() : m_Path.stem().u8string();
			whole.Size = { m_Specification.Width, m_Specification.Height };
			m_Sprites.push_back(whole);
		}
	}

	const SpriteRegion& Texture2D::GetSprite(uint32_t index) const
	{
		return index < m_Sprites.size() ? m_Sprites[index] : m_Sprites[0];
	}

	int32_t Texture2D::FindSprite(std::string_view name) const
	{
		for (size_t i = 0; i < m_Sprites.size(); i++)
		{
			if (m_Sprites[i].Name == name)
			{
				return (int32_t)i;
			}
		}
		return -1;
	}

	TextureRect Texture2D::GetSpriteUVRect(uint32_t index) const
	{
		const SpriteRegion& sprite = GetSprite(index);

		// A whole-image sprite keeps exact 0..1 so wrap modes behave as before.
		if (sprite.Position == glm::uvec2(0) && sprite.Size == glm::uvec2(m_Specification.Width, m_Specification.Height))
		{
			return {};
		}

		const glm::vec2 imageSize = { (float)std::max(m_Specification.Width, 1u), (float)std::max(m_Specification.Height, 1u) };
		// 1/1000 of a texel in from each edge: invisible, but keeps nearest sampling at the very edge
		// of the quad inside this sprite.
		const glm::vec2 inset = glm::vec2(0.001f) / imageSize;
		TextureRect rect;
		rect.Min = glm::vec2(sprite.Position) / imageSize + inset;
		rect.Max = glm::vec2(sprite.Position + sprite.Size) / imageSize - inset;
		return rect;
	}

	glm::vec2 Texture2D::GetSpriteSizeInUnits(uint32_t index) const
	{
		const float pixelsPerUnit = std::max(m_ImportConfig.PixelsPerUnit, 0.001f);
		return glm::vec2(GetSprite(index).Size) / pixelsPerUnit;
	}
}
