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
		bool VSync = true;
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

		// Window attributes
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		virtual void* GetNativeWindow() const = 0;

		static Scope<Window> Create();
	};
}
