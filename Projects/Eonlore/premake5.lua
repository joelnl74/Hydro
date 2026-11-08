-- The Eonlore workspace: Hydro (the engine, Oxygen, Runtime) with Eonlore's game code (Game/) and project (Project/).
-- Hydro is found at HYDRO_DIR, else next to this repository (..\Hydro), else two folders up (inside the engine repository).
local function FindHydro()
	local candidates = { _SCRIPT_DIR .. "/../Hydro", _SCRIPT_DIR .. "/../.." }
	if os.getenv("HYDRO_DIR") ~= nil then
		table.insert(candidates, 1, os.getenv("HYDRO_DIR"))
	end
	for _, directory in ipairs(candidates) do
		if os.isfile(path.getabsolute(path.join(directory, "Engine.lua"))) then
			return path.getabsolute(directory)
		end
	end
	error("Eonlore: Hydro not found. Set HYDRO_DIR or clone it next to this repository (..\\Hydro).")
end

include(FindHydro() .. "/Engine.lua")

HydroLoadWorkspace(_SCRIPT_DIR)

workspace "Hydro"
	HydroWorkspaceDefaults()

HydroIncludeEngine()
