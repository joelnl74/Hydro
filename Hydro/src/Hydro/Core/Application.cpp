#include "hypch.h"
#include "Application.h"

#include "Hydro/Core/EngineResources.h"
#include "Hydro/Core/Time.h"
#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Physics/PhysicsEngine.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/Renderer2D.h"

#include <imgui.h>
#include <thread>
#include <chrono>

namespace Hydro
{
	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		// No engine work here: subsystems start in Init(), which main() calls next.
		s_Instance = this;
	}

	Application::~Application()
	{
		// No engine work here either: everything was stopped in Shutdown().
		s_Instance = nullptr;
	}

	// ---- Startup ------------------------------------------------------------------
	// Order matters: each subsystem may only use the ones started before it.
	void Application::Init()
	{
		// 1. Log: everything after this may log.
		Log::Init();
		HY_CORE_INFO("Starting {0}", m_Specification.Name);

		// 2. Engine resources: where shaders and fonts are, found from the exe (the renderer and ImGui load them).
		EngineResources::Init(m_Specification.Resources);

		// 3. Time: the frame clock.
		Time::Init();

		// 4. Events: must exist before the window, whose callbacks post into it.
		EventDispatcher::Init();

		// 5. Window: OS window + input.
		m_Window = Window::Create();
		m_Window->Init(m_Specification.MainWindow);

		// 6. Renderer: Vulkan context, swapchain, frames, allocator, Renderer2D/3D (needs the window).
		Renderer::Init(*m_Window);

		// The application listens to the events it handles itself and forwards them to layers.
		EventDispatcher& events = EventDispatcher::Get();
		events.Subscribe(EventType::WindowClose, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::WindowResize, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::KeyPressed, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::KeyReleased, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::KeyTyped, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::MouseButtonPressed, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::MouseButtonReleased, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::MouseMoved, HY_BIND_EVENT_FN(Application::OnEvent));
		events.Subscribe(EventType::MouseScrolled, HY_BIND_EVENT_FN(Application::OnEvent));

		// 7. Assets: the open project's assets by handle (empty until OnProjectOpened); creates GPU textures.
		AssetManager::Init();

		// 8. Physics: the libraries' global setup. Scenes make their own worlds while they play.
		PhysicsEngine::Init();

		// 9. ImGui: an overlay, so it sits on top of every application layer.
		m_ImGuiLayer = new ImGuiLayer(m_Specification.ImGui);
		PushOverlay(m_ImGuiLayer);

		HY_CORE_INFO("Engine initialized");
		m_Running = true;

		// 10. The client application: pushes its layers (and may already call Quit()).
		OnInit();
	}

	// ---- Shutdown -----------------------------------------------------------------
	// Exactly the reverse of Init.
	void Application::Shutdown()
	{
		m_Running = false;

		// 10. The client application, while everything is still alive.
		OnShutdown();

		// 9. Layers (ImGui included) own GPU objects, and scenes with physics worlds: detach them once
		//    the GPU is idle, while the device still exists.
		Renderer::WaitIdle();
		m_LayerStack.Clear();
		m_ImGuiLayer = nullptr;

		// 8. Physics.
		PhysicsEngine::Shutdown();

		// 7. Assets.
		AssetManager::Shutdown();

		// 6. Renderer.
		Renderer::Shutdown();

		// 5. Window.
		m_Window->Shutdown();
		m_Window.reset();

		// 4. Events.
		EventDispatcher::Shutdown();

		// 3. Time.
		Time::Shutdown();

		// 2. Engine resources.
		EngineResources::Shutdown();

		// 1. Log, last, so every step above can still log.
		HY_CORE_INFO("Engine shut down");
		Log::Shutdown();
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

	void Application::Quit(int exitCode)
	{
		m_Running = false;
		m_ExitCode = exitCode;
	}

	void Application::Run()
	{
		while (m_Running)
		{
			Time::BeginFrame();

			Time::BeginPhase(FramePhase::Events);
			m_Window->OnUpdate();
			Time::EndPhase(FramePhase::Events);

			Time::BeginPhase(FramePhase::ImGui);
			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}
			m_ImGuiLayer->End();
			Time::EndPhase(FramePhase::ImGui);

			Time::BeginPhase(FramePhase::Update);
			const float deltaTime = Time::GetDeltaTime();
			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(deltaTime);
			}
			Time::EndPhase(FramePhase::Update);

			// A minimized window has a 0x0 framebuffer: nothing can be presented, so skip
			// rendering (and don't spin the CPU) until it is restored.
			if (m_Minimized == false)
			{
				// One command buffer: the offscreen views, then the swapchain pass with ImGui, then submit and present.
				Renderer::RenderFrame();
			}
			else
			{
				Renderer2D::DiscardFrame();
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}

			// Extra ImGui windows dragged outside the main window. ImGui requires this every frame,
			// including frames where the renderer skipped drawing (resize, minimise).
			Time::BeginPhase(FramePhase::PlatformWindows);
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
			Time::EndPhase(FramePhase::PlatformWindows);

			Time::EndFrame();
		}
	}

	void Application::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::WindowClose)
		{
			// Only stop the loop here: main() calls Shutdown() once the current frame is done.
			Quit();
		}

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
