-- The engine repository's own workspace: the engine, Oxygen and Runtime with the game named in hydro-workspace.json.
-- A game in its own repository has the same three lines with its own hydro-workspace.json (Sample/ is an example).
include "./vendor/premake/premake_customization/solution_items.lua"
include "Engine.lua"

HydroLoadWorkspace(_SCRIPT_DIR)

workspace "Hydro"
	HydroWorkspaceDefaults()

	solution_items
	{
		".clang-format",
		".editorconfig",
		".gitattributes"
	}

HydroIncludeEngine()
