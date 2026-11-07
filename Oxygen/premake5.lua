project "Oxygen"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir (HydroWorkspaceDir .. "/bin/" .. outputdir .. "/%{prj.name}")
	objdir (HydroWorkspaceDir .. "/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",

		-- ImGuizmo (the Editor window's transform gizmo) is editor-only, so it is compiled here, not into Hydro.
		HydroRoot .. "/Hydro/vendor/ImGuizmo/ImGuizmo.h",
		HydroRoot .. "/Hydro/vendor/ImGuizmo/ImGuizmo.cpp"
	}

	includedirs
	{
		"src",
		HydroRoot .. "/Hydro/src",
		HydroGameSourceDir
	}

	-- Vendored headers: no warnings from these (see ExternalIncludeDirs in Dependencies.lua).
	ExternalIncludeDirs
	{
		HydroRoot .. "/Hydro/vendor",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}",
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
	HydroWorkspaceDefines()

	links
	{
		HydroWorkspace.Game.Target,
		"Hydro"
	}

	-- Vendored sources compiled into Oxygen: no warnings from them.
	filter "files:**/vendor/ImGuizmo/**.cpp"
		warnings "Off"

	-- Hydro compiles GLSL at runtime with shaderc: put its DLL next to the executable.
	filter {}
	postbuildcommands
	{
		'copy /B /Y "$(VULKAN_SDK)\\Bin\\shaderc_shared.dll" "$(OutDir)" > nul',
		-- The engine's and the editor's own files next to the exe (EngineResources): the exe works from any folder.
		'xcopy /E /I /Y /Q "' .. HydroAssetsDir .. '" "$(OutDir)assets" > nul',
		'xcopy /E /I /Y /Q "' .. OxygenAssetsDir .. '" "$(OutDir)assets" > nul'
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "HY_DEBUG"
		HydroSourceDirDefines()
		runtime "Debug"
		symbols "on"

		postbuildcommands 
		{
		}

	filter "configurations:Release"
		defines "HY_RELEASE"
		HydroSourceDirDefines()
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
