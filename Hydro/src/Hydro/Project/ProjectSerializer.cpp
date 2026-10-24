#include "hypch.h"
#include "ProjectSerializer.h"

#include "Hydro/Utils/YAMLConversions.h"

namespace Hydro
{
	ProjectSerializer::ProjectSerializer(const Ref<Project>& project)
		: m_Project(project)
	{
	}

	bool ProjectSerializer::Serialize(const std::filesystem::path& filepath)
	{
		const ProjectConfig& config = m_Project->GetConfig();
		const RendererSettings& renderer = config.Renderer;

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << config.Name;
		out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.generic_string();
		if ((uint64_t)config.StartScene != 0)
		{
			out << YAML::Key << "StartSceneHandle" << YAML::Value << (uint64_t)config.StartScene;
		}
		else if (config.LegacyStartScene.empty() == false)
		{
			out << YAML::Key << "StartScene" << YAML::Value << config.LegacyStartScene.generic_string();
		}
		out << YAML::EndMap;

		out << YAML::Key << "Renderer" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ClearColor" << YAML::Value << renderer.ClearColor;
		out << YAML::Key << "VSync" << YAML::Value << renderer.VSync;
		out << YAML::Key << "Wireframe" << YAML::Value << renderer.Wireframe;
		out << YAML::Key << "ShaderHotReload" << YAML::Value << renderer.ShaderHotReload;
		out << YAML::EndMap;

		out << YAML::EndMap;

		std::error_code ec;
		if (filepath.has_parent_path())
			std::filesystem::create_directories(filepath.parent_path(), ec);

		std::ofstream fout(filepath);
		if (fout.is_open() == false)
		{
			HY_CORE_ERROR("ProjectSerializer: could not open '{}' for writing", filepath.string());
			return false;
		}

		fout << out.c_str();
		HY_CORE_INFO("Saved project '{}' to {}", config.Name, filepath.string());
		return true;
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("ProjectSerializer: failed to load '{}': {}", filepath.string(), e.what());
			return false;
		}

		YAML::Node projectNode = data["Project"];
		if (projectNode.IsDefined() == false)
		{
			HY_CORE_ERROR("ProjectSerializer: '{}' is not a Hydro project file", filepath.string());
			return false;
		}

		ProjectConfig& config = m_Project->GetConfig();
		try
		{
			if (auto node = projectNode["Name"])
				config.Name = node.as<std::string>();
			if (auto node = projectNode["AssetDirectory"])
				config.AssetDirectory = node.as<std::string>();
			if (auto node = projectNode["StartSceneHandle"])
				config.StartScene = AssetHandle(node.as<uint64_t>(0));
			if (auto node = projectNode["StartScene"])
				config.LegacyStartScene = node.as<std::string>();

			if (YAML::Node rendererNode = data["Renderer"])
			{
				RendererSettings& renderer = config.Renderer;
				if (auto node = rendererNode["ClearColor"])
					renderer.ClearColor = node.as<glm::vec4>();
				if (auto node = rendererNode["VSync"])
					renderer.VSync = node.as<bool>();
				if (auto node = rendererNode["Wireframe"])
					renderer.Wireframe = node.as<bool>();
				if (auto node = rendererNode["ShaderHotReload"])
					renderer.ShaderHotReload = node.as<bool>();
			}
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("ProjectSerializer: bad data in '{}': {}", filepath.string(), e.what());
			return false;
		}

		HY_CORE_INFO("Loaded project '{}' from {}", config.Name, filepath.string());
		return true;
	}
}
