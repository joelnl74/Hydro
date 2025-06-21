project "Oxygen"
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
		"%{prj.name}/src",
		"%{wks.location}/Hydro/src",
		"%{wks.location}/Hydro/vendor",
		"%{wks.location}/Hydro/vendor/spdlog/include",
		"%{IncludeDir.glm}",
		"%{IncludeDir.Vulkan}",
		"%{IncludeDir.entt}",
	}

	links
	{
		"Hydro"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "HY_DEBUG"
		runtime "Debug"
		symbols "on"

		postbuildcommands 
		{
		}

	filter "configurations:Release"
		defines "HY_RELEASE"
		runtime "Release"
		optimize "on"

		postbuildcommands 
		{
		}

	filter "configurations:Dist"
		defines "HY_DIST"
		runtime "Release"
		optimize "on"

		postbuildcommands 
		{
		}
