#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Image.h"

#include <glm/glm.hpp>

#include <filesystem>

namespace Hydro
{
	// The Standard material (plan M4): albedo times tint, plus emission, opaque or cut out. The MVP's only
	// material type; lit by M6's forward lights, otherwise unlit. Plain data: inline on a MeshComponent's
	// material slot, or saved as a material asset (.hymat, MaterialAsset). Renderer3D turns it into GPU
	// state (a cached uniform buffer and descriptor set per distinct MaterialData).
	struct MaterialData
	{
		AssetHandle Albedo = 0;   // a Texture2D; 0 = white
		AssetHandle Emissive = 0; // a Texture2D multiplied by EmissionColor * EmissionIntensity; 0 = white
		glm::vec4 Tint{ 1.0f };   // linear RGBA, multiplies the albedo
		glm::vec2 Tiling{ 1.0f }; // texture repeats per metre (built-in shapes have UVs in metres)
		glm::vec3 EmissionColor{ 0.0f }; // linear RGB; black = no emission
		float EmissionIntensity = 1.0f;
		// Pixels whose albedo alpha is below this are dropped (cut-out: railings, leaves). 0 = opaque.
		float AlphaCutoff = 0.0f;
		TextureFilter Filter = TextureFilter::Nearest; // nearest keeps pixel art crisp

		bool operator==(const MaterialData& other) const
		{
			return Albedo == other.Albedo && Emissive == other.Emissive && Tint == other.Tint && Tiling == other.Tiling
				&& EmissionColor == other.EmissionColor && EmissionIntensity == other.EmissionIntensity
				&& AlphaCutoff == other.AlphaCutoff && Filter == other.Filter;
		}
		bool operator!=(const MaterialData& other) const { return (*this == other) == false; }
	};

	// A material asset (plan M4b): a .hymat file (MaterialSerializer) holding one MaterialData, shared by
	// every mesh slot that names its handle. The AssetManager loads it (AssetManager::GetAsset<MaterialAsset>),
	// together with its textures, and keeps it until the project closes or the file changes.
	//
	// Unlike GPU resources it is editable: it is only data, and Renderer3D builds GPU state from whatever
	// data a mesh resolves to each frame. The editor changes it with SetData (every mesh using it changes at
	// once) and then saves the file; loading the saved file gives the same data.
	class MaterialAsset : public Asset
	{
	public:
		explicit MaterialAsset(const MaterialData& data)
			: m_Data(data) {}

		MaterialAsset(const MaterialAsset&) = delete;
		MaterialAsset& operator=(const MaterialAsset&) = delete;

		// Reads a .hymat file and loads its textures, without sharing: every call makes a new asset. Returns
		// nullptr (logged) when the file can't be read. The AssetManager's loader calls this.
		static Ref<MaterialAsset> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Material; }
		AssetType GetType() const override { return GetStaticType(); }

		const MaterialData& GetData() const { return m_Data; }
		void SetData(const MaterialData& data) { m_Data = data; }

	private:
		MaterialData m_Data;
	};
}
