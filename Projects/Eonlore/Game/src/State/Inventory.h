#pragma once

#include "Hydro/Data/DataRowRef.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The world's money: the aster, a small copper coin pierced with an eight-pointed star, struck by the Leyline Concord.
	// The UI's word for it, until there is a text table.
	inline constexpr const char* CurrencyName = "Aster";

	// The party's items: run state on the game session, shared by every scene of a run. The field director gives the starting
	// inventory once per run.
	struct Inventory
	{
		// The item is looked up when shown or used, so an edit to the table shows at once.
		struct Slot
		{
			DataRowRef Item;
			int Count = 0;
		};

		std::vector<Slot> Slots;
		bool Started = false; // the starting inventory was given this run
		int Money = 0;        // in aster
		// Opened chests and picked-up spots as "<scene>/<tag>", so they stay opened or gone for the rest of the run.
		std::vector<std::string> Found;

		bool WasFound(const std::string& key) const { return std::find(Found.begin(), Found.end(), key) != Found.end(); }
		void MarkFound(const std::string& key)
		{
			if (WasFound(key) == false)
			{
				Found.push_back(key);
			}
		}

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

		// Its line stays at 0, so the list doesn't jump. False, taking nothing, when there aren't that many.
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

		// Returns the count now held.
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

		// Only while a game runs.
		static Inventory& Get();
	};
}
