#include "SceneSettingsCommand.h"

#include <Hydro/Scene/SceneSerializer.h>
#include <Hydro/Utils/YAMLConversions.h>

namespace Hydro
{
	SceneSettingsCommand::SceneSettingsCommand(const Ref<Scene>& scene, std::string before, std::string after)
		: m_Scene(scene), m_Before(std::move(before)), m_After(std::move(after))
	{
	}

	std::string SceneSettingsCommand::Capture(const Scene& scene)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		SceneSerializer::SerializeSettings(out, scene);
		out << YAML::EndMap;
		return out.c_str();
	}

	void SceneSettingsCommand::Apply(const std::string& settings) const
	{
		SceneSerializer::DeserializeSettings(YAML::Load(settings), *m_Scene);
	}
}
