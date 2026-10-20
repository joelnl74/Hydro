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

		-- GLSL sources, compiled to SPIR-V with glslc as part of the build (see the filters below).
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
		"%{wks.location}/Hydro/src"
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
		"Hydro"
	}

	-- macOS / Linux: the Vulkan loader, shaderc and the system libraries Hydro needs (Dependencies.lua).
	LinkHydroDependencies()

	-- Run from Oxygen/, where assets/ is (Visual Studio and Xcode; scripts/Run.sh does the same).
	debugdir "%{prj.location}"

	-- Shader compilation: each shader is a custom build item, so it is only recompiled when its source
	-- changes, and a syntax error fails the build. Output: <name>.<stage>.spv next to the source, e.g.
	-- assets/shaders/vertex.vert -> assets/shaders/vertex.vert.spv (not used at runtime yet: Hydro
	-- compiles the GLSL itself with shaderc).
	filter "files:assets/shaders/**.vert or assets/shaders/**.frag or assets/shaders/**.comp or assets/shaders/**.geom or assets/shaders/**.tesc or assets/shaders/**.tese"
		buildmessage "Compiling shader %{file.relpath}"
		buildoutputs { "%{file.relpath}.spv" }

	filter { "system:windows", "files:assets/shaders/**.vert or assets/shaders/**.frag or assets/shaders/**.comp or assets/shaders/**.geom or assets/shaders/**.tesc or assets/shaders/**.tese" }
		buildcommands { '"$(VULKAN_SDK)\\Bin\\glslc.exe" "%{file.relpath}" -o "%{file.relpath}.spv"' }

	filter { "system:macosx or linux", "files:assets/shaders/**.vert or assets/shaders/**.frag or assets/shaders/**.comp or assets/shaders/**.geom or assets/shaders/**.tesc or assets/shaders/**.tese" }
		buildcommands { '"' .. VULKAN_SDK_BIN .. '/glslc" "%{file.relpath}" -o "%{file.relpath}.spv"' }

	-- Windows: Hydro compiles GLSL at runtime with shaderc, so its DLL goes next to the executable.
	-- (macOS and Linux link shaderc statically or find it through the rpath.)
	filter "system:windows"
		systemversion "latest"
		postbuildcommands
		{
			'copy /B /Y "$(VULKAN_SDK)\\Bin\\shaderc_shared.dll" "$(OutDir)" > nul'
		}

	-- Linux links position-independent executables by default; the static libraries are -fPIC too.
	filter "system:linux"
		pic "On"

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
