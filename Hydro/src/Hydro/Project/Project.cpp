#include "hypch.h"
#include "Project.h"
#include "ProjectSerializer.h"

namespace Hydro
{
	std::filesystem::path Project::GetStartScenePath() const
	{
		if (m_Config.StartScene.empty())
			return {};

		return GetAssetDirectory() / m_Config.StartScene;
	}

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
			return nullptr;

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

		ProjectSerializer serializer(s_ActiveProject);
		return serializer.Serialize(s_ActiveProject->m_ProjectFilePath);
	}

	void Project::Close()
	{
		s_ActiveProject = nullptr;
	}
}
