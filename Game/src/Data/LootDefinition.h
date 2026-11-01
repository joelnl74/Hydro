// Game/src/Data/LootDefinition.h
#pragma once

#include "Data/ItemDefinition.h"
#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <vector>

// What a won battle gives (Assets/Data/Loot.hytable): an enemy and an encounter name a loot table
// (EnemyDefinition::Loot, EncounterDefinition::Loot). A roll first decides whether anything drops (DropChance), then
// picks a rarity by its chance among the table's rarities that have items, then one of those items (their rarity is
// the item's, ItemDefinition::Rarity) and a count. Rules/LootRules.h rolls it.
namespace Hydro
{
	// A rarity's share of a table's drops, in percent (the shares of the rarities present are compared, so they
	// needn't add up to 100).
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
	};
}
