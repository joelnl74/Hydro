// A stand-in for Project's write checks in the headless data tests (scripts/test_data.sh): the data writers ask it
// before and after saving. The real Project.cpp saves through ProjectSerializer, which pulls in the renderer.

#include "hypch.h"

#include "Hydro/Project/Project.h"

namespace Hydro
{
	bool Project::RefusesWrite([[maybe_unused]] const char* what, [[maybe_unused]] const std::filesystem::path& file)
	{
		return false;
	}

	void Project::OnGameContentSaved()
	{
	}
}
