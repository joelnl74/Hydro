#pragma once
#include "Hydro/Core/Window.h"
#include "Hydro/Core/LayerController.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"

#include "Hydro/ImGUI/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Hydro
{
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

	private:
		static Application* s_Instance;
		ImGuiLayer *m_imGuiLayer;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}

