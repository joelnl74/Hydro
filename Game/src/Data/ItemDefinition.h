// Game/src/Data/ItemDefinition.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>

// The game's items as data (the Definitions plan's D4, #171): one row per item in Assets/Data/Items.hytable,
// edited in a text editor until the inspector can (D3). The party's inventory stores rows (DataRowRef) and counts.
namespace Hydro
{
	// What kind of thing an item is (the inventory can sort or filter by it later). Saved by name.
	HY_ENUM()
	enum class ItemKind : uint8_t
	{
		Consumable,
		Equipment,
		KeyItem,
		Material
	};

	// How rare an item is: loot tables (LootTableDefinition) pick a rarity by its chance, then an item of it.
	HY_ENUM()
	enum class ItemRarity : uint8_t
	{
		Common,
		Uncommon,
		Rare,
		Epic,
		Legendary
	};

	HY_DATA(Table, DisplayField(Name))
	struct ItemDefinition
	{
		HY_PROPERTY(Tooltip("Shown in the inventory"))
		std::string Name;
		HY_PROPERTY(Tooltip("The inventory's detail text"))
		std::string Description;
		HY_PROPERTY()
		ItemKind Kind = ItemKind::Consumable;
		HY_PROPERTY(Tooltip("How likely a loot table is to give it (the table's chance for this rarity)"))
		ItemRarity Rarity = ItemRarity::Common;
		HY_PROPERTY(Asset(Texture2D), Tooltip("Optional; the inventory has no icons yet"))
		AssetHandle Icon = 0;
		HY_PROPERTY(Range(0, 999), Tooltip("Restored to every hero when used"))
		int RestoreHP = 0;
		HY_PROPERTY(Range(0, 999), Tooltip("Restored to every hero when used"))
		int RestoreMP = 0;
		HY_PROPERTY(Tooltip("Can be used from the field inventory"))
		bool UsableInField = false;

		// Whether using it from the field inventory does anything.
		bool CanUseInField() const
		{
			return UsableInField && (RestoreHP > 0 || RestoreMP > 0);
		}
	};
}
