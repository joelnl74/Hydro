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

	// Window subsystem. Create only picks the platform implementation; Init opens it, Shutdown releases it.
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

		// Full screen on the window's monitor at its own resolution; false restores the windowed size and position.
		// Posts a WindowResizeEvent like any resize.
		virtual void SetFullscreen(bool fullscreen) = 0;
		virtual bool IsFullscreen() const = 0;

		static Scope<Window> Create();
	};
}
