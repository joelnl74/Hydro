project "Runtime"
	-- The standalone player: plays a project without the editor (docs: "Standalone runtime"). Packaged by
	-- scripts/Package.bat as <Project>.exe. Release keeps a console with the log; Dist has none.
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
		"src",
		"%{wks.location}/Hydro/src",
		"%{wks.location}/Game/src"
	}

	-- Vendored headers: no warnings from these (see ExternalIncludeDirs in Dependencies.lua).
	ExternalIncludeDirs
	{
		"%{wks.location}/Hydro/vendor",
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

	defines
	{
		"YAML_CPP_STATIC_DEFINE"
	}

	links
	{
		"Game",
		"Hydro"
	}

	-- Hydro compiles GLSL at runtime with shaderc: put its DLL next to the executable.
	postbuildcommands
	{
		'copy /B /Y "$(VULKAN_SDK)\\Bin\\shaderc_shared.dll" "$(OutDir)" > nul'
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

	-- The game players get: no console window. main() stays the entry point.
	filter "configurations:Dist"
		kind "WindowedApp"
		defines "HY_DIST"
		runtime "Release"
		optimize "on"
		linkoptions { "/ENTRY:mainCRTStartup" }
