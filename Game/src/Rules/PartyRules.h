// Game/src/Rules/PartyRules.h
#pragma once

#include "Data/CombatStats.h"
#include "Data/ItemDefinition.h"
#include "Hydro/Data/DataRowRef.h"

#include <algorithm>
#include <array>
#include <vector>

namespace Hydro
{
	// Where each equipment slot of a hero is (BattleState::HeroEquipment), the Menu plan's six.
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

	// The party's rules for equipment and ability slots (the Menu plan's M1): plain functions on the run's state and
	// the items' data, no UI. The state is passed in pieces (a hero's slots, the inventory's add / remove) so tests
	// need no session.
	class PartyRules
	{
	public:
		static CombatStats Add(const CombatStats& a, const CombatStats& b)
		{
			return CombatStats{ a.MaxHP + b.MaxHP, a.MaxMP + b.MaxMP, a.Attack + b.Attack, a.Magic + b.Magic, a.Speed + b.Speed };
		}

		// Whether an item of `slot` goes in `position` (an accessory in either accessory position).
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

		// Puts `item` (one taken from the inventory: `take` returns false when there is none) in `position`; what
		// was there goes back to the inventory (`give`). False, changing nothing, when it doesn't fit or none is held.
		template<size_t N, typename Take, typename Give>
		static bool Equip(std::array<DataRowRef, N>& equipment, EquipPosition position, const DataRowRef& item, EquipSlot itemSlot, Take take, Give give)
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

		// Takes off what is in `position`, back to the inventory.
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

		// Puts a known ability in a battle slot; if it already sits in another slot, the two swap. False when the
		// hero doesn't know it. An unset `ability` empties the slot.
		template<size_t N>
		static bool SetAbilitySlot(std::array<DataRowRef, N>& slots, const std::vector<DataRowRef>& known, size_t slot, const DataRowRef& ability)
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
