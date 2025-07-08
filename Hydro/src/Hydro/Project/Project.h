#pragma once
#include <string>
#include <filesystem>

#include "Hydro/Core/Base.h"

namespace Hydro
{
	struct ProjectConfig
	{
		std::string Name = "Untitled";

		std::filesystem::path StartScene;
		std::filesystem::path AssetDirectory;
	};

	class Project
	{
	public:
		ProjectConfig& GetConfig() { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }

		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool Save(const std::filesystem::path& path);
	
	private:
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectDirectory;

		inline static Ref<Project> s_ActiveProject;
	};
}
