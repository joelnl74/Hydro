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

		// Window attributes
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		virtual void* GetNativeWindow() const { return m_Window; }
		virtual GLFWwindow* GetGLFWWindow() const { return m_Window; }

	private:
		GLFWwindow* m_Window = nullptr;

		struct WindowData
		{
			std::string Title;
			unsigned int Width = 0, Height = 0;
			bool VSync = true;
		};

		WindowData m_Data;
	};
}


