#include "hypch.h"
#include "Texture2D.h"
#include "TextureImportSerializer.h"
#include "TextureImporter.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"

#include <string>
#include <unordered_map>

namespace Hydro
{
	// Textures loaded from files outside the asset folder that are still alive, so many sprites using one
	// file share one texture. Weak: the cache never keeps a texture (or GPU memory) alive by itself.
	// Images inside the asset folder are shared by the AssetManager instead.
	static std::unordered_map<std::string, std::weak_ptr<Texture2D>> s_LoadedTextures;

	// The .hymeta's modification time is part of the key, so a re-saved import config re-imports
	// (sprites still holding the old texture keep it until they are given the new one).
	static std::string MakeCacheKey(const std::filesystem::path& path, const TextureProperties& properties, ImageFormat format, int64_t importFileTime)
	{
		return path.generic_u8string() + "|" + std::to_string((int)format) + "|" +
			std::to_string((int)properties.SamplerFilter) + "|" + std::to_string((int)properties.SamplerWrap) + "|" +
			std::to_string(importFileTime);
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

	Ref<Texture2D> Texture2D::Create(const ImportedTexture& imported, const std::filesystem::path& absolutePath)
	{
		Ref<Texture2D> texture = CreateRef<Texture2D>(imported.Specification, imported.Pixels.data(), imported.Config.Properties);
		texture->m_Path = absolutePath;
		texture->SetImportConfig(imported.Config);
		return texture;
	}

	Ref<Texture2D> Texture2D::LoadFromFile(const std::filesystem::path& path, const TextureProperties& properties, ImageFormat format)
	{
		std::error_code error;
		std::filesystem::path absolutePath = std::filesystem::absolute(path, error).lexically_normal();
		if (error)
		{
			absolutePath = path;
		}

		// An image in the open project's asset folder is an asset: the AssetManager loads it once and
		// shares it (its .hymeta decides how), until the project closes.
		const AssetHandle handle = AssetManager::ImportAsset(absolutePath);
		if ((uint64_t)handle != 0)
		{
			return AssetManager::GetAsset<Texture2D>(handle);
		}

		// Anywhere else: loaded here and shared while alive.
		TextureImportConfig fallback;
		fallback.Format = format;
		fallback.Properties = properties;
		const int64_t importFileTime = TextureImporter::GetImportConfigVersion(absolutePath);

		// The settings used (the .hymeta's when there is one) are part of the key.
		TextureImportConfig used = fallback;
		if (importFileTime != 0)
		{
			TextureImportConfig fromFile;
			if (TextureImportSerializer::Deserialize(GetImportFilePath(absolutePath), fromFile))
			{
				used = fromFile;
			}
		}

		const std::string key = MakeCacheKey(absolutePath, used.Properties, used.Format, importFileTime);
		if (auto found = s_LoadedTextures.find(key); found != s_LoadedTextures.end())
		{
			if (Ref<Texture2D> texture = found->second.lock())
			{
				return texture;
			}
		}

		ImportedTexture imported;
		if (TextureImporter::Import(absolutePath, fallback, imported) == false)
		{
			return nullptr;
		}

		Ref<Texture2D> texture = Create(imported, absolutePath);

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
