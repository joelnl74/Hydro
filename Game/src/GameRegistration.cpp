#include "GameRegistration.h"

#include "Generated/GeneratedDataReflection.h"
#include "Generated/GeneratedScriptReflection.h"
#include "State/RpgSession.h"

namespace Hydro
{
	void RegisterGame()
	{
		// Data types first: scripts' data properties name them.
		RegisterGeneratedDataReflection();
		RegisterGeneratedScriptReflection();
		GameSession::Register<RpgSession>();
	}
}
