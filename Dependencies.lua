
-- Hydro dependencies: include directories and libraries for every platform.
--
-- The Vulkan SDK comes from the VULKAN_SDK environment variable on every platform:
--   Windows  C:\VulkanSDK\<version>          (set by the installer; Include, Lib, Bin)
--   macOS    ~/VulkanSDK/<version>/macOS     (set by `source ~/VulkanSDK/<version>/setup-env.sh`; include, lib, bin)
--   Linux    <sdk>/x86_64 or /usr            (include, lib, bin)
-- See docs/Architecture.md, "Platforms", and the Getting started page.
VULKAN_SDK = os.getenv("VULKAN_SDK")
if VULKAN_SDK == nil or VULKAN_SDK == "" then
	if os.target() == "linux" and os.isfile("/usr/include/vulkan/vulkan.h") then
		VULKAN_SDK = "/usr"
	else
		error("VULKAN_SDK is not set. Install the Vulkan SDK (macOS: run `source ~/VulkanSDK/<version>/setup-env.sh` first).")
	end
end

-- The Windows SDK capitalises its folders; the macOS and Linux SDKs don't.
local sdkInclude = os.target() == "windows" and "Include" or "include"
local sdkLib = os.target() == "windows" and "Lib" or "lib"
local sdkBin = os.target() == "windows" and "Bin" or "bin"
VULKAN_SDK_BIN = VULKAN_SDK .. "/" .. sdkBin

IncludeDir = {}
-- Folder names match the submodules exactly (lower case): macOS and Linux file systems can be case-sensitive.
IncludeDir["GLFW"] = "%{wks.location}/Hydro/vendor/glfw/include"
IncludeDir["ImGui"] = "%{wks.location}/Hydro/vendor/imgui"
IncludeDir["glm"] = "%{wks.location}/Hydro/vendor/glm"
IncludeDir["yaml_cpp"] = "%{wks.location}/Hydro/vendor/yaml-cpp/include"
IncludeDir["VMA"] = "%{wks.location}/Hydro/vendor/VulkanMemoryAllocator"
IncludeDir["entt"] = "%{wks.location}/Hydro/vendor/entt"
IncludeDir["spdlog"] = "%{wks.location}/Hydro/vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/Hydro/vendor/stb/include"
IncludeDir["tinyobjloader"] = "%{wks.location}/Hydro/vendor/Obj-loader"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/" .. sdkInclude

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

-- Warnings as errors (/WX) when FatalWarnings is set in the root premake5.lua.
function FatalWarningsIfEnabled()
	if FatalWarnings then
		flags { "FatalCompileWarnings" }
	end
end

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/" .. sdkLib

-- Windows: import libraries from the SDK. Hydro links them (MSVC puts them into Hydro.lib).
Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"

-- macOS and Linux: a static library doesn't carry its link dependencies (and premake's Makefiles
-- don't pass Hydro's GLFW / ImGui / yaml-cpp on, unlike Visual Studio), so every application that
-- links Hydro (Oxygen, later the Runtime) calls this in its project, after links { "Hydro" }.
--   GLFW, ImGui, yaml-cpp  Hydro's vendored libraries, after Hydro so the linker resolves them
--   Vulkan  the loader (libvulkan); on macOS it finds the Metal driver (MoltenVK or KosmicKrisp)
--           through the SDK's ICD files, see setup-env.sh
--   shaderc static (libshaderc_combined.a) when the SDK has it, otherwise the shared library,
--           found at run time through the rpath below
function LinkHydroDependencies()
	filter "system:macosx or linux"
		links { "GLFW", "ImGui", "yaml-cpp" }
		libdirs { "%{LibraryDir.VulkanSDK}" }
		-- The Vulkan loader and shaderc are found next to the SDK at run time, without DYLD_/LD_LIBRARY_PATH.
		linkoptions { "-Wl,-rpath," .. VULKAN_SDK .. "/" .. sdkLib }

	filter "system:macosx"
		links
		{
			"vulkan",
			os.isfile(VULKAN_SDK .. "/lib/libshaderc_combined.a") and "shaderc_combined" or "shaderc_shared",
			-- GLFW (Cocoa) needs these.
			"Cocoa.framework",
			"IOKit.framework",
			"CoreFoundation.framework",
			"QuartzCore.framework"
		}

	filter "system:linux"
		links
		{
			"vulkan",
			os.isfile(VULKAN_SDK .. "/lib/libshaderc_combined.a") and "shaderc_combined" or "shaderc_shared",
			"pthread",
			"dl"
		}

	filter {}
end
