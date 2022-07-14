#include "hypch.h"
#include "Application.h"

namespace Hydro
{
	Application* Application::s_Instance = nullptr;


	Application::Application()
	{
		s_Instance = this;
		m_Window = Window::Create(WindowProps());

		m_Running = true;
	}

	Application::~Application()
	{
	}

	void Application::Run()
	{
		while (m_Running)
		{
			m_Window->OnUpdate();
		}
	}

	void Application::ShutDown()
	{
		m_Running = false;
	}
}
