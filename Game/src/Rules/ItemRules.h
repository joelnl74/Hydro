// Game/src/Rules/ItemRules.h
#pragma once

#include "Data/ItemDefinition.h"
#include "State/BattleState.h"
#include "State/Inventory.h"

namespace Hydro
{
	// Using items (Scripting plan SC8f): plain rules on the run's state, no UI.
	class ItemRules
	{
	public:
		enum class UseResult : uint8_t { Used, NotUsableHere, NoneLeft };

		// Uses one of the inventory's `slot` in the field: restores every hero's MP, and HP of those standing.
		static UseResult UseInField(Inventory::Slot& slot, const ItemDefinition& item, BattleState& party)
		{
			if (item.CanUseInField() == false)
			{
				return UseResult::NotUsableHere;
			}
			if (slot.Count <= 0)
			{
				return UseResult::NoneLeft;
			}
			slot.Count -= 1;
			for (int h = 0; h < HeroCount; h++)
			{
				party.HeroMP[h] = std::min(GetHero(h).MaxMP, party.HeroMP[h] + item.RestoreMP);
				if (party.HeroHP[h] > 0)
				{
					party.HeroHP[h] = std::min(GetHero(h).MaxHP, party.HeroHP[h] + item.RestoreHP);
				}
			}
			return UseResult::Used;
		}
	};
}
