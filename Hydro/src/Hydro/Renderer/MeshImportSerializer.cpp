#include "hypch.h"
#include "MeshImportSerializer.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Hydro
{
	static void WriteMap(YAML::Emitter& out, const char* key, const std::map<uint32_t, AssetHandle>& map)
	{
		out << YAML::Key << key << YAML::Value << YAML::BeginMap;
		for (const auto& [index, handle] : map)
		{
			out << YAML::Key << index << YAML::Value << (uint64_t)handle;
		}
		out << YAML::EndMap;
	}

	static void ReadMap(const YAML::Node& node, std::map<uint32_t, AssetHandle>& map)
	{
		map.clear();
		if (node.IsDefined() == false || node.IsMap() == false)
		{
			return;
		}
		for (const auto& item : node)
		{
			const uint64_t handle = item.second.as<uint64_t>(0);
			if (handle != 0)
			{
				map[item.first.as<uint32_t>()] = AssetHandle(handle);
			}
		}
	}

	bool MeshImportSerializer::Serialize(const std::filesystem::path& sidecar, const MeshImportConfig& config, const MeshImportMapping& mapping)
	{
		// The file is also the model's asset sidecar: keep its Handle and Type (AssetMetaSerializer).
		YAML::Node existing;
		std::error_code existsError;
		if (std::filesystem::exists(sidecar, existsError))
		{
			try
			{
				existing = YAML::LoadFile(sidecar.string());
			}
			catch (const YAML::Exception&)
			{
				// Not valid YAML: rewritten without a handle; the next asset scan gives the model a new one.
			}
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		if (existing.IsMap())
		{
			for (const char* key : { "Handle", "Type" })
			{
				if (auto node = existing[key])
				{
					out << YAML::Key << key << YAML::Value << node;
				}
			}
		}
		out << YAML::Key << "MeshImport" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Scale" << YAML::Value << config.Scale;
		out << YAML::Key << "ConvertYUpToZUp" << YAML::Value << config.ConvertYUpToZUp;
		out << YAML::Key << "GenerateNormals" << YAML::Value << config.GenerateNormals;
		WriteMap(out, "Images", mapping.Images);
		WriteMap(out, "Materials", mapping.Materials);
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream file(sidecar);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("MeshImportSerializer: can't write '{}'", sidecar.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	bool MeshImportSerializer::Deserialize(const std::filesystem::path& sidecar, MeshImportConfig& config, MeshImportMapping& mapping)
	{
		config = {};
		mapping = {};

		std::error_code existsError;
		if (std::filesystem::exists(sidecar, existsError) == false)
		{
			return false;
		}

		try
		{
			const YAML::Node data = YAML::LoadFile(sidecar.string());
			const YAML::Node root = data["MeshImport"];
			if (root.IsDefined() == false || root.IsMap() == false)
			{
				// Not an error: a sidecar with only the handle means the defaults and nothing imported yet.
				return false;
			}

			config.Scale = root["Scale"].as<float>(config.Scale);
			config.ConvertYUpToZUp = root["ConvertYUpToZUp"].as<bool>(config.ConvertYUpToZUp);
			config.GenerateNormals = root["GenerateNormals"].as<bool>(config.GenerateNormals);
			ReadMap(root["Images"], mapping.Images);
			ReadMap(root["Materials"], mapping.Materials);
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("MeshImportSerializer: can't read '{}': {}", sidecar.string(), e.what());
			config = {};
			mapping = {};
			return false;
		}

		return true;
	}
}
