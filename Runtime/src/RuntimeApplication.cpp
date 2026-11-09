#include <Hydro/Core/EntryPoint.h>
#include "Hydro.h"
#include "Hydro/Utils/PlatformUtils.h"

#include "GameRegistration.h"
#include "RuntimeLayer.h"

#include <cstdio>

namespace Hydro
{
	// The standalone player: runs one project's game, with no editor.
	class RuntimeApplication : public Application
	{
	public:
		RuntimeApplication(const ApplicationSpecification& specification, const std::filesystem::path& projectPath)
			: Application(specification), m_ProjectPath(projectPath)
		{
		}

		// Engine subsystems are up when this runs; the layer is detached in Application::Shutdown.
		virtual void OnInit() override
		{
			RegisterGame(); // data types, scripts and the game session, before any scene or data file uses them
			PushLayer(new RuntimeLayer(m_ProjectPath));
		}

	private:
		std::filesystem::path m_ProjectPath;
	};
}

Hydro::Application* Hydro::CreateApplication(const Hydro::ApplicationCommandLine& commandLine)
{
	if (commandLine.Has("--help"))
	{
		// Before the log exists: plain output.
		std::printf("<Game>.exe [--project <file.hyproj>]\n"
			"Without --project it plays Project/<exe name>.hyproj next to the exe (a packaged build, made by\n"
			"scripts/Package.bat or File > Build in Oxygen) and runs in the exe's folder. With --project it runs in\n"
			"the current folder. The engine's files (assets/shaders) are always the copy next to the exe.\n");
	}

	std::filesystem::path projectPath = commandLine.Get("--project");
	std::error_code ec;
	if (projectPath.empty())
	{
		// A packaged game: everything is next to the exe. Only the project named after the exe is played.
		const std::filesystem::path executable = Process::GetExecutablePath();
		std::filesystem::current_path(executable.parent_path(), ec);
		projectPath = executable.parent_path() / "Project" / (executable.stem().string() + Project::FileExtension);
	}
	else
	{
		projectPath = std::filesystem::absolute(projectPath, ec);
	}

	ApplicationSpecification specification;
	specification.Name = projectPath.stem().string();
	specification.MainWindow.Title = specification.Name;
	return new RuntimeApplication(specification, projectPath);
}
