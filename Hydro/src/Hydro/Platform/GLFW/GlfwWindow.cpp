#include "hypch.h"
#include "Hydro/Platform/GLFW/GlfwWindow.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/Events/ApplicationEvents.h"
#include "Hydro/Events/KeyEvents.h"
#include "Hydro/Events/MouseEvents.h"

namespace Hydro
{
	static uint8_t s_GLFWWindowCount = 0;

	static void GLFWErrorCallback(int error, const char* description)
	{
		HY_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	GlfwWindow::~GlfwWindow()
	{
		HY_CORE_ASSERT(m_Window == nullptr, "GlfwWindow destroyed without Shutdown()");
	}

	void GlfwWindow::Init(const WindowSpecification& specification)
	{
		HY_CORE_ASSERT(m_Window == nullptr, "GlfwWindow::Init called twice");

		m_Data.Title = specification.Title;

		if (s_GLFWWindowCount == 0)
		{
			glfwSetErrorCallback(GLFWErrorCallback);
			if (glfwInit() == GLFW_FALSE)
			{
				throw std::runtime_error("Failed to initialise GLFW.");
			}
		}

		// Vulkan only: no OpenGL context. VSync is a swapchain setting (RendererSettings::VSync).
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_Window = glfwCreateWindow((int)specification.Width, (int)specification.Height, m_Data.Title.c_str(), nullptr, nullptr);
		if (m_Window == nullptr)
		{
			throw std::runtime_error("Failed to create the window.");
		}
		++s_GLFWWindowCount;

		int framebufferWidth = 0;
		int framebufferHeight = 0;
		glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
		m_Data.Width = (uint32_t)framebufferWidth;
		m_Data.Height = (uint32_t)framebufferHeight;

		glfwSetWindowUserPointer(m_Window, &m_Data);

		glfwSetWindowCloseCallback(m_Window, []([[maybe_unused]] GLFWwindow* window)
		{
			WindowCloseEvent closeEvent;
			EventDispatcher::Get().Post(closeEvent);
		});

		// The framebuffer size (pixels), not the window size (points on macOS): the renderer sizes the
		// swapchain from it, and 0 x 0 means minimised.
		glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width = (uint32_t)width;
			data.Height = (uint32_t)height;

			WindowResizeEvent resizeEvent;
			resizeEvent.width = width;
			resizeEvent.heigth = height;
			EventDispatcher::Get().Post(resizeEvent);
		});

		glfwSetKeyCallback(m_Window, []([[maybe_unused]] GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
		{
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
				default:
				{
					break;
				}
			}
		});

		glfwSetCharCallback(m_Window, []([[maybe_unused]] GLFWwindow* window, uint32_t codepoint)
		{
			KeyTypedEvent event((KeyCode)codepoint);
			EventDispatcher::Get().Post(event);
		});

		glfwSetMouseButtonCallback(m_Window, []([[maybe_unused]] GLFWwindow* window, int button, int action, [[maybe_unused]] int mods)
		{
			if (action == GLFW_PRESS)
			{
				MouseButtonPressedEvent event(button);
				EventDispatcher::Get().Post(event);
			}
			else if (action == GLFW_RELEASE)
			{
				MouseButtonReleasedEvent event(button);
				EventDispatcher::Get().Post(event);
			}
		});

		glfwSetScrollCallback(m_Window, []([[maybe_unused]] GLFWwindow* window, double xOffset, double yOffset)
		{
			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			EventDispatcher::Get().Post(event);
		});

		glfwSetCursorPosCallback(m_Window, []([[maybe_unused]] GLFWwindow* window, double x, double y)
		{
			MouseMovedEvent event((float)x, (float)y);
			EventDispatcher::Get().Post(event);
		});
	}

	void GlfwWindow::Shutdown()
	{
		if (m_Window == nullptr)
		{
			return;
		}

		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0)
		{
			glfwTerminate();
		}
	}

	void GlfwWindow::OnUpdate()
	{
		glfwPollEvents();
	}
}
