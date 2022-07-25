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

		virtual void Init() = 0;
		virtual void ShutDown() = 0;

		static Ref<RendererContext> Get() { return s_Instance; }
		static Ref<RendererContext> Create();
	
	private:
		static Ref<RendererContext> s_Instance;
	};
}
