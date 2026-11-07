#include "GameRegistration.h"

#include "Generated/GeneratedDataReflection.h"
#include "Generated/GeneratedScriptReflection.h"
#include "State/RpgSession.h"

#include "Hydro/Project/Project.h"

namespace Hydro
{
	void RegisterGame()
	{
		// The stable id is what projects store; the name can change.
		Project::SetCompiledGame({ "eonlore", "Eonlore" });

		// Data types first: scripts' data properties name them.
		RegisterGeneratedDataReflection();
		RegisterGeneratedScriptReflection();
		GameSession::Register<RpgSession>();
	}
}
