#pragma once
#include "Event.h"

namespace Hydro
{
	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;

		EVENT_CLASS_TYPE(WindowClose)
	};

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent() = default;

		EVENT_CLASS_TYPE(WindowResize)
	};
}
