#pragma once
#include "Hydro/Core/Window.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Hydro
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow() = default;
		virtual ~WindowsWindow();

		void Init(const WindowSpecification& specification) override;
		void Shutdown() override;

		void OnUpdate() override;

		unsigned int GetWidth() const override { return m_Data.Width; }
		unsigned int GetHeight() const override { return m_Data.Height; }

		virtual void* GetNativeWindow() const { return m_Window; }
		virtual GLFWwindow* GetGLFWWindow() const { return m_Window; }

		void SetFullscreen(bool fullscreen) override;
		bool IsFullscreen() const override { return m_Fullscreen; }

	private:
		GLFWwindow* m_Window = nullptr;
		bool m_Fullscreen = false;
		// Where the window was before going full screen, to go back to.
		int m_WindowedX = 0, m_WindowedY = 0, m_WindowedWidth = 0, m_WindowedHeight = 0;

		struct WindowData
		{
			std::string Title;
			unsigned int Width = 0, Height = 0;
		};

		WindowData m_Data;
	};
}


