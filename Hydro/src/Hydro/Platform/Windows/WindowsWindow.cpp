#pragma once
#include "hypch.h"
#include "Hydro\Platform\Windows\WindowsWindow.h"
#include "Hydro\Events\EventDispatcher.h"
#include "Hydro\Events\ApplicationEvents.h"
#include "Hydro\Events\KeyEvents.h"
#include "Hydro\Events\MouseEvents.h"

#include "Hydro\Platform\Vulkan\VulkanRendererContext.h"
#include <iostream>

namespace Hydro
{
	static uint8_t s_GLFWWindowCount = 0;

	static void GLFWErrorCallback(int error, const char* description)
	{
		// HZ_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		m_vulkanPresentation.ShutDown();
		Shutdown();
	}

	void Hydro::WindowsWindow::Init(const WindowProps& props)
	{
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;

		if (s_GLFWWindowCount == 0)
		{
			int success = glfwInit();
			glfwSetErrorCallback(GLFWErrorCallback);
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
		++s_GLFWWindowCount;

		glfwSetWindowUserPointer(m_Window, &m_Data);
		SetVSync(true);

		m_rendererContext = RendererContext::Create();
		m_rendererContext->Init();

		Ref<VulkanRendererContext> context = std::dynamic_pointer_cast<VulkanRendererContext>(m_rendererContext);

		m_vulkanPresentation.Init(context->GetInstance(), context->GetVulkanDevice());
		m_vulkanPresentation.InitSurface(*this);
		m_vulkanPresentation.CreateSwapChain(*this, true);

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
			{
				EventDispatcher::Get().Post(WindowCloseEvent());
			});

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int heigth)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

				WindowResizeEvent resizeEvent = WindowResizeEvent();
				resizeEvent.width = width;
				resizeEvent.heigth = heigth;

				EventDispatcher::Get().Post(resizeEvent);
			});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
			{
				auto& data = *((WindowData*)glfwGetWindowUserPointer(window));

				switch (action)
				{
				case GLFW_PRESS:
				{
					KeyPressedEvent event((KeyCode)key, 0);
					EventDispatcher::Get().Post(event);
					break;
				}
				case GLFW_RELEASE:
				{
					KeyReleasedEvent event((KeyCode)key);
					EventDispatcher::Get().Post(event);
					break;
				}
				case GLFW_REPEAT:
				{
					KeyPressedEvent event((KeyCode)key, 1);
					EventDispatcher::Get().Post(event);
					break;
				}
				}
			});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, uint32_t codepoint)
			{
				auto& data = *((WindowData*)glfwGetWindowUserPointer(window));

				KeyTypedEvent event((KeyCode)codepoint);
				EventDispatcher::Get().Post(event);
			});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
			{
				auto& data = *((WindowData*)glfwGetWindowUserPointer(window));

				switch (action)
				{
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent event(button);
					EventDispatcher::Get().Post(event);
					break;
				}
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event(button);
					EventDispatcher::Get().Post(event);
					break;
				}
				}
			});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
			{
				auto& data = *((WindowData*)glfwGetWindowUserPointer(window));

				MouseScrolledEvent event((float)xOffset, (float)yOffset);
				EventDispatcher::Get().Post(event);
			});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y)
			{
				auto& data = *((WindowData*)glfwGetWindowUserPointer(window));
				MouseMovedEvent event((float)x, (float)y);
				EventDispatcher::Get().Post(event);
			});
	}

	void WindowsWindow::Shutdown()
	{
		m_vulkanPresentation.ShutDown();

		glfwDestroyWindow(m_Window);
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0)
		{
			glfwTerminate();
		}
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
		m_vulkanPresentation.DrawFrame();
	}


	void WindowsWindow::SetVSync(bool enabled)
	{
		if (enabled)
			glfwSwapInterval(1);
		else
			glfwSwapInterval(0);

		m_Data.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const
	{
		return m_Data.VSync;
	}
}
