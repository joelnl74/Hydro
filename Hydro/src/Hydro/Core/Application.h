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
	// main's arguments (without the program). Flags are "--name"; a value is the argument after its flag.
	struct ApplicationCommandLine
	{
		std::vector<std::string> Arguments;

		bool Has(const std::string& flag) const
		{
			return std::find(Arguments.begin(), Arguments.end(), flag) != Arguments.end();
		}

		// `fallback` when the flag is missing or is the last argument.
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
		// The application's font files.
		ImGuiLayerSpecification ImGui;
	};

	// Owns the subsystems and runs the loop: main() calls Init, Run and Shutdown (subsystems stop in reverse order;
	// see docs/Architecture.md, "Startup and shutdown"). The constructor and destructor do no work.
	class Application
	{
	public:
		Application(const ApplicationSpecification& specification = ApplicationSpecification());
		virtual ~Application();

		// OnInit runs once every subsystem is up; OnShutdown first thing in Shutdown, while everything is alive.
		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		// Ends the loop after this frame; main returns exitCode.
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
