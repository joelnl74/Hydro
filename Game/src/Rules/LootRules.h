// Game/src/Rules/LootRules.h
#pragma once

#include "Data/LootDefinition.h"
#include "Hydro/Data/Data.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace Hydro
{
	// What a loot table gives: plain rules on data, no scene (the Scripting plan's Rules/ layer). `random` returns 0
	// to 1 (the battle's own generator, so a test can pass a fixed sequence).
	class LootRules
	{
	public:
		struct Drop
		{
			DataRowRef Item;
			int Count = 0;
		};

		// An item's rarity (the game asks the items table; a test answers itself).
		using RarityLookup = std::function<ItemRarity(const DataRowRef&)>;

		// Rolls the table Rolls times; drops of the same item are added together, in the order first found.
		static std::vector<Drop> Roll(const LootTableDefinition& table, const std::function<float()>& random)
		{
			return Roll(table, random, &RarityFromData);
		}

		static std::vector<Drop> Roll(const LootTableDefinition& table, const std::function<float()>& random, const RarityLookup& rarityOf)
		{
			std::vector<Drop> drops;
			for (int roll = 0; roll < table.Rolls; roll++)
			{
				if (random() * 100.0f >= table.DropChance)
				{
					continue;
				}
				const LootEntry* entry = PickEntry(table, random, rarityOf);
				if (entry == nullptr || entry->Item.IsSet() == false)
				{
					continue;
				}
				const int low = std::max(1, entry->MinCount);
				const int high = std::max(low, entry->MaxCount);
				const int count = std::min(high, low + (int)(random() * (float)(high - low + 1)));
				auto found = std::find_if(drops.begin(), drops.end(), [&](const Drop& drop) { return drop.Item == entry->Item; });
				if (found == drops.end())
				{
					drops.push_back(Drop{ entry->Item, count });
				}
				else
				{
					found->Count += count;
				}
			}
			return drops;
		}

	private:
		static ItemRarity RarityFromData(const DataRowRef& itemRef)
		{
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(itemRef);
			return item.IsValid() ? item->Rarity : ItemRarity::Common;
		}

		// A rarity by its share among the table's rarities that have items, then one of its items; without
		// rarities, any item.
		static const LootEntry* PickEntry(const LootTableDefinition& table, const std::function<float()>& random, const RarityLookup& rarityOf)
		{
			if (table.Items.empty())
			{
				return nullptr;
			}
			std::vector<const RarityChance*> present;
			float total = 0.0f;
			for (const RarityChance& chance : table.Rarities)
			{
				const bool hasItems = std::any_of(table.Items.begin(), table.Items.end(), [&](const LootEntry& entry) { return rarityOf(entry.Item) == chance.Rarity; });
				if (hasItems && chance.Percent > 0.0f)
				{
					present.push_back(&chance);
					total += chance.Percent;
				}
			}
			std::vector<const LootEntry*> candidates;
			if (present.empty())
			{
				for (const LootEntry& entry : table.Items)
				{
					candidates.push_back(&entry);
				}
			}
			else
			{
				float pick = random() * total;
				ItemRarity rarity = present.back()->Rarity;
				for (const RarityChance* chance : present)
				{
					if (pick < chance->Percent)
					{
						rarity = chance->Rarity;
						break;
					}
					pick -= chance->Percent;
				}
				for (const LootEntry& entry : table.Items)
				{
					if (rarityOf(entry.Item) == rarity)
					{
						candidates.push_back(&entry);
					}
				}
			}
			if (candidates.empty())
			{
				return nullptr;
			}
			const size_t index = std::min(candidates.size() - 1, (size_t)(random() * (float)candidates.size()));
			return candidates[index];
		}
	};
}
