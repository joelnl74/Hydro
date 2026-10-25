#include "hypch.h"
#include "MaterialSerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Renderer/Texture2D.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Hydro
{
	// The type name in the file; the only material type of the MVP.
	static constexpr const char* StandardMaterialType = "Standard";

	// The fields of a material, inside a map the caller opened.
	static void WriteFields(YAML::Emitter& out, const MaterialData& data)
	{
		if ((uint64_t)data.Albedo != 0)
		{
			out << YAML::Key << "AlbedoHandle" << YAML::Value << (uint64_t)data.Albedo;
		}
		if ((uint64_t)data.Emissive != 0)
		{
			out << YAML::Key << "EmissiveHandle" << YAML::Value << (uint64_t)data.Emissive;
		}
		out << YAML::Key << "Tint" << YAML::Value << data.Tint;
		out << YAML::Key << "Tiling" << YAML::Value << data.Tiling;
		out << YAML::Key << "EmissionColor" << YAML::Value << data.EmissionColor;
		out << YAML::Key << "EmissionIntensity" << YAML::Value << data.EmissionIntensity;
		out << YAML::Key << "AlphaCutoff" << YAML::Value << data.AlphaCutoff;
		out << YAML::Key << "Filter" << YAML::Value << (data.Filter == TextureFilter::Linear ? "Linear" : "Nearest");
	}

	void MaterialSerializer::SerializeData(YAML::Emitter& out, const MaterialData& data)
	{
		out << YAML::BeginMap;
		WriteFields(out, data);
		out << YAML::EndMap;
	}

	// The textures are loaded now, so the renderer (which never loads) finds them.
	MaterialData MaterialSerializer::DeserializeData(const YAML::Node& node)
	{
		MaterialData data;
		data.Albedo = AssetHandle(node["AlbedoHandle"].as<uint64_t>(0));
		data.Emissive = AssetHandle(node["EmissiveHandle"].as<uint64_t>(0));
		if ((uint64_t)data.Albedo != 0)
		{
			AssetManager::GetAsset<Texture2D>(data.Albedo);
		}
		if ((uint64_t)data.Emissive != 0)
		{
			AssetManager::GetAsset<Texture2D>(data.Emissive);
		}
		data.Tint = node["Tint"].as<glm::vec4>(data.Tint);
		data.Tiling = node["Tiling"].as<glm::vec2>(data.Tiling);
		data.EmissionColor = node["EmissionColor"].as<glm::vec3>(data.EmissionColor);
		data.EmissionIntensity = node["EmissionIntensity"].as<float>(data.EmissionIntensity);
		data.AlphaCutoff = node["AlphaCutoff"].as<float>(data.AlphaCutoff);
		data.Filter = node["Filter"].as<std::string>("Nearest") == "Linear" ? TextureFilter::Linear : TextureFilter::Nearest;
		return data;
	}

	bool MaterialSerializer::Serialize(const std::filesystem::path& filepath, const MaterialData& data)
	{
		// The material's own handle goes first, like in scenes; a new file gets one here.
		const AssetHandle handle = AssetManager::ImportAsset(filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		if ((uint64_t)handle != 0)
		{
			out << YAML::Key << AssetMetaSerializer::HandleKey << YAML::Value << (uint64_t)handle;
		}
		out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << StandardMaterialType;
		WriteFields(out, data);
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream file(filepath);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("MaterialSerializer: can't write '{}'", filepath.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	bool MaterialSerializer::Deserialize(const std::filesystem::path& filepath, MaterialData& data)
	{
		data = {};

		YAML::Node root;
		try
		{
			root = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("MaterialSerializer: can't read '{}': {}", filepath.string(), e.what());
			return false;
		}

		const YAML::Node material = root["Material"];
		if (material.IsDefined() == false || material.IsMap() == false)
		{
			HY_CORE_ERROR("MaterialSerializer: '{}' has no Material section", filepath.string());
			return false;
		}

		try
		{
			const std::string type = material["Type"].as<std::string>(StandardMaterialType);
			if (type != StandardMaterialType)
			{
				HY_CORE_WARN("MaterialSerializer: '{}' is a '{}' material; read as Standard (the only type so far)", filepath.string(), type);
			}
			data = DeserializeData(material);
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("MaterialSerializer: bad value in '{}': {}", filepath.string(), e.what());
			return false;
		}

		return true;
	}
}
