#pragma once
#include "Hydro\Events\Event.h"

namespace Hydro
{
	class Layer
	{
	public:
		Layer();
		virtual ~Layer();

		virtual void OnAttach() {};
		virtual void OnDetach() {};
		virtual void OnUpdate() {};
		virtual void OnEvent(Event& event) {}
	};
}
