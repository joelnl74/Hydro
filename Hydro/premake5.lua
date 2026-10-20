project "Hydro"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	-- Visual Studio: runs the "Premake" utility project first, so new/removed source files are
	-- picked up automatically on build (no need to run Win-GenProjects.bat). Makefiles and Xcode
	-- projects are regenerated with scripts/Mac-GenProjects.sh instead.
	if os.target() == "windows" then
		dependson { "Premake" }
	end

	pchheader "hypch.h"
	pchsource "src/hypch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",
		"vendor/VulkanMemoryAllocator/**.h",
		"vendor/VulkanMemoryAllocator/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",
		"vendor/entt/**.hpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE"
	}

	includedirs
	{
		"src"
	}

	-- Vendored headers: no warnings from these (see ExternalIncludeDirs in Dependencies.lua).
	ExternalIncludeDirs
	{
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.tinyobjloader}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.VMA}"
	}

	warnings "Extra"
	FatalWarningsIfEnabled()

	links
	{
		"GLFW",
		"ImGui",
		"yaml-cpp"
	}

	-- Vendored sources compiled into Hydro, including the ImGui backend copies in Platform/.
	filter { "files:vendor/** or src/Hydro/Platform/**/imgui_impl_*.cpp" }
		warnings "Off"

	-- One folder per OS under Platform/ (Windows, MacOS, Linux); only the target's own is built.
	-- Platform/GLFW (window, input) and Platform/Vulkan are shared by all of them.
	filter "system:not windows"
		removefiles { "src/Hydro/Platform/Windows/**" }

	filter "system:not macosx"
		removefiles { "src/Hydro/Platform/MacOS/**" }

	filter "system:not linux"
		removefiles { "src/Hydro/Platform/Linux/**" }

	filter "system:windows"
		systemversion "latest"

		-- shaderc is used through its C API, so the release DLL is safe with the debug CRT, and only
		-- shaderc_shared.dll from the installed SDK is needed at runtime (Oxygen copies it).
		links
		{
			"%{Library.Vulkan}",
			"%{Library.ShaderC_Release}"
		}

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
