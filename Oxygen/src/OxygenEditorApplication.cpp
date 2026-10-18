#include <Hydro/Core/EntryPoint.h>
#include "Hydro.h"

#include "EditorLayer.h"
#include "Scripts/ExampleScripts.h"

class OxygenEditorApplication : public Hydro::Application
{
public:
	OxygenEditorApplication(const Hydro::ApplicationSpecification& specification)
		: Application(specification)
	{
	}

	// Engine subsystems are up when this runs; layers pushed here are detached in Application::Shutdown.
	virtual void OnInit() override
	{
		// Scripts must be registered before the editor opens a scene that uses them.
		Oxygen::RegisterExampleScripts();

		PushLayer(new Hydro::EditorLayer());
	}
};

Hydro::Application* Hydro::CreateApplication()
{
	Hydro::ApplicationSpecification specification;
	specification.Name = "Oxygen";
	specification.MainWindow.Title = "Oxygen";

	return new OxygenEditorApplication(specification);
}
