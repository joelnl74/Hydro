// Game/src/State/Inventory.h
#pragma once

#include "Hydro/Data/DataRowRef.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The world's money (lore, Nov 2, 2026): the aster, a small copper coin pierced with an eight-pointed star, struck
	// by the Leyline Concord, the old compact of shrines and guild cities that keeps the roads between the eastern
	// provinces and the western kingdoms. It passes everywhere: easterners also call it "sen", westerners "star".
	// The UI's word for it (until there is a text table, like the other UI words).
	inline constexpr const char* CurrencyName = "Aster";

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
		int Money = 0;        // in aster (CurrencyName)
		// What the party has found for good this run: opened chests and picked-up spots, as "<scene>/<tag>", so they
		// stay opened or gone when the player comes back (FieldDirectorScript).
		std::vector<std::string> Found;

		bool WasFound(const std::string& key) const { return std::find(Found.begin(), Found.end(), key) != Found.end(); }
		void MarkFound(const std::string& key)
		{
			if (WasFound(key) == false)
			{
				Found.push_back(key);
			}
		}

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
