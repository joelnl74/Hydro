#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Core/Window.h"

namespace Hydro
{
	class RendererContext
	{
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		virtual void Init(Window& window) = 0;

		static Scope<RendererContext> Create();
	};
}
