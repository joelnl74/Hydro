#include "hypch.h"
#include "Application.h"

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/Renderer2D.h"
#include "Hydro/Renderer/Renderer3D.h"
#include <glm/ext/matrix_transform.hpp>

namespace Hydro
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		s_Instance = this;

		// Initialize and create engine systems.
		Log::Init();

		// Create engine systems.
		m_Window = Window::Create(WindowProps());
		Renderer::Create(*m_Window);
		EventDispatcher::Create();

		// Setup events for application.
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

		m_imGuiLayer = new ImGuiLayer();
		PushLayer(m_imGuiLayer);

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

	void Application::Quit()
	{
		m_Running = false;
	}

	void Application::Run()
	{
		OnInit();
		while (m_Running)
		{
			m_Window->OnUpdate();

			m_imGuiLayer->Begin();

			glm::mat4 transform = glm::mat4(1);
			glm::mat4 transform1 = glm::mat4(1);

			transform = glm::translate(transform, glm::vec3(800, 600, 0));
			transform = glm::scale(transform, glm::vec3(400, 300, 0));
			Renderer2D::DrawQuad(transform, glm::vec4(1, 0, 0, 1));

			transform1 = glm::translate(transform1, glm::vec3(0, 0, 0));
			transform1 = glm::scale(transform1, glm::vec3(100, 100, 0));
			Renderer2D::DrawQuad(transform1, glm::vec4(0, 1, 0, 1));

			for (Layer* layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}
			Renderer::GetVulkanSwapChain()->CreateVulkanComposeImage();
			m_imGuiLayer->End();

			for (Layer* layer : m_LayerStack)
				layer->OnUpdate();

			Renderer::RenderFrame();
		}

		Renderer::ShutDown();
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
		}

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
		}
	}
}
