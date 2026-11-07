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

		// Only the project's game, without opening it (empty Id: generic). False when the file can't be read.
		static bool ReadGame(const std::filesystem::path& filepath, GameInfo& game);

	private:
		Ref<Project> m_Project;
	};
}
