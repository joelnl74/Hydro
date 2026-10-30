project "Game"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	-- The game's own code: its scripts (HY_SCRIPT classes) and their generated reflection. Linked by
	-- every application that runs the game: the editor (Oxygen) and the standalone player (Runtime).
	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	-- Writes src/Generated/GeneratedScriptReflection.cpp from the HY_SCRIPT / HY_PROPERTY annotations
	-- before every build (only touched when it changes).
	prebuildmessage "Generating Hydro native-script reflection"
	prebuildcommands
	{
		'py "%{wks.location}/Tools/ReflectionGenerator/generate_reflection.py" "%{wks.location}/."'
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

	-- Built after Hydro (the reflection step and the headers need it). Not linked here: a static library
	-- that links Hydro would carry a copy of it; the applications link Game and Hydro.
	dependson { "Hydro" }

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
