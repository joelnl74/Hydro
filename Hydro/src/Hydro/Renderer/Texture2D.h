#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Asset/Asset.h"
#include "Hydro/Renderer/Image.h"

#include <cstdint>
#include <filesystem>

namespace Hydro
{
	struct Texture2DData;

	// A 2D texture (RGBA8) that sprites and quads are drawn with: Renderer2D::DrawQuad(transform, texture)
	// or SpriteRendererComponent::Texture.
	//
	// A resource: made from an ImageSpecification (what it is: size, RGBA or SRGB) and TextureProperties
	// (how it is sampled: filter, wrap), immutable afterwards, and non-copyable. Textures are shared by
	// many sprites, so they are held in a Ref<Texture2D>. Its image is freed through the renderer's
	// deletion queue (Renderer::SubmitResourceFree) after the frames in flight that may sample it, so
	// dropping the last Ref at any time is safe.
	//
	// An Asset (AssetType::Texture2D): every image in a project's asset folder has an AssetHandle.
	// TODO(assets): sprites still reference textures directly and scenes save the file path; with the
	// texture step of the asset system, components hold an AssetHandle, the AssetManager loads textures
	// (GetAsset<Texture2D>) and LoadFromFile's sharing moves into it. Until then GetHandle() is 0.
	class Texture2D : public Asset
	{
	public:
		// Uploads specification.Width * specification.Height RGBA8 pixels (4 bytes each, first row = top
		// of the image). specification.Format is ImageFormat::SRGB (colour, the default for sprites) or
		// ImageFormat::RGBA (data such as masks); Usage must be ImageUsage::Texture.
		Texture2D(const ImageSpecification& specification, const void* pixels, const TextureProperties& properties = {});
		~Texture2D() override;

		Texture2D(const Texture2D&) = delete;
		Texture2D& operator=(const Texture2D&) = delete;

		static AssetType GetStaticType() { return AssetType::Texture2D; }
		AssetType GetType() const override { return GetStaticType(); }

		// Loads an image file (PNG, JPG, TGA, BMP; any channel count, converted to RGBA8). Returns nullptr
		// and logs an error when the file can't be read. While a texture loaded from the same file with
		// the same format and properties is alive, that texture is returned instead of loading it again.
		static Ref<Texture2D> LoadFromFile(const std::filesystem::path& path, const TextureProperties& properties = {}, ImageFormat format = ImageFormat::SRGB);
		// True for file extensions LoadFromFile can read (GetAssetTypeFromExtension gives Texture2D).
		static bool IsSupportedFile(const std::filesystem::path& path);

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }
		const ImageSpecification& GetSpecification() const { return m_Specification; }
		const TextureProperties& GetProperties() const { return m_Properties; }
		// The file it was loaded from (absolute), or empty for textures made in code.
		const std::filesystem::path& GetPath() const { return m_Path; }

	private:
		ImageSpecification m_Specification;
		TextureProperties m_Properties;
		std::filesystem::path m_Path;
		Scope<Texture2DData> m_Data; // the backend image (Platform/Vulkan/VulkanTexture2DData.h)

		friend const Texture2DData& GetTexture2DData(const Texture2D& texture);
	};
}
