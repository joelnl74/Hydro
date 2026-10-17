#pragma once
#include "Hydro/Core/Application.h"

extern Hydro::Application* Hydro::CreateApplication();

int main(int argc, char** argv)
{
	// The application's lifetime is explicit: construction and destruction do no engine work.
	Hydro::Application* app = Hydro::CreateApplication();

	app->Init();     // start subsystems in order, then the client's OnInit
	app->Run();      // the frame loop, until Quit() or the window closes
	app->Shutdown(); // client's OnShutdown, then subsystems in reverse order

	delete app;
}
