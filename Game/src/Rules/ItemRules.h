#pragma once

#include "Data/ItemDefinition.h"
#include "State/PartyState.h"
#include "State/Inventory.h"

namespace Hydro
{
	// Using items: plain rules on the run's state, no UI.
	class ItemRules
	{
	public:
		enum class UseResult : uint8_t { Used, NotUsableHere, NoneLeft };

		// Restores every party member's MP, and the HP of those standing.
		static UseResult UseInField(Inventory::Slot& slot, const ItemDefinition& item, PartyState& party)
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
			for (PartyMember& member : party.Members)
			{
				const HeroStats stats = member.GetStats();
				member.MP = std::min(stats.MaxMP, member.MP + item.RestoreMP);
				if (member.IsStanding())
				{
					member.HP = std::min(stats.MaxHP, member.HP + item.RestoreHP);
				}
			}
			return UseResult::Used;
		}
	};
}
