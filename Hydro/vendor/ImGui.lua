-- Dear ImGui (submodule in imgui/), the core library only. The GLFW and Vulkan backends are
-- compiled into Hydro (Platform/GLFW/imgui_impl_glfw.*, Platform/Vulkan/imgui_impl_vulkan.*).
project "ImGui"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	warnings "Off"
	location "imgui"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"imgui/imconfig.h",
		"imgui/imgui.h",
		"imgui/imgui.cpp",
		"imgui/imgui_draw.cpp",
		"imgui/imgui_internal.h",
		"imgui/imgui_tables.cpp",
		"imgui/imgui_widgets.cpp",
		"imgui/imstb_rectpack.h",
		"imgui/imstb_textedit.h",
		"imgui/imstb_truetype.h",
		"imgui/imgui_demo.cpp"
	}

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		pic "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		optimize "on"
		symbols "off"
