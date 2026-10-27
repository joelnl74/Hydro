include "./vendor/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "Hydro"
	architecture "x86_64"
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

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Compiler warnings (docs/Architecture.md, "Tooling"): Hydro and Oxygen build at /W4 (premake
-- "Extra"). Vendored code is excluded: headers come in through externalincludedirs and vendored
-- sources compile with warnings off (helpers in Dependencies.lua). Set FatalWarnings to true once a
-- Debug build has no warnings left; from then on any new warning fails the build (/WX). A variable rather than a command-line
-- option, because the "Premake" utility project regenerates the solution without extra options.
FatalWarnings = false

group "Dependencies"
	include "vendor/premake"
	include "Hydro/vendor/GLFW"
	include "Hydro/vendor/imgui"
	include "Hydro/vendor/yaml-cpp"
	include "Hydro/vendor/box2d.premake5.lua"
	include "Hydro/vendor/JoltPhysics.premake5.lua"
group ""

include "Hydro"
include "Oxygen"
