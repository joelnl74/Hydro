// Game/src/GameRegistration.h
#pragma once

namespace Hydro
{
	// Everything the game registers with the engine, once at startup, before a project or scene is opened (the
	// editor's and the standalone player's OnInit): its data types and enums (GeneratedDataReflection), its scripts
	// (GeneratedScriptReflection) and its game session type (RpgSession). Safe to call more than once.
	void RegisterGame();
}
