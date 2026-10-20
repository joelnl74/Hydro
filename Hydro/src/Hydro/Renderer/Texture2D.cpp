#include "hypch.h"
#include "Texture2D.h"

#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"

#include <stb_image.h>

#include <cctype>
#include <string>
#include <unordered_map>

namespace Hydro
{
	// Textures loaded from files that are still alive, so many sprites using one file share one texture.
	// Weak: the cache never keeps a texture (or GPU memory) alive by itself.
	// TODO(assets): replaced by the AssetManager's handle -> asset table.
	static std::unordered_map<std::string, std::weak_ptr<Texture2D>> s_LoadedTextures;

	static std::string MakeCacheKey(const std::filesystem::path& path, const TextureProperties& properties, ImageFormat format)
	{
		return path.generic_u8string() + "|" + std::to_string((int)format) + "|" +
			std::to_string((int)properties.SamplerFilter) + "|" + std::to_string((int)properties.SamplerWrap);
	}

	Texture2D::Texture2D(const ImageSpecification& specification, const void* pixels, const TextureProperties& properties)
		: m_Specification(specification), m_Properties(properties), m_Data(CreateScope<Texture2DData>())
	{
		HY_CORE_ASSERT(specification.Usage == ImageUsage::Texture, "Texture2D: Usage must be ImageUsage::Texture");
		m_Specification.Usage = ImageUsage::Texture;
		m_Specification.Mips = 1;
		m_Specification.Layers = 1;

		m_Data->Image = CreateScope<VulkanImage>(m_Specification, pixels, m_Properties);
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

		const std::string key = MakeCacheKey(absolutePath, properties, format);
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

		ImageSpecification specification;
		specification.Width = (uint32_t)width;
		specification.Height = (uint32_t)height;
		specification.Format = format;
		specification.Usage = ImageUsage::Texture;

		Ref<Texture2D> texture = CreateRef<Texture2D>(specification, pixels, properties);
		stbi_image_free(pixels);
		texture->m_Path = absolutePath;

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
		std::string extension = path.extension().string();
		for (char& c : extension)
		{
			c = (char)std::tolower((unsigned char)c);
		}

		return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp";
	}
}
