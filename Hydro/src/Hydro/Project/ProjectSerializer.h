#pragma once
#include "Project.h"

namespace Hydro
{
	// Saves / loads a project as YAML (.hyproj). Keys missing from the file keep their defaults,
	// so older project files still load after new settings are added.
	//
	// Project:
	//   Name: Sandbox
	//   AssetDirectory: Assets
	//   StartScene: Scenes/Scene.hyscene
	// Renderer:
	//   ClearColor: [0, 0, 0, 1]
	//   VSync: true
	//   Wireframe: false
	//   ShaderHotReload: true
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
