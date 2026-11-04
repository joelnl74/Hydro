#pragma once

#include "Hydro.h"
#include "Data/ItemDefinition.h"
#include "Data/LootDefinition.h"
#include "Rules/LootRules.h"
#include "Scripts/Battle/Combatants.h"
#include "State/BattleState.h"
#include "State/Inventory.h"
#include "State/PartyState.h"
#include "State/QuestLog.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

// A won battle's rewards: experience (and the level ups it brings), aster and loot, given to the run's state.

namespace Hydro
{
	class BattleResults
	{
	public:
		// Each defeat also counts for the side quests asking for it (QuestRules) and drops its aster. Returns the lines the
		// victory shows, one after another.
		static std::vector<std::string> Give(const Combatants& actors, const std::function<float()>& random)
		{
			std::vector<std::string> lines;
			int experience = 0;
			int money = 0;
			for (const Combatant& enemy : actors.GetAll())
			{
				if (enemy.IsHero() || enemy.Row.empty())
				{
					continue;
				}
				experience += Combatants::EnemyOf(enemy).Experience;
				money += Combatants::EnemyOf(enemy).Money;
				QuestLog::Get().RecordDefeat(enemy.Row);
			}
			if (experience > 0)
			{
				lines.push_back("Gained " + std::to_string(experience) + " experience.");
				for (const std::string& line : PartyState::Get().GainBattleExperience(experience))
				{
					lines.push_back(line);
				}
			}
			if (money > 0)
			{
				Inventory::Get().Money += money;
				lines.push_back("Found " + std::to_string(money) + " " + CurrencyName + ".");
			}
			GiveLoot(actors, random, lines);
			return lines;
		}

	private:
		// The loot (Loot.hytable): each enemy's table, then the encounter's.
		static void GiveLoot(const Combatants& actors, const std::function<float()>& random, std::vector<std::string>& lines)
		{
			std::vector<LootRules::Drop> drops;
			int money = 0;
			auto rollTable = [&drops, &money, &random](const DataRowRef& reference)
			{
				const DataView<LootTableDefinition> table = reference.IsSet() ? Data::GetRow<LootTableDefinition>(reference)
					: DataView<LootTableDefinition>();
				if (table.IsValid() == false)
				{
					return;
				}
				money += LootRules::RollMoney(*table, random);
				for (const LootRules::Drop& drop : LootRules::Roll(*table, random))
				{
					auto found = std::find_if(drops.begin(), drops.end(),
						[&](const LootRules::Drop& other) { return other.Item == drop.Item; });
					if (found == drops.end())
					{
						drops.push_back(drop);
					}
					else
					{
						found->Count += drop.Count;
					}
				}
			};
			for (const Combatant& enemy : actors.GetAll())
			{
				if (enemy.IsHero() == false && enemy.Row.empty() == false)
				{
					rollTable(Combatants::EnemyOf(enemy).Loot);
				}
			}
			rollTable(BattleState::Get().EncounterLoot);

			std::string found;
			for (const LootRules::Drop& drop : drops)
			{
				const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(drop.Item);
				if (item.IsValid() == false)
				{
					continue;
				}
				Inventory::Get().Add(drop.Item, drop.Count);
				found += (found.empty() ? "" : ", ") + item->Name + (drop.Count > 1 ? " x" + std::to_string(drop.Count) : std::string());
			}
			if (found.empty() == false)
			{
				HY_INFO("Battle: found {}", found);
				lines.push_back("Found " + found + ".");
			}
			if (money > 0)
			{
				Inventory::Get().Money += money;
				lines.push_back("Found " + std::to_string(money) + " " + CurrencyName + " more.");
			}
		}
	};
}
