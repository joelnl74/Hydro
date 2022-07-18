#include "hypch.h"
#include "Application.h"

namespace Hydro
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		Log::Init();

		s_Instance = this;
		m_Window = Window::Create(WindowProps());

		EventDispatcher::Create();
		EventDispatcher::Get().Subscribe(EventType::WindowClose, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::WindowResize, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::KeyPressed, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::KeyReleased, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::KeyTyped, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::MouseButtonPressed, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::MouseButtonReleased, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::MouseMoved, HY_BIND_EVENT_FN(Application::OnEvent));
		EventDispatcher::Get().Subscribe(EventType::MouseScrolled, HY_BIND_EVENT_FN(Application::OnEvent));

		m_Running = true;

		HY_CORE_TRACE("Engine initialized");
	}

	Application::~Application()
	{
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PopOverlay(layer);
		layer->OnAttach();
	}

	void Application::Run()
	{
		OnInit();
		while (m_Running)
		{
			m_Window->OnUpdate();
			for (Layer* layer : m_LayerStack)
				layer->OnUpdate();
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
			WindowResizeEvent receivedEvent = EventDispatcher::GetEvent<WindowResizeEvent&>(e);

			std::cout << receivedEvent.width;
			std::cout << receivedEvent.heigth;
		}

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
		}
	}
}
