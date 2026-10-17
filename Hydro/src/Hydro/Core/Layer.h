#pragma once
#include "Hydro/Events/Event.h"

namespace Hydro
{
	class Layer
	{
	public:
		Layer();
		virtual ~Layer();

		virtual void OnAttach() {};
		virtual void OnDetach() {};
		virtual void OnUpdate(float deltaTime) {};
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& event) {}
	};
}
