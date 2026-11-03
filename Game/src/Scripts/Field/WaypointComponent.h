#pragma once

#include "Hydro/Scene/EntityRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <string>

namespace Hydro
{
	// One stop of a townsperson's route: where they walk to and what they do there (wait, face a way or an entity, play an
	// Action clip). StopChance decides whether they stop at all.
	HY_COMPONENT()
	struct WaypointComponent
	{
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
