#include "GameRegistration.h"

#include "Generated/GeneratedDataReflection.h"
#include "Generated/GeneratedScriptReflection.h"

#include "Hydro/Project/Project.h"

namespace Hydro
{
	void RegisterGame()
	{
		// The stable id is what projects store; the name can change.
		Project::SetCompiledGame({ "hydro-sample", "Hydro Sample" });

		RegisterGeneratedDataReflection();
		RegisterGeneratedScriptReflection();
	}
}
