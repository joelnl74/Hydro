#pragma once
#include "Hydro/Core/Window.h"
#include "Hydro/Core/LayerStack.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"

#include "Hydro/ImGUI/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Hydro
{
	struct ApplicationSpecification
	{
		std::string Name = "Hydro Application";
		WindowSpecification MainWindow;
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
		void Quit();

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

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}
