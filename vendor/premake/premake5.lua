project "Premake"
	kind "Utility"

	targetdir (HydroWorkspaceDir .. "/bin/" .. outputdir .. "/%{prj.name}")
	objdir (HydroWorkspaceDir .. "/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		HydroWorkspaceDir .. "/**premake5.lua"
	}

	postbuildmessage "Regenerating project files with Premake5!"
	postbuildcommands
	{
		'"' .. HydroRoot .. '/vendor/premake/bin/premake5" %{_ACTION} --file="' .. HydroWorkspaceDir .. '/premake5.lua"'
	}
