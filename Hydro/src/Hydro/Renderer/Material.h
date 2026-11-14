#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Image.h"

#include <glm/glm.hpp>

#include <filesystem>

namespace Hydro
{
	// The Standard material: albedo times tint, an optional normal map, emission, opaque or cut out. Plain data, inline
	// on a mesh slot or saved as a .hymat; Renderer3D caches GPU state per distinct MaterialData.
	struct MaterialData
	{
		// A game's fragment shader (.frag asset) given these fields and textures instead of the Standard shader (0);
		// runs on mesh.vert, interface in mesh_material.glsl (docs/Architecture.md, "Material shaders").
		AssetHandle Shader = 0;
		AssetHandle Albedo = 0;   // a Texture2D; 0 = white
		AssetHandle Emissive = 0; // a Texture2D multiplied by EmissionColor * EmissionIntensity; 0 = white
		// A tangent-space normal map (imported as RGBA data, green up the image, glTF style), sampled at the albedo's UV.
		// 0 = the surface's own normal. scripts/game/textures/make_normal_maps.py makes them from albedos.
		AssetHandle Normal = 0;
		float NormalStrength = 1.0f; // 0 = flat, 1 = as the map says, 2 = twice as steep
		glm::vec4 Tint{ 1.0f };   // linear RGBA, multiplies the albedo
		glm::vec2 Tiling{ 1.0f }; // texture repeats per metre (built-in shapes have UVs in metres)
		glm::vec3 EmissionColor{ 0.0f }; // linear RGB; black = no emission
		float EmissionIntensity = 1.0f;
		// Pixels whose albedo alpha is below this are dropped (cut-out: railings, leaves). 0 = opaque.
		float AlphaCutoff = 0.0f;
		TextureFilter Filter = TextureFilter::Nearest; // nearest keeps pixel art crisp

		bool operator==(const MaterialData& other) const
		{
			return Shader == other.Shader && Albedo == other.Albedo && Emissive == other.Emissive && Normal == other.Normal
				&& NormalStrength == other.NormalStrength
				&& Tint == other.Tint && Tiling == other.Tiling
				&& EmissionColor == other.EmissionColor && EmissionIntensity == other.EmissionIntensity
				&& AlphaCutoff == other.AlphaCutoff && Filter == other.Filter;
		}
		bool operator!=(const MaterialData& other) const { return (*this == other) == false; }
	};

	// A .hymat material asset shared by every mesh slot that names it. Only data, so it is editable: SetData changes
	// every mesh using it at once.
	class MaterialAsset : public Asset
	{
	public:
		explicit MaterialAsset(const MaterialData& data)
			: m_Data(data) {}

		MaterialAsset(const MaterialAsset&) = delete;
		MaterialAsset& operator=(const MaterialAsset&) = delete;

		// Reads a .hymat and loads its textures, unshared (the AssetManager's loader). nullptr (logged) on failure.
		static Ref<MaterialAsset> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Material; }
		AssetType GetType() const override { return GetStaticType(); }

		const MaterialData& GetData() const { return m_Data; }
		void SetData(const MaterialData& data) { m_Data = data; }

	private:
		MaterialData m_Data;
	};
}
