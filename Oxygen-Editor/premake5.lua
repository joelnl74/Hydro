project "Oxygen-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	includedirs
	{
	}

	links
	{
		"Hydro-Engine"
	}

	filter "system:windows"
		systemversion "latest"

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
