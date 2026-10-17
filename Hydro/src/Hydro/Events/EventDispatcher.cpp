#include <hypch.h>
#include "EventDispatcher.h"

namespace Hydro
{
	EventDispatcher* EventDispatcher::s_Instance = nullptr;

	void EventDispatcher::Init()
	{
		HY_CORE_ASSERT(s_Instance == nullptr, "EventDispatcher::Init called twice");
		s_Instance = new EventDispatcher();
	}

	void EventDispatcher::Shutdown()
	{
		delete s_Instance;
		s_Instance = nullptr;
	}
}
