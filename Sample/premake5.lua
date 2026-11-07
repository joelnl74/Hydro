-- The Sample workspace: Hydro with the engine's own small test game (docs/Architecture.md, "Workspaces"). A game in
-- its own repository looks the same, with include(<its Hydro checkout> .. "/Engine.lua").
local hydro = os.getenv("HYDRO_DIR") or (_SCRIPT_DIR .. "/..")
include(hydro .. "/Engine.lua")

HydroLoadWorkspace(_SCRIPT_DIR)

workspace "Hydro"
	HydroWorkspaceDefaults()

HydroIncludeEngine()
