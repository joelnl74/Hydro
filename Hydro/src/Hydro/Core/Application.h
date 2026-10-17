#pragma once
#include "Hydro/Core/Window.h"
#include "Hydro/Core/LayerController.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"

#include "Hydro/ImGUI/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Hydro
{
	// CPU time spent in each phase of the last frame, in milliseconds.
	struct FrameTimings
	{
		float EventsMs = 0.0f;          // window/OS event polling
		float ImGuiMs = 0.0f;           // building the editor UI (layers' OnImGuiRender)
		float UpdateMs = 0.0f;          // game/scene logic (layers' OnUpdate)
		float RenderMs = 0.0f;          // frame fence wait + acquire + recording (Renderer2D batches, ImGui)
		float PresentMs = 0.0f;         // queue submit + present (includes vsync wait)
		float PlatformWindowsMs = 0.0f; // ImGui windows dragged outside the main window
		float FrameMs = 0.0f;           // whole loop iteration
		uint64_t FrameCount = 0;
		double UptimeSeconds = 0.0;
	};

	class Application
	{
	public:
		Application();
		virtual ~Application();
		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnUpdate() {}
		virtual void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		void Quit();

		inline Window& GetWindow() { return *m_Window; }
		const FrameTimings& GetFrameTimings() const { return m_FrameTimings; }
		static inline Application& Get() { return *s_Instance; }

	private:
		void Run();
		void ShutDown();

	private:
		Scope<Window> m_Window;
		LayerController m_LayerStack;

		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;
		FrameTimings m_FrameTimings;

	private:
		static Application* s_Instance;
		ImGuiLayer *m_imGuiLayer;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}

