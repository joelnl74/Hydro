// Game/src/Rules/QuestRules.h
#pragma once

#include "Data/QuestDefinition.h"
#include "Hydro/Data/DataRowRef.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// Side quests' counting (QuestDefinition Kind Defeat / Collect): plain rules on the quest log's entries, no scene,
	// no data files (the Scripting plan's Rules/ layer; tested in Tools/DataTests/GameRulesTest.cpp). A quest entry is
	// anything with Definition, Stage, Progress, Goal and Done (QuestLog::Quest); `lookup` finds its definition (the
	// game asks the quests table, a test answers itself). A side quest has two stages: do it (stage 0), then go back
	// to its giver (stage 1), which is where the counting moves it once Progress reaches Goal.
	class QuestRules
	{
	public:
		// A defeated enemy (its row of Enemies.hytable) counts for every unfinished Defeat quest on it. Returns
		// whether any quest changed.
		template<typename Quest, typename Lookup>
		static bool RecordDefeat(std::vector<Quest>& quests, const std::string& enemyRow, Lookup lookup)
		{
			bool changed = false;
			for (Quest& quest : quests)
			{
				const QuestDefinition* definition = quest.Done ? nullptr : lookup(quest.Definition);
				if (definition == nullptr || definition->Kind != QuestKind::Defeat || definition->Enemy.Row != enemyRow || quest.Progress >= quest.Goal)
				{
					continue;
				}
				quest.Progress++;
				changed = true;
				if (quest.Progress >= quest.Goal)
				{
					quest.Stage = 1;
				}
			}
			return changed;
		}

		// Collect quests: their progress is how many the party holds (`countOf` an item row), at most Goal; enough
		// moves the quest to its second stage, too few (an item used up meanwhile) back to its first. Returns whether
		// any quest changed.
		template<typename Quest, typename Lookup, typename CountOf>
		static bool RefreshCollect(std::vector<Quest>& quests, Lookup lookup, CountOf countOf)
		{
			bool changed = false;
			for (Quest& quest : quests)
			{
				const QuestDefinition* definition = quest.Done ? nullptr : lookup(quest.Definition);
				if (definition == nullptr || definition->Kind != QuestKind::Collect)
				{
					continue;
				}
				const int held = std::min(quest.Goal, std::max(0, countOf(definition->Item)));
				const int stage = held >= quest.Goal ? 1 : 0;
				if (held != quest.Progress || stage != quest.Stage)
				{
					quest.Progress = held;
					quest.Stage = stage;
					changed = true;
				}
			}
			return changed;
		}

		// Whether a side quest can be turned in.
		template<typename Quest>
		static bool IsReady(const Quest& quest)
		{
			return quest.Done == false && quest.Goal > 0 && quest.Progress >= quest.Goal;
		}

		// The quest a giver has for the party now: the first of theirs (in `order`) that isn't finished, or "" when
		// every one is. `find` returns the log's entry for a definition, or nullptr when the run doesn't have it yet.
		template<typename Find>
		static std::string CurrentFor(const std::vector<std::string>& order, Find find)
		{
			for (const std::string& row : order)
			{
				const auto* quest = find(row);
				if (quest == nullptr || quest->Done == false)
				{
					return row;
				}
			}
			return std::string();
		}
	};
}
