#include "hypch.h"
#include "Window.h"

#ifdef HY_PLATFORM_WINDOWS
#include "Hydro/Platform/Windows/WindowsWindow.h"
#endif

namespace Hydro
{
	Scope<Window> Window::Create()
	{
#ifdef HY_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>();
#else
		return nullptr;
#endif
	}
}
