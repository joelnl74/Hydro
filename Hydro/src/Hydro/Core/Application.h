#pragma once
#include "Hydro/Core/Window.h"

int main(int argc, char** argv);

namespace Hydro
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

		Window& GetWindow() { return *m_Window; }

	private:
		void Run();
		void ShutDown();

	private:
		Scope<Window> m_Window;

		bool m_Running = true;
		bool m_Minimized = false;

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}

