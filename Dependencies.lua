
-- Hydro Dependencies
VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Hydro/vendor/GLFW/include"
IncludeDir["ImGui"] = "%{wks.location}/Hydro/vendor/ImGui"
IncludeDir["ImGuizmo"] = "%{wks.location}/Hydro/vendor/ImGuizmo" -- Oxygen only (transform gizmo)
IncludeDir["glm"] = "%{wks.location}/Hydro/vendor/glm"
IncludeDir["yaml_cpp"] = "%{wks.location}/Hydro/vendor/yaml-cpp/include"
IncludeDir["box2d"] = "%{wks.location}/Hydro/vendor/box2d/include"
IncludeDir["VMA"] = "%{wks.location}/Hydro/vendor/VulkanMemoryAllocator"
IncludeDir["entt"] = "%{wks.location}/Hydro/vendor/entt"
IncludeDir["spdlog"] = "%{wks.location}/Hydro/vendor/spdlog/include"
IncludeDir["stb"] = "%{wks.location}/Hydro/vendor/stb/include"
IncludeDir["cgltf"] = "%{wks.location}/Hydro/vendor/cgltf"
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
