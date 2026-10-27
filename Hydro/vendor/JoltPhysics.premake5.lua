-- Jolt Physics (https://github.com/jrouwe/JoltPhysics), a git submodule at Hydro/vendor/JoltPhysics
-- pinned to v5.6.0.
--
-- The script lives next to the submodule (not inside it), so the submodule stays clean. Paths are
-- relative to this file. Included from the root premake5.lua.
--
-- Facts taken from the pinned tag's Jolt/Jolt.cmake and Build/CMakeLists.txt (re-check them when the
-- tag changes):
--   * C++17. Sources are Jolt/**.cpp. The DX12, Vulkan, Metal and CPU compute backends are left out
--     (Jolt only needs them for GPU hair); the files are guarded by JPH_USE_* defines, so they would
--     compile to nothing, but they are excluded to keep the project small.
--   * The compile options below must be IDENTICAL in every project that includes Jolt headers
--     (see JoltDefines in Dependencies.lua): Jolt changes class layouts with them, and
--     JPH::RegisterTypes() reports a mismatch at startup.
--   * Left off on purpose: the debug renderer, the profiler, the object stream (serialisation),
--     floating point exceptions, AVX and friends. Hydro draws its own debug lines, and no AVX keeps
--     results the same across machines (the same choice as for Box2D).
--   * MSVC needs /Zc:__cplusplus (Jolt checks __cplusplus).
project "JoltPhysics"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"JoltPhysics/Jolt/**.h",
		"JoltPhysics/Jolt/**.inl",
		"JoltPhysics/Jolt/**.cpp"
	}

	removefiles
	{
		"JoltPhysics/Jolt/Compute/CPU/**",
		"JoltPhysics/Jolt/Compute/DX12/**",
		"JoltPhysics/Jolt/Compute/MTL/**",
		"JoltPhysics/Jolt/Compute/VK/**",
		"JoltPhysics/Jolt/Shaders/**"
	}

	includedirs
	{
		"JoltPhysics"
	}

	JoltDefines()

	-- Vendored code: no warnings.
	warnings "Off"

	filter "action:vs*"
		buildoptions { "/Zc:__cplusplus" }

	filter "system:windows"
		systemversion "latest"

	-- Debug is optimised on purpose, like Box2D: Jolt at -Od is many times slower (it is heavily
	-- templated), which makes every Play in a Debug build crawl, and nobody steps through the solver.
	-- NoRuntimeChecks is required with /O2 (MSVC rejects /O2 together with /RTC1). Remove these lines
	-- to debug Jolt.
	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		optimize "Speed"
		flags { "NoRuntimeChecks" }

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		optimize "on"
		symbols "off"
