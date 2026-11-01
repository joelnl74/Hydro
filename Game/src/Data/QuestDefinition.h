// Game/src/Data/QuestDefinition.h
#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <string>
#include <vector>

// A quest's words: its title and the objective of each stage (Assets/Data/Quests.hytable). What starts a quest,
// moves it on and finishes it is still the quest's script (ShrineQuestScript); QuestLog keeps which quests the run
// has, their stage and progress (game state, not data).
namespace Hydro
{
	HY_DATA()
	struct QuestStage
	{
		HY_PROPERTY(Tooltip("What the tracker says to do"))
		std::string Objective;
	};

	HY_DATA(Table, DisplayField(Title))
	struct QuestDefinition
	{
		HY_PROPERTY(Tooltip("Shown in the tracker and the New Quest / Quest Complete banners"))
		std::string Title;
		HY_PROPERTY(Tooltip("In order; the quest's script moves from one to the next"))
		std::vector<QuestStage> Stages;
	};
}
