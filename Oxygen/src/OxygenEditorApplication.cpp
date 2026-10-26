#include <Hydro/Core/EntryPoint.h>
#include "Hydro.h"

#include "EditorLayer.h"
#include "EditorPaths.h"
#include "EditorSettings.h"
#include "Scripts/ExampleScripts.h"

#include <cstdio>

class OxygenEditorApplication : public Hydro::Application
{
public:
	OxygenEditorApplication(const Hydro::ApplicationSpecification& specification, const Hydro::EditorLaunchOptions& launch)
		: Application(specification), m_Launch(launch)
	{
	}

	// Engine subsystems are up when this runs; layers pushed here are detached in Application::Shutdown.
	virtual void OnInit() override
	{
		// Scripts must be registered before the editor opens a scene that uses them.
		Hydro::RegisterExampleScripts();

		PushLayer(new Hydro::EditorLayer(m_Launch));
	}

private:
	Hydro::EditorLaunchOptions m_Launch;
};

Hydro::Application* Hydro::CreateApplication(const Hydro::ApplicationCommandLine& commandLine)
{
	if (commandLine.Has("--help"))
	{
		// Before the log exists: plain output.
		std::printf("Oxygen [--project <file.hyproj>] [--scene <name | file.hyscene>] [--play] [--frames <n>]\n"
			"       [--capture <file.png>] [--validate] [--exit]\n"
			"--capture saves the Game view (1280 x 720) after --frames frames (120); --validate loads every scene\n"
			"of the project and exits with 1 when any warned; both imply --exit.\n");
	}
	const Hydro::EditorLaunchOptions launch = Hydro::EditorLaunchOptions::Parse(commandLine);

	Hydro::ApplicationSpecification specification;
	specification.Name = "Oxygen";
	specification.MainWindow.Title = "Oxygen";

	// The editor's UI font (Open Sans, in Oxygen/assets/Fonts), at the size saved in the editor
	// settings. Runs before the log exists, so ReadFontSize never logs.
	float fontSize = Hydro::EditorSettings().FontSize;
	Hydro::EditorSettingsSerializer::ReadFontSize(Hydro::EditorPaths::GetEditorSettingsPath(), fontSize);

	const std::filesystem::path fonts = "assets/Fonts/opensans";
	specification.ImGui.Fonts =
	{
		{ "Regular", fonts / "OpenSans-Regular.ttf", fontSize },
		{ "Bold", fonts / "OpenSans-Bold.ttf", fontSize },
	};

	return new OxygenEditorApplication(specification, launch);
}
