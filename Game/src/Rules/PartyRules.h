#pragma once

#include "Data/CombatStats.h"
#include "Data/ItemDefinition.h"
#include "Hydro/Data/DataRowRef.h"

#include <algorithm>
#include <array>
#include <vector>

namespace Hydro
{
	// Each hero's six equipment positions (PartyMember::Equipment).
	enum class EquipPosition : uint8_t
	{
		Weapon = 0,
		OffHand,
		Head,
		Body,
		Accessory1,
		Accessory2,
		Count
	};

	// The party's rules for equipment and ability slots on the run's state and the items' data. State is passed in pieces
	// so tests need no session.
	class PartyRules
	{
	public:
		static CombatStats Add(const CombatStats& a, const CombatStats& b)
		{
			return CombatStats{ a.MaxHP + b.MaxHP, a.MaxMP + b.MaxMP, a.Attack + b.Attack, a.Magic + b.Magic, a.Speed + b.Speed };
		}

		// An accessory fits either accessory position.
		static bool Fits(EquipSlot slot, EquipPosition position)
		{
			switch (slot)
			{
				case EquipSlot::Weapon: return position == EquipPosition::Weapon;
				case EquipSlot::OffHand: return position == EquipPosition::OffHand;
				case EquipSlot::Head: return position == EquipPosition::Head;
				case EquipSlot::Body: return position == EquipPosition::Body;
				case EquipSlot::Accessory: return position == EquipPosition::Accessory1 || position == EquipPosition::Accessory2;
				default: return false;
			}
		}

		static const char* GetPositionName(EquipPosition position)
		{
			switch (position)
			{
				case EquipPosition::Weapon: return "Weapon";
				case EquipPosition::OffHand: return "Off-hand";
				case EquipPosition::Head: return "Head";
				case EquipPosition::Body: return "Body";
				case EquipPosition::Accessory1: return "Accessory";
				case EquipPosition::Accessory2: return "Accessory";
				default: return "";
			}
		}

		// `take` takes one from the inventory, `give` returns what was worn. False, changing nothing, when it doesn't fit or
		// none is held.
		template<size_t N, typename Take, typename Give>
		static bool Equip(std::array<DataRowRef, N>& equipment, EquipPosition position, const DataRowRef& item, EquipSlot itemSlot,
			Take take, Give give)
		{
			const size_t index = (size_t)position;
			if (index >= N || item.IsSet() == false || Fits(itemSlot, position) == false)
			{
				return false;
			}
			if (equipment[index] == item)
			{
				return true;
			}
			if (take(item) == false)
			{
				return false;
			}
			if (equipment[index].IsSet())
			{
				give(equipment[index]);
			}
			equipment[index] = item;
			return true;
		}

		// Back to the inventory.
		template<size_t N, typename Give>
		static void Unequip(std::array<DataRowRef, N>& equipment, EquipPosition position, Give give)
		{
			const size_t index = (size_t)position;
			if (index < N && equipment[index].IsSet())
			{
				give(equipment[index]);
				equipment[index] = DataRowRef();
			}
		}

		// If it already sits in another slot, the two swap. False when the hero doesn't know it; an unset `ability` empties
		// the slot.
		template<size_t N>
		static bool SetAbilitySlot(std::array<DataRowRef, N>& slots, const std::vector<DataRowRef>& known, size_t slot,
			const DataRowRef& ability)
		{
			if (slot >= N)
			{
				return false;
			}
			if (ability.IsSet() == false)
			{
				slots[slot] = DataRowRef();
				return true;
			}
			if (std::find(known.begin(), known.end(), ability) == known.end())
			{
				return false;
			}
			for (size_t other = 0; other < N; other++)
			{
				if (other != slot && slots[other] == ability)
				{
					slots[other] = slots[slot];
				}
			}
			slots[slot] = ability;
			return true;
		}
	};
}
