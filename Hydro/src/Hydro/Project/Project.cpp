#include "hypch.h"
#include "Project.h"
#include "ProjectSerializer.h"

namespace Hydro
{
	Ref<Project> Project::New(const std::filesystem::path& projectFile)
	{
		Ref<Project> project = CreateRef<Project>();
		project->m_ProjectFilePath = projectFile;
		project->m_ProjectDirectory = projectFile.parent_path();
		project->m_Config.Name = projectFile.stem().string();

		s_ActiveProject = project;
		return s_ActiveProject;
	}

	Ref<Project> Project::Load(const std::filesystem::path& projectFile)
	{
		Ref<Project> project = CreateRef<Project>();
		project->m_ProjectFilePath = projectFile;
		project->m_ProjectDirectory = projectFile.parent_path();

		ProjectSerializer serializer(project);
		if (serializer.Deserialize(projectFile) == false)
		{
			return nullptr;
		}

		s_ActiveProject = project;
		return s_ActiveProject;
	}

	bool Project::SaveActive()
	{
		if (s_ActiveProject == nullptr)
		{
			HY_CORE_WARN("Project::SaveActive: no project is open");
			return false;
		}

		if (RefusesWrite("the project file", s_ActiveProject->m_ProjectFilePath))
		{
			return false;
		}

		ProjectSerializer serializer(s_ActiveProject);
		return serializer.Serialize(s_ActiveProject->m_ProjectFilePath);
	}

	bool Project::RefusesWrite(const char* what, const std::filesystem::path& file)
	{
		if (s_ActiveProject == nullptr || s_ActiveProject->m_ReadOnly == false)
		{
			return false;
		}

		HY_CORE_WARN("Not saving {} {}: the project is open read-only (it belongs to {}; open it with that game's editor)", what,
			file.filename().string(), s_ActiveProject->m_Config.Game.Name);
		return true;
	}

	void Project::OnGameContentSaved()
	{
		if (s_ActiveProject == nullptr || s_ActiveProject->m_Config.Game.IsGeneric() == false || s_CompiledGame.IsGeneric())
		{
			return;
		}

		s_ActiveProject->m_Config.Game = s_CompiledGame;
		HY_CORE_INFO("Project '{}' now uses {}'s code, so it belongs to that game (saved in its project file)",
			s_ActiveProject->m_Config.Name, s_CompiledGame.Name);
		SaveActive();
	}

	void Project::Close()
	{
		s_ActiveProject = nullptr;
	}
}
