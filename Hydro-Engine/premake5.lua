project "Hydro-Engine"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "hypch.h"
	pchsource "src/hypch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

	includedirs
	{
		"src",
	}

	links
	{
	}

	filter "files:vendor/ImGuizmo/**.cpp"
	flags { "NoPCH" }

	filter "system:windows"
		systemversion "latest"

		defines
		{
		}

	filter "configurations:Debug"
		defines "HY_DEBUG"
		runtime "Debug"
		symbols "on"

		links
		{
		}

	filter "configurations:Release"
		defines "HY_RELEASE"
		runtime "Release"
		optimize "on"

		links
		{
		}

	filter "configurations:Distribution"
		defines "HY_DIST"
		runtime "Release"
		optimize "on"

		links
		{
		}
