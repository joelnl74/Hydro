#pragma once
#include "Hydro/Core/Window.h"
#include "Hydro/Core/LayerStack.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"

#include "Hydro/ImGUI/ImGuiLayer.h"

#include <algorithm>
#include <string>
#include <vector>

int main(int argc, char** argv);

namespace Hydro
{
	// What main was started with (argv without the program), for the application to read its options
	// from. Flags are "--name"; a value is the argument after its flag.
	struct ApplicationCommandLine
	{
		std::vector<std::string> Arguments;

		bool Has(const std::string& flag) const
		{
			return std::find(Arguments.begin(), Arguments.end(), flag) != Arguments.end();
		}

		// The argument after `flag`, or `fallback` when the flag isn't there or is the last argument.
		std::string Get(const std::string& flag, const std::string& fallback = std::string()) const
		{
			for (size_t i = 0; i + 1 < Arguments.size(); i++)
			{
				if (Arguments[i] == flag)
				{
					return Arguments[i + 1];
				}
			}
			return fallback;
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Hydro Application";
		WindowSpecification MainWindow;
		// Fonts for the ImGui layer (the application's own font files).
		ImGuiLayerSpecification ImGui;
	};

	// Owns the subsystems and runs the loop. Its lifetime is three explicit steps, called by main()
	// (EntryPoint.h): Init() -> Run() -> Shutdown(). The constructor and destructor do no work.
	//
	// Init starts the subsystems in a fixed order and Shutdown stops them in exactly the reverse
	// order; see docs/Architecture.md "Startup and shutdown".
	class Application
	{
	public:
		Application(const ApplicationSpecification& specification = ApplicationSpecification());
		virtual ~Application();

		// Hooks for the client application. OnInit runs after every engine subsystem is up;
		// OnShutdown runs first thing in Shutdown, while everything is still alive.
		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		// Ends the frame loop after this frame; exitCode is what main returns (0 = fine).
		void Quit(int exitCode = 0);
		int GetExitCode() const { return m_ExitCode; }

		inline Window& GetWindow() { return *m_Window; }
		const ApplicationSpecification& GetSpecification() const { return m_Specification; }
		static inline Application& Get() { return *s_Instance; }

	private:
		void Init();
		void Run();
		void Shutdown();

	private:
		ApplicationSpecification m_Specification;

		Scope<Window> m_Window;
		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer = nullptr; // owned by m_LayerStack

		bool m_Running = false;
		bool m_Minimized = false;
		int m_ExitCode = 0;

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication(const ApplicationCommandLine& commandLine);
}
