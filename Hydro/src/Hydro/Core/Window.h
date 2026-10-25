#pragma once
#include "Hydro/Core/Base.h"

#include <string>

namespace Hydro
{
	struct WindowSpecification
	{
		std::string Title = "Hydro Engine";
		uint32_t Width = 1280;
		uint32_t Height = 720;
	};

	// Window subsystem. Window::Create only picks the platform implementation; nothing is opened
	// until Init, and everything is released in Shutdown (never in the destructor).
	class Window
	{
	public:
		virtual ~Window() = default;

		virtual void Init(const WindowSpecification& specification) = 0;
		virtual void Shutdown() = 0;

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual void* GetNativeWindow() const = 0;

		// Full screen on the monitor the window is on (the monitor's own resolution and refresh rate);
		// false goes back to the windowed size and position it had before. Posts a WindowResizeEvent
		// like any other resize, so the swapchain follows.
		virtual void SetFullscreen(bool fullscreen) = 0;
		virtual bool IsFullscreen() const = 0;

		static Scope<Window> Create();
	};
}
