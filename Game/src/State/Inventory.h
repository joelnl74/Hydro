// Game/src/State/Inventory.h
#pragma once

#include "Hydro/Data/DataRowRef.h"

#include <vector>

namespace Hydro
{
	// The party's items: run state, on the game session (RpgSession::Items, Scripting plan SC2), so every field scene
	// and interior of a run shares them and a new run starts with none. The field director fills it from its
	// starting inventory once per run (FieldDirectorScript).
	struct Inventory
	{
		// One line: which item (a row of the items table, looked up when shown or used, so an edit to the table shows
		// at once) and how many. State, not data.
		struct Slot
		{
			DataRowRef Item;
			int Count = 0;
		};

		std::vector<Slot> Slots;
		bool Started = false; // the starting inventory was given this run

		// How many of an item the party holds (0 when none).
		int CountOf(const DataRowRef& item) const
		{
			for (const Slot& slot : Slots)
			{
				if (slot.Item == item)
				{
					return slot.Count;
				}
			}
			return 0;
		}

		// Takes `count` of an item away (its line stays, at 0, so the list doesn't jump). False, taking nothing,
		// when there aren't that many.
		bool Remove(const DataRowRef& item, int count = 1)
		{
			for (Slot& slot : Slots)
			{
				if (slot.Item == item && slot.Count >= count)
				{
					slot.Count -= count;
					return true;
				}
			}
			return false;
		}

		// Adds `count` of an item: to its line, or a new line at the end. Returns the count now held.
		int Add(const DataRowRef& item, int count = 1)
		{
			for (Slot& slot : Slots)
			{
				if (slot.Item == item)
				{
					slot.Count += count;
					return slot.Count;
				}
			}
			Slots.push_back(Slot{ item, count });
			return count;
		}

		// The running session's. Only while a game runs.
		static Inventory& Get();
	};
}
