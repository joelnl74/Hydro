#pragma once
#include <functional>
#include <Map>

#include "Hydro/Core/Base.h"
#include "Event.h"

namespace Hydro
{
	class EventDispatcher
	{
	public:
		~EventDispatcher() = default;

		using SlotType = std::function< void(Event&) >;

		void EventDispatcher::Subscribe(EventType eventType, SlotType&& slot)
		{
			m_Observers[eventType].push_back(slot);
		}

		void Post(Event& postEvent) const
		{
			auto type = postEvent.GetEventType();

			// Ignore events for which we do not have an observer.
			if (m_Observers.find(type) == m_Observers.end())
				return;

			auto&& observers = m_Observers.at(type);

			for (auto&& observer : observers)
				observer(postEvent);
		}



	public:
		static EventDispatcher& Get() { return *s_Instance; }
		static void Create();
		static EventDispatcher *s_Instance;

		// [TODO] Should probally do this once and then send derrived class to all observers.
		template<typename T>
		static T GetEvent(Event &e)
		{
			return static_cast<T&>(e);
		}

	private:
		std::map<EventType, std::vector<SlotType>> m_Observers;
	};
}

