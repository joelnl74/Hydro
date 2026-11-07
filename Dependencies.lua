
-- Hydro Dependencies
VULKAN_SDK = os.getenv("VULKAN_SDK")

-- The engine checkout (this file's folder), and the resource folders each exe gets a copy of (EngineResources).
HydroRoot = path.getabsolute(_SCRIPT_DIR)
HydroAssetsDir = path.translate(HydroRoot .. "/Hydro/assets", "\\")
OxygenAssetsDir = path.translate(HydroRoot .. "/Oxygen/assets", "\\")

-- Oxygen's development override (Debug and Release only): it reads shaders and fonts from the source folders
-- when they exist, so shader hot reload edits the real files. Packaged and Dist builds use their own copy.
function HydroSourceDirDefines()
	defines
	{
		'HY_ENGINE_DIR="' .. HydroRoot .. '/Hydro"',
		'HY_EDITOR_DIR="' .. HydroRoot .. '/Oxygen"'
	}
end

-- Where Oxygen's File > Build finds the engine's scripts and the workspace it packages (every configuration).
function HydroWorkspaceDefines()
	defines
	{
		'HY_HYDRO_ROOT="' .. HydroRoot .. '"',
		'HY_WORKSPACE_DIR="' .. HydroWorkspaceDir .. '"'
	}
end

IncludeDir = {}
IncludeDir["GLFW"] = HydroRoot .. "/Hydro/vendor/GLFW/include"
IncludeDir["ImGui"] = HydroRoot .. "/Hydro/vendor/ImGui"
IncludeDir["ImGuizmo"] = HydroRoot .. "/Hydro/vendor/ImGuizmo" -- Oxygen only (transform gizmo)
IncludeDir["glm"] = HydroRoot .. "/Hydro/vendor/glm"
IncludeDir["yaml_cpp"] = HydroRoot .. "/Hydro/vendor/yaml-cpp/include"
IncludeDir["box2d"] = HydroRoot .. "/Hydro/vendor/box2d/include"
IncludeDir["JoltPhysics"] = HydroRoot .. "/Hydro/vendor/JoltPhysics" -- Jolt headers are included as <Jolt/Jolt.h>
IncludeDir["VMA"] = HydroRoot .. "/Hydro/vendor/VulkanMemoryAllocator"
IncludeDir["entt"] = HydroRoot .. "/Hydro/vendor/entt"
IncludeDir["spdlog"] = HydroRoot .. "/Hydro/vendor/spdlog/include"
IncludeDir["stb"] = HydroRoot .. "/Hydro/vendor/stb/include"
IncludeDir["cgltf"] = HydroRoot .. "/Hydro/vendor/cgltf"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

-- Vendored include directories: searched like includedirs, but the compiler reports no warnings
-- from headers inside them. Premake 5.0.0-beta2 and newer have externalincludedirs/externalwarnings;
-- the bundled premake (5.0.0-beta1) doesn't, so there the same is done with MSVC's own options:
-- /external:I marks a directory as external and /external:W0 silences warnings from it.
-- Expands %{IncludeDir.X}, %{VULKAN_SDK} and %{wks.location} now, into an absolute Windows path, so
-- /external:I names the same directory the compiler finds through the include path.
function ResolveIncludeDir(dir)
	local resolved = dir
	for _ = 1, 4 do
		resolved = resolved:gsub("%%{IncludeDir%.([%w_]+)}", function(name) return IncludeDir[name] end)
		resolved = resolved:gsub("%%{VULKAN_SDK}", function() return VULKAN_SDK or "" end)
		resolved = resolved:gsub("%%{wks%.location}", function() return _MAIN_SCRIPT_DIR end)
	end
	return path.translate(path.normalize(resolved), "\\")
end

function ExternalIncludeDirs(dirs)
	if externalincludedirs ~= nil then
		externalincludedirs(dirs)
		externalwarnings "Off"
	else
		includedirs(dirs)
		local options = { "/external:W0" }
		for _, dir in ipairs(dirs) do
			table.insert(options, '/external:I"' .. ResolveIncludeDir(dir) .. '"')
		end
		buildoptions(options)
	end
end

-- Jolt's compile options. They change class layouts, so EVERY project that includes Jolt headers
-- (the JoltPhysics library and Hydro) must use exactly these; JPH::RegisterTypes() reports a mismatch.
-- Mirrors Jolt's own CMake defaults except: no debug renderer, no profiler, no object stream, no
-- floating point exceptions and no AVX (see Hydro/vendor/JoltPhysics.premake5.lua).
function JoltDefines()
	filter "configurations:Debug"
		defines { "JPH_ENABLE_ASSERTS" }

	-- Jolt's own switch for its extra debug code (its CMake uses NDEBUG, which would also change
	-- assert() in all of Hydro).
	filter "configurations:Release or configurations:Dist"
		defines { "JPH_NO_DEBUG" }

	filter {}
end

-- Warnings as errors (/WX) when FatalWarnings is set in the root premake5.lua.
function FatalWarningsIfEnabled()
	if FatalWarnings then
		flags { "FatalCompileWarnings" }
	end
end

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["VulkanUtils"] = "%{LibraryDir.VulkanSDK}/VkLayer_utils.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.VulkanSDK}/SPIRV-Toolsd.lib"

Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"
