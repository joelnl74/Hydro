#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Image.h"
#include "Hydro/Renderer/TextureImport.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace Hydro
{
	struct Texture2DData;

	// Part of a texture in texture coordinates: Min is the top-left corner (v = 0 is the image's first,
	// top row), Max the bottom-right. Renderer2D::DrawQuad(transform, texture, rect) draws it.
	struct TextureRect
	{
		glm::vec2 Min{ 0.0f };
		glm::vec2 Max{ 1.0f };
	};

	// A 2D texture (RGBA8) that sprites and quads are drawn with: Renderer2D::DrawQuad(transform, texture)
	// or SpriteRendererComponent::Texture.
	//
	// A resource: made from an ImageSpecification (what it is: size, RGBA or SRGB) and TextureProperties
	// (how it is sampled: filter, wrap), immutable afterwards, and non-copyable. Textures are shared by
	// many sprites, so they are held in a Ref<Texture2D>. Its image is freed through the renderer's
	// deletion queue (Renderer::SubmitResourceFree) after the frames in flight that may sample it, so
	// dropping the last Ref at any time is safe.
	//
	// Sprites: every texture is also a list of sprites (TextureImportConfig). A Single texture has one,
	// the whole image; a Multiple texture (a sprite sheet) has the named regions it was sliced into.
	// SpriteRendererComponent::SpriteIndex picks one. Sprite 0 always exists.
	//
	// TODO(assets): sprites reference textures directly and scenes save the file path. Once the
	// AssetManager exists (Roadmap "Asset system"), components hold an AssetHandle instead and
	// LoadFromFile's sharing moves into it.
	class Texture2D
	{
	public:
		// Uploads specification.Width * specification.Height RGBA8 pixels (4 bytes each, first row = top
		// of the image). specification.Format is ImageFormat::SRGB (colour, the default for sprites) or
		// ImageFormat::RGBA (data such as masks); Usage must be ImageUsage::Texture.
		Texture2D(const ImageSpecification& specification, const void* pixels, const TextureProperties& properties = {});
		~Texture2D();

		Texture2D(const Texture2D&) = delete;
		Texture2D& operator=(const Texture2D&) = delete;

		// Loads an image file (PNG, JPG, TGA, BMP; any channel count, converted to RGBA8). Returns nullptr
		// and logs an error when the file can't be read.
		// When "<path>.hymeta" exists, its TextureImportConfig decides format, sampling, colour key and
		// sprites, and properties/format are ignored; otherwise they are used, with one sprite.
		// While a texture loaded from the same file with the same settings (and the same .hymeta
		// version) is alive, that texture is returned instead of loading it again, so saving a new
		// .hymeta and calling LoadFromFile again re-imports.
		static Ref<Texture2D> LoadFromFile(const std::filesystem::path& path, const TextureProperties& properties = {}, ImageFormat format = ImageFormat::SRGB);
		// True for file extensions LoadFromFile can read.
		static bool IsSupportedFile(const std::filesystem::path& path);
		// "<image path>.hymeta": where an image's import config is saved.
		static std::filesystem::path GetImportFilePath(const std::filesystem::path& imagePath);

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }
		const ImageSpecification& GetSpecification() const { return m_Specification; }
		const TextureProperties& GetProperties() const { return m_Properties; }
		// The file it was loaded from (absolute), or empty for textures made in code.
		const std::filesystem::path& GetPath() const { return m_Path; }

		// How it was imported (defaults for textures made in code).
		const TextureImportConfig& GetImportConfig() const { return m_ImportConfig; }
		SpriteMode GetSpriteMode() const { return m_ImportConfig.Mode; }
		// Always at least 1. An index out of range means sprite 0 in the functions below.
		uint32_t GetSpriteCount() const { return (uint32_t)m_Sprites.size(); }
		const SpriteRegion& GetSprite(uint32_t index) const;
		// -1 when there is no sprite with that name.
		int32_t FindSprite(std::string_view name) const;
		// The sprite's texture coordinates, inset by a tiny fraction of a texel so nearest sampling at
		// the quad's edge never picks the neighbouring sprite. A whole-image sprite is (0,0)-(1,1).
		TextureRect GetSpriteUVRect(uint32_t index) const;
		// Size in world units: pixels / PixelsPerUnit.
		glm::vec2 GetSpriteSizeInUnits(uint32_t index) const;

	private:
		ImageSpecification m_Specification;
		TextureProperties m_Properties;
		std::filesystem::path m_Path;
		TextureImportConfig m_ImportConfig;
		std::vector<SpriteRegion> m_Sprites; // never empty
		Scope<Texture2DData> m_Data; // the backend image (Platform/Vulkan/VulkanTexture2DData.h)

		// Sets the import config and builds m_Sprites from it (one sprite for the whole image when it
		// has none). Called by the constructor (defaults) and LoadFromFile.
		void SetImportConfig(const TextureImportConfig& config);

		friend const Texture2DData& GetTexture2DData(const Texture2D& texture);
	};
}
