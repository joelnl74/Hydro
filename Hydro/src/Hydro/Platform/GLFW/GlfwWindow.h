#pragma once
#include "Hydro/Core/Window.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Hydro
{
	// The desktop window on every platform (Windows, macOS, Linux): GLFW does the per-OS work.
	// Width and height are the framebuffer size in pixels, which is what the swapchain needs; on a
	// Retina display that is larger than the window size GLFW reports in points.
	class GlfwWindow : public Window
	{
	public:
		GlfwWindow() = default;
		~GlfwWindow() override;

		void Init(const WindowSpecification& specification) override;
		void Shutdown() override;

		void OnUpdate() override;

		uint32_t GetWidth() const override { return m_Data.Width; }
		uint32_t GetHeight() const override { return m_Data.Height; }

		void* GetNativeWindow() const override { return m_Window; }
		GLFWwindow* GetGLFWWindow() const { return m_Window; }

	private:
		struct WindowData
		{
			std::string Title;
			uint32_t Width = 0;
			uint32_t Height = 0;
		};

		GLFWwindow* m_Window = nullptr;
		WindowData m_Data;
	};
}
