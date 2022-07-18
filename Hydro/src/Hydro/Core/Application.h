#pragma once
#include "Hydro/Core/Window.h"
#include "Hydro/Core/LayerController.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"

int main(int argc, char** argv);

namespace Hydro
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

		Window& GetWindow() { return *m_Window; }
		static Application& Get() { return *s_Instance; }

	private:
		void Run();
		void ShutDown();
		void OnEvent(Event& e);

	private:
		Scope<Window> m_Window;
		LayerController m_LayerStack;

		bool m_Running = true;
		bool m_Minimized = false;

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}

