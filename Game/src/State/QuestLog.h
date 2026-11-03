#pragma once

#include "Hydro.h"
#include "Data/QuestDefinition.h"
#include "Rules/QuestRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace Hydro
{
	struct QuestLog
	{
		// A new run starts with an empty log.
		static QuestLog& Get();

		struct Quest
		{
			DataRowRef Definition; // its row of the quests table
			int Stage = 0;
			int Progress = 0;
			int Goal = 0;          // 0: the objective has no count
			bool Done = false;

			// From the data every time, so a table edit shows at once; the row ID when the row is gone.
			std::string GetTitle() const
			{
				const DataView<QuestDefinition> quest = Data::GetRow<QuestDefinition>(Definition);
				return quest.IsValid() ? quest->Title : Definition.Row;
			}

			std::string GetObjective() const
			{
				const DataView<QuestDefinition> quest = Data::GetRow<QuestDefinition>(Definition);
				return quest.IsValid() && Stage >= 0 && Stage < (int)quest->Stages.size() ? quest->Stages[(size_t)Stage].Objective
					: std::string();
			}
		};

		struct Announcement
		{
			std::string Header; // "New Quest", "Quest Complete"
			std::string Title;
		};

		std::vector<Quest> Quests;
		std::vector<Announcement> Announcements; // oldest first; QuestTrackerScript shows and removes them
		uint32_t Revision = 0;                   // bumped by every change (the tracker flashes)

		Quest* Find(const DataRowRef& definition)
		{
			for (Quest& quest : Quests)
			{
				if (quest.Definition == definition)
				{
					return &quest;
				}
			}
			return nullptr;
		}

		// Adds the quest at its first stage (or restarts it) and announces it.
		void Start(const DataRowRef& definition, int goal = 0)
		{
			Quest* quest = Find(definition);
			if (quest == nullptr)
			{
				Quests.push_back(Quest());
				quest = &Quests.back();
			}
			quest->Definition = definition;
			quest->Stage = 0;
			quest->Progress = 0;
			quest->Goal = goal;
			quest->Done = false;
			Announcements.push_back({ "New Quest", quest->GetTitle() });
			Revision++;
			HY_INFO("Quest: started '{}': {}", quest->GetTitle(), quest->GetObjective());
		}

		void SetStage(const DataRowRef& definition, int stage, int goal = 0)
		{
			if (Quest* quest = Find(definition))
			{
				quest->Stage = stage;
				quest->Progress = 0;
				quest->Goal = goal;
				Revision++;
				HY_INFO("Quest: '{}': {}", quest->GetTitle(), quest->GetObjective());
			}
		}

		void SetProgress(const DataRowRef& definition, int progress)
		{
			Quest* quest = Find(definition);
			if (quest != nullptr && quest->Progress != progress)
			{
				quest->Progress = progress;
				Revision++;
				HY_INFO("Quest: '{}': {} / {}", quest->GetTitle(), progress, quest->Goal);
			}
		}

		void Complete(const DataRowRef& definition)
		{
			Quest* quest = Find(definition);
			if (quest != nullptr && quest->Done == false)
			{
				quest->Done = true;
				Announcements.push_back({ "Quest Complete", quest->GetTitle() });
				Revision++;
				HY_INFO("Quest: completed '{}'", quest->GetTitle());
			}
		}

		void Remove(const DataRowRef& definition)
		{
			Quests.erase(std::remove_if(Quests.begin(), Quests.end(), [&](const Quest& quest) { return quest.Definition == definition; }),
				Quests.end());
			Revision++;
		}

		void RecordDefeat(const std::string& enemyRow)
		{
			if (QuestRules::RecordDefeat(Quests, enemyRow, &FindDefinition))
			{
				Revision++;
				HY_INFO("Quest: a {} defeat counted", enemyRow);
			}
		}

		// `countOf` is the inventory's CountOf.
		template<typename CountOf>
		void RefreshCollect(CountOf countOf)
		{
			if (QuestRules::RefreshCollect(Quests, &FindDefinition, countOf))
			{
				Revision++;
			}
		}

		// The view keeps the table loaded for the frame; nullptr when the row is gone.
		static const QuestDefinition* FindDefinition(const DataRowRef& definition)
		{
			const DataTableView<QuestDefinition> table = Data::GetTable<QuestDefinition>(definition.Table);
			return table.IsValid() ? table.FindRow(definition.Row) : nullptr;
		}

		// The newest quest that isn't done; nullptr when there is none.
		const Quest* GetTracked()
		{
			for (auto it = Quests.rbegin(); it != Quests.rend(); ++it)
			{
				if (it->Done == false)
				{
					return &*it;
				}
			}
			return nullptr;
		}

		// NPCs a quest speaks for (by the name after "Npc "). Talk returns true when the quest takes the conversation, so the
		// NPC's usual lines are not shown.
		void Claim(const std::string& npc)
		{
			if (std::find(m_Claimed.begin(), m_Claimed.end(), npc) == m_Claimed.end())
			{
				m_Claimed.push_back(npc);
			}
		}

		void Release(const std::string& npc)
		{
			m_Claimed.erase(std::remove(m_Claimed.begin(), m_Claimed.end(), npc), m_Claimed.end());
		}

		bool Talk(const std::string& npc)
		{
			if (std::find(m_Claimed.begin(), m_Claimed.end(), npc) == m_Claimed.end())
			{
				return false;
			}
			m_Talked = npc;
			return true;
		}

		// True once after the player talked to `npc`.
		bool TakeTalk(const std::string& npc)
		{
			if (m_Talked != npc)
			{
				return false;
			}
			m_Talked.clear();
			return true;
		}

	private:
		std::vector<std::string> m_Claimed;
		std::string m_Talked;
	};
}
