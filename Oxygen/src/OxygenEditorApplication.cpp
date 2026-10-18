#include <Hydro/Core/EntryPoint.h>
#include "Hydro.h"

#include "EditorLayer.h"
#include "EditorPaths.h"
#include "EditorSettings.h"
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
		Hydro::RegisterExampleScripts();

		PushLayer(new Hydro::EditorLayer());
	}
};

Hydro::Application* Hydro::CreateApplication()
{
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

	return new OxygenEditorApplication(specification);
}
