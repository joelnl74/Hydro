#pragma once
#include "Project.h"

namespace Hydro
{
	// Saves and loads a project as YAML (.hyproj): a Project map and a Renderer map. Missing keys keep their defaults,
	// so older project files still load.
	class ProjectSerializer
	{
	public:
		ProjectSerializer(const Ref<Project>& project);

		bool Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);

	private:
		Ref<Project> m_Project;
	};
}
