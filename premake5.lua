include "./vendor/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "Hydro"
	architecture "x86_64"
	startproject "Oxygen-Editor"

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
group ""

include "Hydro-Engine"
include "Oxygen-Editor"
