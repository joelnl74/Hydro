#include <hypch.h>
#include "EventDispatcher.h"

namespace Hydro
{
	EventDispatcher* EventDispatcher::s_Instance = nullptr;


	void EventDispatcher::Create()
	{
		s_Instance = new EventDispatcher();
	}

	void EventDispatcher::Subscribe(EventType eventType, SlotType&& slot)
	{
		m_Observers[eventType].push_back(slot);
	}

	void EventDispatcher::Post(const Event& postEvent) const
	{
		auto type = postEvent.GetEventType();

		// Ignore events for which we do not have an observer (yet).
		if (m_Observers.find(type) == m_Observers.end())
			return;

		auto&& observers = m_Observers.at(type);

		for (auto&& observer : observers)
			observer(postEvent);
	}
}
