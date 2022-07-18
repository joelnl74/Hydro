#include <hypch.h>
#include "EventDispatcher.h"

namespace Hydro
{
	EventDispatcher* EventDispatcher::s_Instance = nullptr;

	void EventDispatcher::Create()
	{
		s_Instance = new EventDispatcher();
	}
}
