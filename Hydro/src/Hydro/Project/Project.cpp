#pragma once
#include "hypch.h"
#include "Project.h"

namespace Hydro
{
	Ref<Project> Hydro::Project::New()
	{
		s_ActiveProject = CreateRef<Project>();
		return s_ActiveProject;
	}
	
	Ref<Project> Project::Load(const std::filesystem::path& path)
	{
		Ref<Project> project = CreateRef<Project>();
		s_ActiveProject = project;
		
		return s_ActiveProject;
	}
	
	bool Project::Save(const std::filesystem::path& path)
	{
		return false;
	}
}
