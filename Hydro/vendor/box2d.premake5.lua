-- Box2D (https://github.com/erincatto/box2d), a git submodule at Hydro/vendor/box2d pinned to v3.1.1.
--
-- The script lives next to the submodule (not inside it), so the submodule stays clean. Paths are
-- relative to this file. Included from the root premake5.lua.
--
-- Facts taken from the pinned tag's src/CMakeLists.txt (re-check them when the tag changes):
--   * C17 (_Static_assert, anonymous unions). The bundled premake (5.0.0-beta1) has no cdialect "C17",
--     so the standard is passed as a compiler option.
--   * No defines or special flags are needed on MSVC (atomics use intrinsics). BOX2D_AVX2 is left off:
--     the default SSE2 code path (NEON on Apple silicon) keeps results the same across machines.
--   * gcc/clang need -ffp-contract=off (same reason).
project "box2d"
	kind "StaticLib"
	language "C"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"box2d/src/**.h",
		"box2d/src/**.c",

		"box2d/include/**.h"
	}

	includedirs
	{
		"box2d/include",
		"box2d/src"
	}

	filter "action:vs*"
		buildoptions { "/std:c17" }

	filter "action:not vs*"
		buildoptions { "-std=gnu17", "-ffp-contract=off" }

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		pic "On"

	-- Debug is optimised on purpose: Box2D at -Od is several times slower, which makes every Play
	-- in a Debug build crawl, and nobody steps through the solver. NoRuntimeChecks is required with
	-- /O2 (MSVC rejects /O2 together with /RTC1). Optimised builds define NDEBUG, which would switch
	-- Box2D's asserts off, so they are turned back on explicitly. Remove these lines to debug Box2D.
	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
		optimize "Speed"
		flags { "NoRuntimeChecks" }
		defines { "B2_ENABLE_ASSERT" }

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		optimize "on"
		symbols "off"
