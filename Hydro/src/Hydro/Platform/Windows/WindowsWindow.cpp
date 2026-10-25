#include "hypch.h"
#include "Hydro/Platform/Windows/WindowsWindow.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"
#include "Hydro/Events/KeyEvents.h"
#include "Hydro/Events/MouseEvents.h"

#include <iostream>

namespace Hydro
{
	static uint8_t s_GLFWWindowCount = 0;

	static void GLFWErrorCallback(int error, const char* description)
	{
		HY_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::~WindowsWindow()
	{
		HY_CORE_ASSERT(m_Window == nullptr, "WindowsWindow destroyed without Shutdown()");
	}

	void WindowsWindow::Init(const WindowSpecification& specification)
	{
		HY_CORE_ASSERT(m_Window == nullptr, "WindowsWindow::Init called twice");

		m_Data.Title = specification.Title;
		m_Data.Width = specification.Width;
		m_Data.Height = specification.Height;

		if (s_GLFWWindowCount == 0)
		{
			glfwInit();
			glfwSetErrorCallback(GLFWErrorCallback);
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_Window = glfwCreateWindow((int)specification.Width, (int)specification.Height, m_Data.Title.c_str(), nullptr, nullptr);
		++s_GLFWWindowCount;

		glfwSetWindowUserPointer(m_Window, &m_Data);
		// VSync is a swapchain setting (RendererSettings::VSync), not a window one: there is no GL context.

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
			{
				WindowCloseEvent closeEvent;
				EventDispatcher::Get().Post(closeEvent);
			});

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int heigth)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
				data.Width = (unsigned int)width;
				data.Height = (unsigned int)heigth;

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
		if (m_Window == nullptr)
			return;

		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0)
		{
			glfwTerminate();
		}
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
	}

	// The monitor that holds most of the window (the primary one when it overlaps none).
	static GLFWmonitor* FindMonitorOf(GLFWwindow* window)
	{
		int windowX = 0, windowY = 0, windowWidth = 0, windowHeight = 0;
		glfwGetWindowPos(window, &windowX, &windowY);
		glfwGetWindowSize(window, &windowWidth, &windowHeight);

		int count = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&count);
		GLFWmonitor* best = glfwGetPrimaryMonitor();
		int bestArea = 0;
		for (int i = 0; i < count; i++)
		{
			const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
			if (mode == nullptr)
			{
				continue;
			}

			int monitorX = 0, monitorY = 0;
			glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
			const int overlapWidth = std::max(0, std::min(windowX + windowWidth, monitorX + mode->width) - std::max(windowX, monitorX));
			const int overlapHeight = std::max(0, std::min(windowY + windowHeight, monitorY + mode->height) - std::max(windowY, monitorY));
			if (overlapWidth * overlapHeight > bestArea)
			{
				bestArea = overlapWidth * overlapHeight;
				best = monitors[i];
			}
		}

		return best;
	}

	void WindowsWindow::SetFullscreen(bool fullscreen)
	{
		if (m_Window == nullptr || fullscreen == m_Fullscreen)
		{
			return;
		}

		if (fullscreen)
		{
			glfwGetWindowPos(m_Window, &m_WindowedX, &m_WindowedY);
			glfwGetWindowSize(m_Window, &m_WindowedWidth, &m_WindowedHeight);

			GLFWmonitor* monitor = FindMonitorOf(m_Window);
			const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
			if (mode == nullptr)
			{
				HY_CORE_WARN("Window: no monitor found, staying windowed");
				return;
			}

			// The monitor's current mode: no mode switch, so going in and out is quick.
			glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		else
		{
			glfwSetWindowMonitor(m_Window, nullptr, m_WindowedX, m_WindowedY, std::max(1, m_WindowedWidth), std::max(1, m_WindowedHeight), 0);
		}

		m_Fullscreen = fullscreen;
	}

}
