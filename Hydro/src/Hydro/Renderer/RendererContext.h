#pragma once
#include "Hydro/Core/Base.h"

namespace Hydro
{
	class RendererContext
	{
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		virtual void Init() = 0;

		static Scope<RendererContext> Create();
	};
}
