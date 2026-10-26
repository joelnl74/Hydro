#include "hypch.h"
#include "SceneSerializer.h"
#include "EntitySerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Renderer/RenderFeatureSerializer.h"
#include "Hydro/Renderer/RenderProfileSerializer.h"
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
		// Only written when the scene uses a render profile (post-processing plan P4).
		const SceneLook& look = m_Scene->GetLook();
		if (look != SceneLook{})
		{
			out << YAML::Key << "Look" << YAML::Value << YAML::BeginMap;
			if ((uint64_t)look.Profile != 0)
			{
				out << YAML::Key << "ProfileHandle" << YAML::Value << (uint64_t)look.Profile;
			}
			out << YAML::Key << "Environment" << YAML::Value << RenderProfileSerializer::LookSourceToString(look.Environment);
			out << YAML::Key << "PostProcess" << YAML::Value << RenderProfileSerializer::LookSourceToString(look.PostProcess);
			out << YAML::EndMap;
		}
		// The scene's own values, also when the look takes them from a profile (they come back when it
		// stops). Only written when the scene sets its own ambient and fog (forward lighting, M6).
		const SceneEnvironment& environment = m_Scene->GetEnvironment();
		if (environment.Override)
		{
			out << YAML::Key << "Environment" << YAML::Value;
			RenderProfileSerializer::SerializeEnvironment(out, environment);
		}
		// Only when it differs from the defaults, so scenes that don't use it are saved as before.
		const ScenePostProcess& postProcess = m_Scene->GetPostProcess();
		if (postProcess != ScenePostProcess{})
		{
			out << YAML::Key << "PostProcess" << YAML::Value;
			RenderProfileSerializer::SerializePostProcess(out, postProcess);
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
		m_Scene->SetEnvironment(RenderProfileSerializer::DeserializeEnvironment(data["Environment"]));
		m_Scene->SetPostProcess(RenderProfileSerializer::DeserializePostProcess(data["PostProcess"]));

		// Missing: no profile, the scene's own values (scenes from before P4).
		SceneLook look;
		YAML::Node lookNode = data["Look"];
		if (lookNode && lookNode.IsMap())
		{
			if (auto profileNode = lookNode["ProfileHandle"])
			{
				look.Profile = AssetHandle(profileNode.as<uint64_t>(0));
			}
			if (auto environmentSource = lookNode["Environment"])
			{
				look.Environment = RenderProfileSerializer::LookSourceFromString(environmentSource.as<std::string>("Scene"));
			}
			if (auto postProcessSource = lookNode["PostProcess"])
			{
				look.PostProcess = RenderProfileSerializer::LookSourceFromString(postProcessSource.as<std::string>("Scene"));
			}
		}
		// Loaded now (a profile that can't be loaded is logged by the manager), so the renderer finds it.
		// The handle is kept either way: the scene's own values are drawn until the file is back.
		if ((uint64_t)look.Profile != 0)
		{
			AssetManager::GetAsset<RenderProfile>(look.Profile);
		}
		m_Scene->SetLook(look);

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
