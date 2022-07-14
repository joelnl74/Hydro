#include "hypch.h"
#include "Window.h"

#ifdef HY_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Hydro
{
	Scope<Window> Window::Create(const WindowProps& props)
	{
#ifdef HY_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
#else
		return nullptr;
#endif
	}
}


