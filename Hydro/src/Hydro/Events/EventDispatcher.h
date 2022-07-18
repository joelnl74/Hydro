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

		using SlotType = std::function< void(const Event&) >;
		void Subscribe(EventType eventType, SlotType&& slot);
		void Post(const Event& postEvent) const;

	public:
		static EventDispatcher& Get() { return *s_Instance; }
		static void Create();
		static EventDispatcher *s_Instance;

	private:
		std::map<EventType, std::vector<SlotType>> m_Observers;
	};
}

