#include "hypch.h"
#include "MeshImporter.h"
#include "MeshImportSerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Renderer/Material.h"
#include "Hydro/Renderer/MaterialSerializer.h"
#include "Hydro/Renderer/Texture2D.h"

#include <algorithm>
#include <fstream>
#include <iterator>

// The asset side of a model import (MeshImporter::ImportAsset): turning the model's images and materials
// into project assets with stable handles. MeshImporter.cpp holds the file reading and flattening.

namespace Hydro
{
	// A file name part without characters Windows doesn't allow.
	static std::string ToFileNamePart(std::string name)
	{
		for (char& c : name)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
			{
				c = '_';
			}
		}
		return name.empty() ? std::string("unnamed") : name;
	}

	static bool ReadFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes)
	{
		std::ifstream in(path, std::ios::binary);
		if (in.is_open() == false)
		{
			return false;
		}
		bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		return true;
	}

	static bool WriteFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
	{
		std::error_code error;
		std::filesystem::create_directories(path.parent_path(), error);
		std::ofstream out(path, std::ios::binary);
		if (out.is_open() == false)
		{
			HY_CORE_ERROR("MeshImporter: can't write '{}'", path.string());
			return false;
		}
		out.write((const char*)bytes.data(), (std::streamsize)bytes.size());
		return true;
	}

	// The texture asset for image `index`, made the first time: an embedded image is written to
	// "<model>.textures/<index>_<name>.<ext>" byte for byte (rewritten when the model's copy changed); an
	// external file is registered where it is. 0 when there is none (logged).
	static AssetHandle ResolveImage(const ImportedModel& model, uint32_t index, const std::filesystem::path& modelPath, MeshImportMapping& mapping)
	{
		const ImportedImage& image = model.Images[index];

		const auto mapped = mapping.Images.find(index);
		if (mapped != mapping.Images.end() && AssetManager::IsAssetHandleValid(mapped->second))
		{
			if (image.EmbeddedData.empty() == false)
			{
				// The model may have a new version of the image: same file, same handle, new bytes.
				const std::filesystem::path file = AssetManager::GetAbsolutePath(mapped->second);
				std::vector<uint8_t> current;
				if (ReadFileBytes(file, current) == false || current != image.EmbeddedData)
				{
					WriteFileBytes(file, image.EmbeddedData);
				}
			}
			return mapped->second;
		}

		AssetHandle handle = 0;
		if (image.EmbeddedData.empty() == false)
		{
			const std::string extension = MeshImporter::GetImageExtension(image.MimeType);
			if (extension.empty())
			{
				HY_CORE_WARN("MeshImporter: '{}': image '{}' is '{}', which the engine can't decode; its material uses its colour",
					modelPath.filename().string(), image.Name, image.MimeType.empty() ? std::string("of an unknown type") : image.MimeType);
				return 0;
			}

			const std::filesystem::path folder = modelPath.parent_path() / std::filesystem::u8path(modelPath.stem().u8string() + ".textures");
			const std::filesystem::path file = folder / std::filesystem::u8path(std::to_string(index) + "_" + ToFileNamePart(image.Name) + extension);
			if (WriteFileBytes(file, image.EmbeddedData) == false)
			{
				return 0;
			}
			handle = AssetManager::ImportAsset(file);
		}
		else if (image.File.empty() == false)
		{
			handle = AssetManager::ImportAsset(image.File);
			if ((uint64_t)handle == 0)
			{
				HY_CORE_WARN("MeshImporter: '{}': image '{}' ('{}') is missing or outside the project's asset folder; its material uses its colour",
					modelPath.filename().string(), image.Name, image.File.string());
				return 0;
			}
		}
		else
		{
			return 0;
		}

		if ((uint64_t)handle != 0)
		{
			mapping.Images[index] = handle;
		}
		return handle;
	}

	// The material asset for material `index`, made the first time as "<model>_<name>.hymat" next to the
	// model. An existing one is never rewritten: it may have been edited.
	static AssetHandle ResolveMaterial(const ImportedModel& model, uint32_t index, const std::filesystem::path& modelPath, MeshImportMapping& mapping)
	{
		const auto mapped = mapping.Materials.find(index);
		if (mapped != mapping.Materials.end() && AssetManager::IsAssetHandleValid(mapped->second))
		{
			return mapped->second;
		}

		const ImportedMaterial& source = model.Materials[index];
		MaterialData material;
		material.Tint = source.BaseColor;
		material.Tiling = source.Tiling;
		material.EmissionColor = source.Emissive;
		material.EmissionIntensity = source.EmissiveStrength;
		material.AlphaCutoff = source.AlphaCutoff;
		material.Filter = source.LinearFilter ? TextureFilter::Linear : TextureFilter::Nearest;
		if (source.BaseColorImage >= 0 && source.BaseColorImage < (int32_t)model.Images.size())
		{
			material.Albedo = ResolveImage(model, (uint32_t)source.BaseColorImage, modelPath, mapping);
		}
		if (source.EmissiveImage >= 0 && source.EmissiveImage < (int32_t)model.Images.size())
		{
			material.Emissive = ResolveImage(model, (uint32_t)source.EmissiveImage, modelPath, mapping);
		}

		const std::filesystem::path folder = modelPath.parent_path();
		const std::string stem = modelPath.stem().u8string() + "_" + ToFileNamePart(source.Name);
		std::filesystem::path file = folder / std::filesystem::u8path(stem + MaterialSerializer::FileExtension);
		std::error_code error;
		for (int number = 2; std::filesystem::exists(file, error); number++)
		{
			file = folder / std::filesystem::u8path(stem + " " + std::to_string(number) + MaterialSerializer::FileExtension);
		}
		if (MaterialSerializer::Serialize(file, material) == false)
		{
			return 0;
		}

		const AssetHandle handle = AssetManager::FindHandle(file);
		if ((uint64_t)handle != 0)
		{
			mapping.Materials[index] = handle;
			HY_CORE_INFO("MeshImporter: made {} for '{}'", file.filename().u8string(), modelPath.filename().u8string());
		}
		return handle;
	}

	Ref<Mesh> MeshImporter::ImportAsset(const std::filesystem::path& path)
	{
		const std::filesystem::path sidecar = AssetMetaSerializer::GetSidecarPath(path);
		MeshImportConfig config;
		MeshImportMapping mapping;
		MeshImportSerializer::Deserialize(sidecar, config, mapping);

		ImportedModel model;
		if (Import(path, model) == false)
		{
			return nullptr;
		}

		MeshData data = Flatten(model, config);
		if (data.Vertices.empty() || data.Indices.empty())
		{
			HY_CORE_ERROR("MeshImporter: '{}' has no triangles in its scene", path.string());
			return nullptr;
		}

		// The materials (and their textures) become assets only for models in the open project.
		const bool inProject = (uint64_t)AssetManager::FindHandle(path) != 0;
		const MeshImportMapping before = mapping;

		uint32_t slotCount = 0;
		for (const SubMesh& subMesh : data.SubMeshes)
		{
			slotCount = std::max(slotCount, subMesh.MaterialSlot + 1);
		}

		std::vector<MeshMaterialInfo> materials(slotCount);
		for (uint32_t slot = 0; slot < slotCount; slot++)
		{
			if (slot >= model.Materials.size())
			{
				materials[slot].Name = "Default";
				continue;
			}
			materials[slot].Name = model.Materials[slot].Name;
			if (inProject)
			{
				materials[slot].DefaultMaterial = ResolveMaterial(model, slot, path, mapping);
				if ((uint64_t)materials[slot].DefaultMaterial != 0)
				{
					// Loaded now (with its textures), so the renderer, which never loads, finds it.
					AssetManager::GetAsset<MaterialAsset>(materials[slot].DefaultMaterial);
				}
			}
		}

		std::error_code existsError;
		if (inProject && (mapping.Images != before.Images || mapping.Materials != before.Materials || std::filesystem::exists(sidecar, existsError) == false))
		{
			MeshImportSerializer::Serialize(sidecar, config, mapping);
		}

		Ref<Mesh> mesh = CreateRef<Mesh>(data, path.stem().u8string());
		mesh->SetMaterials(materials);
		HY_CORE_INFO("MeshImporter: imported '{}': {} sub-meshes, {} triangles, {} materials", path.filename().u8string(),
			mesh->GetSubMeshes().size(), mesh->GetIndexCount() / 3, slotCount);
		return mesh;
	}
}
