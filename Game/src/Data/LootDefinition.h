#pragma once

#include "Data/ItemDefinition.h"
#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <vector>

// What a won battle or an opened chest gives: whether anything drops, then a rarity by its chance, then one of its items
// and a count. Rules/LootRules.h rolls it.
namespace Hydro
{
	// A rarity's share of a table's drops; the present rarities' shares are compared, so they needn't add up to 100.
	HY_DATA()
	struct RarityChance
	{
		HY_PROPERTY()
		ItemRarity Rarity = ItemRarity::Common;
		HY_PROPERTY(Range(0, 100))
		float Percent = 100.0f;
	};

	HY_DATA()
	struct LootEntry
	{
		HY_PROPERTY(DataRow(ItemDefinition))
		DataRowRef Item;
		HY_PROPERTY(Range(1, 99))
		int MinCount = 1;
		HY_PROPERTY(Range(1, 99))
		int MaxCount = 1;
	};

	HY_DATA(Table)
	struct LootTableDefinition
	{
		HY_PROPERTY(Range(0, 100), Tooltip("Percent chance that a roll gives anything at all"))
		float DropChance = 100.0f;
		HY_PROPERTY(Range(0, 10), Tooltip("How many times the table is rolled"))
		int Rolls = 1;
		HY_PROPERTY(Tooltip("Each rarity's share; empty: every item is equally likely"))
		std::vector<RarityChance> Rarities;
		HY_PROPERTY(Tooltip("What it can give; an item's rarity is its own (Items.hytable)"))
		std::vector<LootEntry> Items;
		HY_PROPERTY(Range(0, 99999), Tooltip("Aster it also gives, at least (a chest's coins; added to a battle's)"))
		int MinMoney = 0;
		HY_PROPERTY(Range(0, 99999), Tooltip("Aster it also gives, at most"))
		int MaxMoney = 0;
	};
}
