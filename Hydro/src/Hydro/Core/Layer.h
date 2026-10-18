#pragma once
#include "Hydro/Events/Event.h"

namespace Hydro
{
	class Layer
	{
	public:
		Layer();
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate([[maybe_unused]] float deltaTime) {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent([[maybe_unused]] Event& event) {}
	};
}
