// Game/src/Scripts/Field/WaypointScript.h
#pragma once

#include "Hydro.h"

#include <string>

namespace Hydro
{
	// One stop of a townsperson's route (NpcRouteScript): put it on each child of the route's entity, in the order
	// they are walked (the hierarchy's order). Its position is where they walk to; the rest says what they do there:
	// stand for WaitMin to WaitMax seconds (with StopChance; otherwise they walk straight on), facing Facing or the
	// LookAt entity, playing Action<Facing> ("Work": the monk's broom) or Idle<Facing>.
	// Data only: until the game can register its own components (the planned component registry), a waypoint's
	// settings live on this script, which does nothing at runtime.
	HY_SCRIPT()
	class WaypointScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 120), Tooltip("Seconds it stands here, at least (0 and WaitMax 0: walks straight on)"))
		float WaitMin = 0.0f;
		HY_PROPERTY(Range(0, 120), Tooltip("Seconds it stands here, at most"))
		float WaitMax = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Chance it stops here at all on a pass (1: every time)"))
		float StopChance = 1.0f;
		HY_PROPERTY(Tooltip("While standing: Up (away from the camera), Down, Left or Right; empty: the way it arrived"))
		std::string Facing;
		HY_PROPERTY(Tooltip("While standing: face this entity instead (a shop's merchant, a noticeboard)"))
		EntityRef LookAt;
		HY_PROPERTY(Tooltip("The clip prefix played while standing (\"Work\" plays Work<Facing>); empty: Idle<Facing>"))
		std::string Action;
	};
}
