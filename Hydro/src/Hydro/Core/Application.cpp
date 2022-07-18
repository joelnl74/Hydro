#include "hypch.h"
#include "Application.h"

namespace Hydro
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		s_Instance = this;
		m_Window = Window::Create(WindowProps());

		EventDispatcher::Create();
		EventDispatcher::Get().Subscribe(EventType::WindowClose, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::WindowResize, HY_BIND_EVENT_FN(Application::OnEvent));

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

	// Handle events.
	void Application::OnEvent(Event& e)
	{
		// Handle close event.
		if (e.GetEventType() == EventType::WindowClose)
		{
			ShutDown();
		}

		// Handle resize event.
		if (e.GetEventType() == EventType::WindowResize)
		{
			auto receivedEvent = EventDispatcher::GetEvent<WindowResizeEvent&>(e);

			std::cout << receivedEvent.width;
			std::cout << receivedEvent.heigth;
		}
	}
}
