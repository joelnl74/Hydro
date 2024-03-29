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
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
	include "vendor/premake"
	include "Hydro/vendor/GLFW"
	include "Hydro/vendor/imgui"
group ""

include "Hydro"
include "Oxygen"
