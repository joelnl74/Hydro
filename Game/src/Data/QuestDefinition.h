// Game/src/Data/QuestDefinition.h
#pragma once

#include "Data/DialogueDefinition.h"
#include "Data/InventoryDefinition.h"
#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>
#include <vector>

// A quest's words: its title and the objective of each stage (Assets/Data/Quests.hytable). A story quest is moved on by
// its scripts (ShrineQuestScript, CountrysideQuestScript); a side quest (Kind Defeat or Collect) is entirely data: an
// NPC offers it, the run counts it (Rules/QuestRules.h) and the NPC takes it back (QuestGiverScript). QuestLog keeps
// which quests the run has, their stage and progress (game state, not data).
namespace Hydro
{
	// What a quest asks. Story: its scripts decide. Defeat: win battles against Count of Enemy. Collect: bring
	// Count of Item (from loot or gathering), handed over when it is turned in. Saved by name.
	HY_ENUM()
	enum class QuestKind : uint8_t
	{
		Story,
		Defeat,
		Collect
	};

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
		HY_PROPERTY(Tooltip("In order; the quest's script moves from one to the next (a side quest: what to do, then whom to return to)"))
		std::vector<QuestStage> Stages;

		// Side quests (Kind Defeat or Collect; a story quest leaves these empty).
		HY_PROPERTY(Tooltip("Story: its scripts move it on; Defeat / Collect: a side quest an NPC offers (QuestGiverScript)"))
		QuestKind Kind = QuestKind::Story;
		HY_PROPERTY(Tooltip("Side quests: who offers it and takes it back: the name after \"Npc \" in their tag"))
		std::string Giver;
		HY_PROPERTY(DataRow(EnemyDefinition), Tooltip("Defeat: the enemy whose defeats count"))
		DataRowRef Enemy;
		HY_PROPERTY(DataRow(ItemDefinition), Tooltip("Collect: the item to bring; handed over when the quest is turned in"))
		DataRowRef Item;
		HY_PROPERTY(Range(1, 99), Tooltip("Defeat / Collect: how many"))
		int Count = 1;
		HY_PROPERTY(Tooltip("Given when it is turned in"))
		std::vector<InventoryEntry> RewardItems;
		HY_PROPERTY(Range(0, 9999), Tooltip("Experience for every party member standing, when it is turned in"))
		int RewardExperience = 0;
		HY_PROPERTY(Range(0, 99999), Tooltip("Aster (the world's money) given when it is turned in"))
		int RewardMoney = 0;
		HY_PROPERTY(Tooltip("What the giver says offering it; the quest starts when the box closes"))
		std::vector<DialogueLine> OfferLines;
		HY_PROPERTY(Tooltip("What the giver says while it isn't done; {Left} is how many are still to go"))
		std::vector<DialogueLine> ProgressLines;
		HY_PROPERTY(Tooltip("What the giver says when it is turned in (the rewards are listed after)"))
		std::vector<DialogueLine> DoneLines;
	};
}
