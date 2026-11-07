-- Hydro's part of a workspace. A workspace (the engine repository itself, Sample/, or a game's own repository)
-- includes this file, loads its hydro-workspace.json and gets the engine projects plus a library for its game:
--
--   include(<Hydro checkout> .. "/Engine.lua")
--   HydroLoadWorkspace(_SCRIPT_DIR)
--   workspace "Hydro"
--       HydroWorkspaceDefaults()
--   HydroIncludeEngine()
--
-- Source paths come from HydroRoot (the engine checkout); outputs (bin/, bin-int/, the solution) from the workspace.
-- Its solution is <workspace>/build/Hydro.sln (the engine repository's own: Hydro.sln next to this file).

include "Dependencies.lua"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Compiler warnings (docs/Architecture.md, "Tooling"): Hydro and Oxygen build at /W4 (premake "Extra"). Set
-- FatalWarnings to true once a Debug build has no warnings left. A variable rather than a command-line option,
-- because the "Premake" utility project regenerates the solution without extra options.
FatalWarnings = false

-- Reads <directory>/hydro-workspace.json, the workspace's one description (docs/Architecture.md, "Workspaces"):
--   { "Game": { "Id", "Name", "Target", "CodeDirectory" }, "DefaultProject" }
function HydroLoadWorkspace(directory)
	local file = path.join(directory, "hydro-workspace.json")
	local text = io.readfile(file)
	if text == nil then
		error("Hydro: no hydro-workspace.json in " .. directory)
	end
	local workspaceInfo, problem = json.decode(text)
	if workspaceInfo == nil or workspaceInfo.Game == nil then
		error("Hydro: " .. file .. " is not a workspace description: " .. tostring(problem))
	end
	for _, key in ipairs({ "Id", "Name", "Target", "CodeDirectory" }) do
		if type(workspaceInfo.Game[key]) ~= "string" or workspaceInfo.Game[key] == "" then
			error("Hydro: " .. file .. " needs Game." .. key)
		end
	end

	HydroWorkspace = workspaceInfo
	HydroWorkspaceDir = path.getabsolute(directory)
	HydroGameDir = path.join(HydroWorkspaceDir, workspaceInfo.Game.CodeDirectory)
	HydroGameSourceDir = path.join(HydroGameDir, "src")
	HydroWarnAboutEngineVersion()
	return workspaceInfo
end

-- engine.lock (one line: the Hydro commit the workspace was last built against). Locally a different checkout is only
-- a warning, since engine and game often change together; CI checks it out exactly.
function HydroWarnAboutEngineVersion()
	local lock = io.readfile(path.join(HydroWorkspaceDir, "engine.lock"))
	if lock == nil then
		return
	end
	local required = lock:gsub("%s+", "")
	local current = os.outputof('git -C "' .. HydroRoot .. '" rev-parse HEAD')
	if current ~= nil and required ~= "" and current:sub(1, #required) ~= required and required:sub(1, #current) ~= current then
		print("WARNING: this workspace is built against a different Hydro revision.")
		print("  Required Hydro: " .. required)
		print("  Current Hydro:  " .. current)
	end
end

-- Inside `workspace "..."`: the settings every Hydro workspace shares.
function HydroWorkspaceDefaults()
	architecture "x86_64"
	startproject "Oxygen"
	configurations { "Debug", "Release", "Dist" }
	flags { "MultiProcessorCompile" }

	-- Another workspace keeps the solution and the engine's project files in its own build/ folder, so they never
	-- overwrite the engine repository's (next to the engine's sources).
	if HydroWorkspaceDir ~= HydroRoot then
		location(path.join(HydroWorkspaceDir, "build"))
	end
end

-- The engine's projects and the workspace's game library.
function HydroIncludeEngine()
	group "Dependencies"
		include(HydroRoot .. "/vendor/premake")
		include(HydroRoot .. "/Hydro/vendor/GLFW")
		include(HydroRoot .. "/Hydro/vendor/imgui")
		include(HydroRoot .. "/Hydro/vendor/yaml-cpp")
		include(HydroRoot .. "/Hydro/vendor/box2d.premake5.lua")
		include(HydroRoot .. "/Hydro/vendor/JoltPhysics.premake5.lua")
	group ""

	include(HydroRoot .. "/Hydro")
	HydroGameLibrary()
	include(HydroRoot .. "/Oxygen")
	include(HydroRoot .. "/Runtime")
end

-- The game's code: its scripts (HY_SCRIPT), data types (HY_DATA, HY_ENUM) and their generated reflection, linked by
-- the editor (Oxygen) and the player (Runtime). <CodeDirectory>/src must have GameRegistration.h (RegisterGame()).
function HydroGameLibrary()
	local generated = path.join(HydroGameSourceDir, "Generated")

	project(HydroWorkspace.Game.Target)
		location(HydroGameDir)
		kind "StaticLib"
		language "C++"
		cppdialect "C++17"
		staticruntime "off"

		targetdir (HydroWorkspaceDir .. "/bin/" .. outputdir .. "/%{prj.name}")
		objdir (HydroWorkspaceDir .. "/bin-int/" .. outputdir .. "/%{prj.name}")

		files
		{
			HydroGameSourceDir .. "/**.h",
			HydroGameSourceDir .. "/**.cpp"
		}

		-- Writes Generated/GeneratedScriptReflection.cpp and GeneratedDataReflection.cpp from the annotations before
		-- every build (each only touched when it changes).
		prebuildmessage "Generating Hydro script and data reflection"
		prebuildcommands
		{
			'py "' .. HydroRoot .. '/Tools/ReflectionGenerator/generate_reflection.py" "' .. HydroRoot .. '"'
				.. ' --source "' .. HydroRoot .. '/Hydro/src" --source "' .. HydroGameSourceDir .. '"'
				.. ' --output "' .. generated .. '/GeneratedScriptReflection.cpp"'
				.. ' --data-output "' .. generated .. '/GeneratedDataReflection.cpp"'
		}

		includedirs
		{
			HydroGameSourceDir,
			HydroRoot .. "/Hydro/src"
		}

		-- Vendored headers: no warnings from these (see ExternalIncludeDirs in Dependencies.lua).
		ExternalIncludeDirs
		{
			HydroRoot .. "/Hydro/vendor",
			"%{IncludeDir.spdlog}",
			"%{IncludeDir.ImGui}",
			"%{IncludeDir.glm}",
			"%{IncludeDir.VulkanSDK}",
			"%{IncludeDir.entt}",
			"%{IncludeDir.yaml_cpp}",
			"%{IncludeDir.VMA}"
		}

		warnings "Extra"
		FatalWarningsIfEnabled()

		defines { "YAML_CPP_STATIC_DEFINE" }

		-- Built after Hydro (the reflection step and the headers need it). Not linked here: a static library that
		-- links Hydro would carry a copy of it; the applications link the game and Hydro.
		dependson { "Hydro" }

		filter "system:windows"
			systemversion "latest"
			-- The generated reflection file has more sections than the default object format allows.
			buildoptions { "/bigobj" }

		filter "configurations:Debug"
			defines "HY_DEBUG"
			runtime "Debug"
			symbols "on"

		filter "configurations:Release"
			defines "HY_RELEASE"
			runtime "Release"
			optimize "on"

		filter "configurations:Dist"
			defines "HY_DIST"
			runtime "Release"
			optimize "on"

		filter {}
end
