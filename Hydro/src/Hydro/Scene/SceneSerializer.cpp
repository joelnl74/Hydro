#include "hypch.h"
#include "SceneSerializer.h"
#include "EntitySerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Renderer/RenderFeatureSerializer.h"
#include "Hydro/Utils/YAMLConversions.h"

namespace Hydro
{
	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	bool SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		// The file's asset handle comes first (AssetMetaSerializer reads only the first line): the one it
		// already has when the file is overwritten, a new one for a new file.
		const AssetHandle handle = AssetManager::ImportAsset(filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << AssetMetaSerializer::HandleKey << YAML::Value << (uint64_t)handle;
		out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
		// Only written when the scene overrides the project's render features, so 2D scenes stay as they were.
		const SceneRenderSettings& renderSettings = m_Scene->GetRenderSettings();
		if (renderSettings.OverrideProject)
		{
			out << YAML::Key << "RenderSettings" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "OverrideProject" << YAML::Value << true;
			out << YAML::Key << "Features" << YAML::Value;
			RenderFeatureSerializer::Serialize(out, renderSettings.Features);
			out << YAML::EndMap;
		}
		// Only written when the scene sets its own ambient and fog (forward lighting, M6).
		const SceneEnvironment& environment = m_Scene->GetEnvironment();
		if (environment.Override)
		{
			out << YAML::Key << "Environment" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "SkyAmbient" << YAML::Value << environment.SkyAmbient;
			out << YAML::Key << "GroundAmbient" << YAML::Value << environment.GroundAmbient;
			out << YAML::Key << "Fog" << YAML::Value << environment.Fog;
			out << YAML::Key << "FogColor" << YAML::Value << environment.FogColor;
			out << YAML::Key << "FogStart" << YAML::Value << environment.FogStart;
			out << YAML::Key << "FogEnd" << YAML::Value << environment.FogEnd;
			out << YAML::EndMap;
		}
		// Only when it differs from the defaults, so scenes that don't use it are saved as before.
		const ScenePostProcess& postProcess = m_Scene->GetPostProcess();
		if (postProcess != ScenePostProcess{})
		{
			out << YAML::Key << "PostProcess" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Exposure" << YAML::Value << postProcess.Exposure;
			out << YAML::Key << "Bloom" << YAML::Value << postProcess.Bloom;
			out << YAML::Key << "BloomThreshold" << YAML::Value << postProcess.BloomThreshold;
			out << YAML::Key << "BloomKnee" << YAML::Value << postProcess.BloomKnee;
			out << YAML::Key << "BloomIntensity" << YAML::Value << postProcess.BloomIntensity;
			out << YAML::Key << "BloomRadius" << YAML::Value << postProcess.BloomRadius;
			out << YAML::EndMap;
		}
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		// The scene's list order (what the hierarchy shows). Only "real" entities (made through
		// CreateEntity) are in it; raw registry entities without an ID, like the editor's demo
		// galaxy stars, are runtime-only.
		const std::vector<entt::entity>& entities = m_Scene->GetEntityOrder();

		for (entt::entity handle : entities)
		{
			EntitySerializer::Serialize(out, Entity{ handle, m_Scene.get() });
		}

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::error_code ec;
		if (filepath.has_parent_path())
			std::filesystem::create_directories(filepath.parent_path(), ec);

		std::ofstream fout(filepath);
		if (fout.is_open() == false)
		{
			HY_CORE_ERROR("SceneSerializer: could not open '{}' for writing", filepath.string());
			return false;
		}

		fout << out.c_str();
		HY_CORE_INFO("Saved scene '{}' ({} entities) to {}", m_Scene->GetName(), entities.size(), filepath.string());
		return true;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("SceneSerializer: failed to load '{}': {}", filepath.string(), e.what());
			return false;
		}

		if (data["Scene"].IsDefined() == false)
		{
			HY_CORE_ERROR("SceneSerializer: '{}' is not a Hydro scene file", filepath.string());
			return false;
		}

		m_Scene->SetName(data["Scene"].as<std::string>());

		// Missing: the scene uses the project's render features.
		SceneRenderSettings renderSettings;
		YAML::Node renderNode = data["RenderSettings"];
		if (renderNode && renderNode.IsMap())
		{
			if (auto overrideNode = renderNode["OverrideProject"])
			{
				renderSettings.OverrideProject = overrideNode.as<bool>(false);
			}
			if (auto featuresNode = renderNode["Features"])
			{
				RenderFeatureSerializer::Deserialize(featuresNode, renderSettings.Features);
			}
		}
		m_Scene->SetRenderSettings(renderSettings);

		// Missing: the Global lights are the ambient light and there is no fog.
		SceneEnvironment environment;
		YAML::Node environmentNode = data["Environment"];
		if (environmentNode && environmentNode.IsMap())
		{
			environment.Override = true;
			environment.SkyAmbient = environmentNode["SkyAmbient"].as<glm::vec3>(environment.SkyAmbient);
			environment.GroundAmbient = environmentNode["GroundAmbient"].as<glm::vec3>(environment.GroundAmbient);
			environment.Fog = environmentNode["Fog"].as<bool>(environment.Fog);
			environment.FogColor = environmentNode["FogColor"].as<glm::vec3>(environment.FogColor);
			environment.FogStart = environmentNode["FogStart"].as<float>(environment.FogStart);
			environment.FogEnd = environmentNode["FogEnd"].as<float>(environment.FogEnd);
		}
		m_Scene->SetEnvironment(environment);

		ScenePostProcess postProcess;
		YAML::Node postProcessNode = data["PostProcess"];
		if (postProcessNode && postProcessNode.IsMap())
		{
			if (auto exposure = postProcessNode["Exposure"])
			{
				postProcess.Exposure = exposure.as<float>(postProcess.Exposure);
			}
			if (auto bloom = postProcessNode["Bloom"])
			{
				postProcess.Bloom = bloom.as<bool>(postProcess.Bloom);
			}
			if (auto threshold = postProcessNode["BloomThreshold"])
			{
				postProcess.BloomThreshold = threshold.as<float>(postProcess.BloomThreshold);
			}
			if (auto knee = postProcessNode["BloomKnee"])
			{
				postProcess.BloomKnee = knee.as<float>(postProcess.BloomKnee);
			}
			if (auto intensity = postProcessNode["BloomIntensity"])
			{
				postProcess.BloomIntensity = intensity.as<float>(postProcess.BloomIntensity);
			}
			if (auto radius = postProcessNode["BloomRadius"])
			{
				postProcess.BloomRadius = radius.as<float>(postProcess.BloomRadius);
			}
		}
		m_Scene->SetPostProcess(postProcess);

		size_t count = 0;
		if (auto entities = data["Entities"])
		{
			try
			{
				for (auto entityNode : entities)
				{
					EntitySerializer::Deserialize(entityNode, *m_Scene, true);
					count++;
				}
			}
			catch (const YAML::Exception& e)
			{
				HY_CORE_ERROR("SceneSerializer: bad entity data in '{}': {}", filepath.string(), e.what());
				return false;
			}
		}

		HY_CORE_INFO("Loaded scene '{}' ({} entities) from {}", m_Scene->GetName(), count, filepath.string());
		return true;
	}
}
