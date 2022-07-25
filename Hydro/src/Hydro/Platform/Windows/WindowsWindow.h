#pragma once
#include "Hydro/Core/Window.h"
#include "Hydro/Renderer/RendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanPresentation.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Hydro
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		unsigned int GetWidth() const override { return m_Data.Width; }
		unsigned int GetHeight() const override { return m_Data.Height; }

		// Window attributes
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		virtual void* GetNativeWindow() const { return m_Window; }
	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();
	private:
		GLFWwindow* m_Window;


		// Rendering stuff.
		Ref<RendererContext> m_rendererContext;
		VulkanPresentation m_vulkanPresentation;

		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
			bool VSync;
		};

		WindowData m_Data;
	};
}


