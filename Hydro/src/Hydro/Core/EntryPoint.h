#pragma once
#include "Hydro/Core/Application.h"

extern Hydro::Application* Hydro::CreateApplication();

// argc/argv: command-line options (--project, --scene) are planned (Roadmap C8).
int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	// The application's lifetime is explicit: construction and destruction do no engine work.
	Hydro::Application* app = Hydro::CreateApplication();

	app->Init();     // start subsystems in order, then the client's OnInit
	app->Run();      // the frame loop, until Quit() or the window closes
	app->Shutdown(); // client's OnShutdown, then subsystems in reverse order

	delete app;
}
