#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Asset/Asset.h"
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

	// Part of a texture in texture coordinates: Min is the top-left corner (v = 0 is the top row), Max the bottom-right.
	struct ImportedTexture;

	struct TextureRect
	{
		glm::vec2 Min{ 0.0f };
		glm::vec2 Max{ 1.0f };
	};

	// An RGBA8 texture asset, also a list of sprites (sprite 0 always exists). Immutable and shared through Ref; its
	// image is freed through the renderer's deletion queue, so dropping the last Ref is safe at any time.
	class Texture2D : public Asset
	{
	public:
		// Uploads Width * Height RGBA8 pixels (first row = top). Format is SRGB (colour, the sprite default) or RGBA
		// (data such as masks); Usage must be ImageUsage::Texture.
		Texture2D(const ImageSpecification& specification, const void* pixels, const TextureProperties& properties = {});
		~Texture2D() override;

		Texture2D(const Texture2D&) = delete;
		Texture2D& operator=(const Texture2D&) = delete;

		// Replaces a rectangle of tightly packed RGBA8 pixels (rows from the image's first). Waits for the GPU, so it is
		// for editor edits only, never every frame.
		void SetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* pixels);

		static AssetType GetStaticType() { return AssetType::Texture2D; }
		AssetType GetType() const override { return GetStaticType(); }

		// A texture from a decoded image (TextureImporter::Import), remembering `absolutePath` as its file.
		static Ref<Texture2D> Create(const ImportedTexture& imported, const std::filesystem::path& absolutePath);

		// Loads an image file; nullptr (logged) when it can't be read. A "<path>.hymeta" overrides properties and format.
		// Files in the project's asset folder are shared by the AssetManager and reloaded when the file or .hymeta changes.
		static Ref<Texture2D> LoadFromFile(const std::filesystem::path& path, const TextureProperties& properties = {},
			ImageFormat format = ImageFormat::SRGB);
		// Loads an image for a short-lived use (thumbnails): never shared or kept, so dropping the Ref frees it.
		static Ref<Texture2D> LoadUnshared(const std::filesystem::path& path);
		// True for file extensions LoadFromFile can read (GetAssetTypeFromExtension gives Texture2D).
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
		// The sprite's texture coordinates, inset a fraction of a texel so nearest sampling never picks a neighbour.
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

		// Sets the import config and builds m_Sprites from it (one whole-image sprite when it has none).
		void SetImportConfig(const TextureImportConfig& config);

		friend const Texture2DData& GetTexture2DData(const Texture2D& texture);
	};
}
