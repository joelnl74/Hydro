// Game/src/Data/CombatStats.h
#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

namespace Hydro
{
	// A combatant's numbers. A hero's grow with its level (HeroDefinition: Base + GrowthPerLevel per level above 1).
	HY_DATA()
	struct CombatStats
	{
		HY_PROPERTY(Range(0, 9999))
		int MaxHP = 10;
		HY_PROPERTY(Range(0, 999))
		int MaxMP = 0;
		HY_PROPERTY(Range(0, 999))
		int Attack = 5;
		HY_PROPERTY(Range(0, 999))
		int Magic = 0;
		HY_PROPERTY(Range(0, 99), Tooltip("Who acts first: higher goes earlier"))
		int Speed = 5;
	};
}
