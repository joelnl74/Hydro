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
		"src/**.cpp",

		-- GLSL sources, compiled to SPIR-V by Visual Studio as part of the build (see filter below).
		"assets/shaders/**.vert",
		"assets/shaders/**.frag",
		"assets/shaders/**.comp",
		"assets/shaders/**.geom",
		"assets/shaders/**.tesc",
		"assets/shaders/**.tese"
	}

	includedirs
	{
		"src",
		"%{wks.location}/Hydro/src",
		"%{wks.location}/Hydro/vendor",
		"%{wks.location}/Hydro/vendor/spdlog/include",
		"%{IncludeDir.glm}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.VMA}",
	}

	defines
	{
		"YAML_CPP_STATIC_DEFINE"
	}

	links
	{
		"Hydro"
	}

	-- Shader compilation: each shader is a custom build item, so it is only
	-- recompiled when its source changes. Output: <name>.<stage>.spv next to the source,
	-- e.g. assets/shaders/vertex.vert -> assets/shaders/vertex.vert.spv
	filter "files:assets/shaders/**.vert or assets/shaders/**.frag or assets/shaders/**.comp or assets/shaders/**.geom or assets/shaders/**.tesc or assets/shaders/**.tese"
		buildmessage "Compiling shader %{file.relpath}"
		buildcommands
		{
			'"$(VULKAN_SDK)\\Bin\\glslc.exe" "%{file.relpath}" -o "%{file.relpath}.spv"'
		}
		buildoutputs { "%{file.relpath}.spv" }

	-- Hydro compiles GLSL at runtime with shaderc: put its DLL next to the executable.
	filter {}
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
