#include "hypch.h"
#include "Application.h"

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/Renderer2D.h"
#include "Hydro/Renderer/Renderer3D.h"
#include <glm/ext/matrix_transform.hpp>

#include "Hydro/Utils/PlatformUtils.h"

#include <imgui.h>
#include <thread>
#include <chrono>

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

		// ImGui sits on top of every application layer.
		m_imGuiLayer = new ImGuiLayer();
		PushOverlay(m_imGuiLayer);

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
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::Quit()
	{
		m_Running = false;
	}

	void Application::Run()
	{
		using Clock = std::chrono::steady_clock;
		auto Ms = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<float, std::milli>(b - a).count(); };

		OnInit();
		const Clock::time_point startTime = Clock::now();

		while (m_Running)
		{
			// Timings are written at the end of the frame, so whatever reads them
			// (e.g. the editor's stats window) always sees the previous complete frame.
			FrameTimings timings;
			const Clock::time_point frameStart = Clock::now();

			m_Window->OnUpdate();
			const Clock::time_point afterEvents = Clock::now();

			m_imGuiLayer->Begin();

			for (Layer* layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}
			
			m_imGuiLayer->End();
			const Clock::time_point afterImGui = Clock::now();

			for (Layer* layer : m_LayerStack)
				layer->OnUpdate(m_FrameTimings.FrameMs / 1000.0f);
			const Clock::time_point afterUpdate = Clock::now();
			bool rendered = false;

			// A minimized window has a 0x0 framebuffer: nothing can be presented, so skip
			// rendering (and don't spin the CPU) until it is restored.
			if (m_Minimized == false)
			{
				// One command buffer for the frame: Renderer2D's offscreen pass, then the
				// swapchain pass with ImGui, then submit + present.
				Renderer::RenderFrame();
				rendered = true;
			}
			else
			{
				Renderer2D::DiscardFrame();
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}

			// Extra ImGui windows dragged outside the main window. ImGui requires this every
			// frame; it used to run inside the swapchain's command-buffer recording, so it was
			// skipped whenever DrawFrame() returned early on a resize -> ImGui assert/crash.
			const Clock::time_point beforePlatform = Clock::now();
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
			const Clock::time_point frameEnd = Clock::now();

			timings.EventsMs = Ms(frameStart, afterEvents);
			timings.ImGuiMs = Ms(afterEvents, afterImGui);
			timings.UpdateMs = Ms(afterImGui, afterUpdate);
			timings.RenderMs = rendered ? Renderer::GetLastRecordMs() : 0.0f;   // fence wait + acquire + record
			timings.PresentMs = rendered ? Renderer::GetLastPresentMs() : 0.0f; // submit + present
			timings.PlatformWindowsMs = Ms(beforePlatform, frameEnd);
			timings.FrameMs = Ms(frameStart, frameEnd);
			timings.FrameCount = m_FrameTimings.FrameCount + 1;
			timings.UptimeSeconds = std::chrono::duration<double>(frameEnd - startTime).count();
			m_FrameTimings = timings;
		}

		ShutDown();
	}

	void Application::ShutDown()
	{
		m_Running = false;

		OnShutdown();

		// Layers (ImGui included) own GPU objects, so detach them while the device still exists.
		Renderer::WaitIdle();
		m_LayerStack.Clear();
		m_imGuiLayer = nullptr;

		Renderer::ShutDown();
	}

	// Handle events.
	void Application::OnEvent(Event& e)
	{
		// Handle close event.
		if (e.GetEventType() == EventType::WindowClose)
		{
			// Only stop the loop here: Run() tears everything down once the current frame is done.
			Quit();
		}

		// Handle resize event.
		if (e.GetEventType() == EventType::WindowResize)
		{
			WindowResizeEvent& resizeEvent = static_cast<WindowResizeEvent&>(e);

			m_Minimized = resizeEvent.width == 0 || resizeEvent.heigth == 0;
			Renderer::OnWindowResize((uint32_t)resizeEvent.width, (uint32_t)resizeEvent.heigth);
		}

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
		}
	}
}
