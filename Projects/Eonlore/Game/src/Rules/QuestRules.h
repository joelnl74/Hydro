#pragma once

#include "Data/QuestDefinition.h"
#include "Hydro/Data/DataRowRef.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// Side quest counting (Defeat / Collect) on the quest log's entries, with no scene or data files (tested in
	// Tools/DataTests/GameRulesTest.cpp). Stage 0 is doing it; reaching Goal moves it to stage 1, back to the giver.
	class QuestRules
	{
	public:
		// Counts for every unfinished Defeat quest on `enemyRow`. Returns whether any quest changed.
		template<typename Quest, typename Lookup>
		static bool RecordDefeat(std::vector<Quest>& quests, const std::string& enemyRow, Lookup lookup)
		{
			bool changed = false;
			for (Quest& quest : quests)
			{
				const QuestDefinition* definition = quest.Done ? nullptr : lookup(quest.Definition);
				if (definition == nullptr || definition->Kind != QuestKind::Defeat || definition->Enemy.Row != enemyRow
					|| quest.Progress >= quest.Goal)
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

		// Progress is how many the party holds, at most Goal: enough moves the quest to stage 1, too few back to stage 0.
		// Returns whether any quest changed.
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

		template<typename Quest>
		static bool IsReady(const Quest& quest)
		{
			return quest.Done == false && quest.Goal > 0 && quest.Progress >= quest.Goal;
		}

		// The first of the giver's quests (in `order`) that isn't finished, or "" when every one is. `find` returns the log's
		// entry, or nullptr when the run doesn't have it yet.
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
