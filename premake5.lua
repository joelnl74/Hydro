-- Hydro workspace. Generate the projects with the script for your platform (scripts/README.md):
--   Windows  scripts/Win-GenProjects.bat     -> Visual Studio 2022 solution (Hydro.sln)
--   macOS    scripts/Mac-GenProjects.sh      -> Makefiles (default) or an Xcode project (--xcode)
--   Linux    scripts/Linux-GenProjects.sh    -> Makefiles (build check only, not a supported target yet)
include "./vendor/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "Hydro"
	startproject "Oxygen"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".clang-format",
		".editorconfig",
		".gitattributes"
	}

	flags
	{
		"MultiProcessorCompile"
	}

	filter "system:windows"
		architecture "x86_64"

	-- Apple Silicon (M1 and later). For an Intel Mac, use "x86_64" here.
	filter "system:macosx"
		architecture "ARM64"
		toolset "clang"
		systemversion "12.0"

	filter "system:linux"
		architecture "x86_64"

	filter {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Compiler warnings (docs/Architecture.md, "Tooling"): Hydro and Oxygen build at /W4 (premake
-- "Extra"). Vendored code is excluded: headers come in through externalincludedirs and vendored
-- sources compile with warnings off (helpers in Dependencies.lua). Set FatalWarnings to true once a
-- Debug build has no warnings left; from then on any new warning fails the build (/WX). A variable rather than a command-line
-- option, because the "Premake" utility project regenerates the solution without extra options.
FatalWarnings = false

group "Dependencies"
	-- Visual Studio only: a utility project that reruns premake when a premake5.lua changes.
	if os.target() == "windows" then
		include "vendor/premake"
	end
	-- GLFW and ImGui are submodules; their projects are described here, not in the submodules.
	include "Hydro/vendor/GLFW.lua"
	include "Hydro/vendor/ImGui.lua"
	include "Hydro/vendor/yaml-cpp"
group ""

include "Hydro"
include "Oxygen"
