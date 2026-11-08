#pragma once

#include "Data/CombatStats.h"
#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>

// The game's items as data: one row per item in Assets/Data/Items.hytable. The inventory stores rows and counts.
namespace Hydro
{
	// Saved by name.
	HY_ENUM()
	enum class ItemKind : uint8_t
	{
		Consumable,
		Equipment,
		KeyItem,
		Material
	};

	// Accessory fits either accessory position; Off-hand is a shield, a tome or a book. None: not equipment.
	HY_ENUM()
	enum class EquipSlot : uint8_t
	{
		None,
		Weapon,
		OffHand,
		Head,
		Body,
		Accessory
	};

	// Loot tables pick a rarity by its chance, then an item of it.
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
		HY_PROPERTY(Tooltip("Equipment: the slot it goes in (None: not equipment)"))
		EquipSlot Slot = EquipSlot::None;
		HY_PROPERTY(Tooltip("Equipment: added to the wearer's numbers"))
		CombatStats Bonus{ 0, 0, 0, 0, 0 };

		bool CanUseInField() const
		{
			return UsableInField && (RestoreHP > 0 || RestoreMP > 0);
		}
	};
}
