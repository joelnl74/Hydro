#pragma once
#include "Hydro/Core/Application.h"

extern Hydro::Application* Hydro::CreateApplication();

int main(int argc, char** argv)
{
	// Create application.
	auto app = Hydro::CreateApplication();

	// Run application.
	app->Run();

	// Clean application after run.
	delete app;
}
