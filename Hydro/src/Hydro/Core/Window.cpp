#include "hypch.h"
#include "Window.h"

#include "Hydro/Platform/GLFW/GlfwWindow.h"

namespace Hydro
{
	Scope<Window> Window::Create()
	{
		// Every desktop platform (Windows, macOS, Linux) uses the GLFW window.
		return CreateScope<GlfwWindow>();
	}
}
