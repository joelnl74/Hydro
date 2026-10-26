#pragma once
#include "Hydro/Core/Application.h"

extern Hydro::Application* Hydro::CreateApplication(const Hydro::ApplicationCommandLine& commandLine);

int main(int argc, char** argv)
{
	// The command line, as given: the application reads its own options from it (Oxygen: --project,
	// --scene, --play, --capture... see EditorLaunchOptions).
	Hydro::ApplicationCommandLine commandLine;
	for (int i = 1; i < argc; i++)
	{
		commandLine.Arguments.emplace_back(argv[i]);
	}

	// The application's lifetime is explicit: construction and destruction do no engine work.
	Hydro::Application* app = Hydro::CreateApplication(commandLine);

	app->Init();     // start subsystems in order, then the client's OnInit
	app->Run();      // the frame loop, until Quit() or the window closes
	app->Shutdown(); // client's OnShutdown, then subsystems in reverse order

	const int exitCode = app->GetExitCode();
	delete app;
	return exitCode;
}
