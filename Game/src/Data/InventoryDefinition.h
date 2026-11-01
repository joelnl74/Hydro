// Game/src/Data/InventoryDefinition.h
#pragma once

#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <vector>

// A list of items with counts, in the order they are listed: the party's starting items
// (Assets/Data/StartingInventory.hydef); later also loot and shop stock. Data, not game state: what the party
// carries during a run lives in FieldDirectorScript (and, with J5, the save's GameState).
namespace Hydro
{
	HY_DATA()
	struct InventoryEntry
	{
		HY_PROPERTY(DataRow(ItemDefinition))
		DataRowRef Item;
		HY_PROPERTY(Range(0, 99))
		int Count = 1;
	};

	HY_DATA(Definition)
	struct InventoryDefinition
	{
		HY_PROPERTY(Tooltip("In the order the inventory lists them"))
		std::vector<InventoryEntry> Items;
		HY_PROPERTY(Range(0, 999999), Tooltip("Aster the party starts with"))
		int Money = 0;
	};
}
