// Game/src/Scripts/Field/AbilitiesView.h
#pragma once

#include "Hydro.h"
#include "Rules/PartyRules.h"
#include "State/BattleState.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The field menu's Abilities page (the Menu plan's M5; "Page Abilities" in "Field Menu Canvas"; built by
	// scripts/ui/build_game_ui.py). Owned by FieldMenuView. The battle's 8 ability slots on the left ("Ability Slot 0"
	// to "7"), the abilities the hero knows on the right ("Known Ability 0" to "9"; row 0 empties a slot), the
	// focused ability's details below. Enter on a slot moves to the known list; Enter on a known ability puts it in
	// the slot last focused (PartyRules::SetAbilitySlot: if it sits in another slot, the two swap) and goes back to
	// the slot. The battle's Abilities sub-menu lists the slots in this order.
	class AbilitiesView
	{
	public:
		static constexpr int SlotRows = BattleState::AbilitySlotCount;
		static constexpr int KnownRows = 10; // row 0 empties the slot, then up to 9 known abilities

		enum class Request : uint8_t { None, Back };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Abilities page";
			for (int i = 0; i < SlotRows; i++)
			{
				m_Slots[i] = ViewHelpers::Find(scene, "Ability Slot " + std::to_string(i), owner);
				m_SlotNames[i] = ViewHelpers::Find(scene, "Ability Slot " + std::to_string(i) + " Name", owner);
				m_SlotCosts[i] = ViewHelpers::Find(scene, "Ability Slot " + std::to_string(i) + " Count", owner);
			}
			for (int i = 0; i < KnownRows; i++)
			{
				m_Known[i] = ViewHelpers::Find(scene, "Known Ability " + std::to_string(i), owner);
				m_KnownNames[i] = ViewHelpers::Find(scene, "Known Ability " + std::to_string(i) + " Name", owner);
				m_KnownSlots[i] = ViewHelpers::Find(scene, "Known Ability " + std::to_string(i) + " Count", owner);
			}
			m_SlotCount = ViewHelpers::Find(scene, "Ability Slot Panel Header Count", owner);
			m_KnownCount = ViewHelpers::Find(scene, "Ability Known Panel Header Count", owner);
			m_DetailName = ViewHelpers::Find(scene, "Ability Detail Name", owner);
			m_DetailInfo = ViewHelpers::Find(scene, "Ability Detail Info", owner);
			m_DetailText = ViewHelpers::Find(scene, "Ability Detail Text", owner);
			m_Help = ViewHelpers::Find(scene, "Help Text", owner);
		}

		// Moves the focus to the slots, on the one chosen last.
		void Focus()
		{
			UI::Focus(EntityOf(m_Slots[std::clamp(m_Slot, 0, SlotRows - 1)]));
		}

		// Whether the focus is in the known list (choosing what goes in a slot), for the help bar's keys.
		bool IsChoosing() const { return m_Choosing; }

		// The rows' events. Returns whether the event was the page's.
		bool HandleEvent(const UIEvent& event, Request& request)
		{
			request = Request::None;
			for (int i = 0; i < SlotRows; i++)
			{
				if ((entt::entity)event.Target != m_Slots[i])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					m_Slot = i;
					m_Choosing = false;
					PresentDetail(Slots()[(size_t)i], i);
					PresentHelp();
					break;
				case UIEventType::Submit:
					m_Slot = i;
					m_Choosing = true;
					UI::Focus(EntityOf(m_Known[RowOf(Slots()[(size_t)i])]));
					break;
				case UIEventType::Cancel:
					request = Request::Back;
					break;
				default:
					break;
				}
				return true;
			}
			for (int i = 0; i < KnownRows; i++)
			{
				if ((entt::entity)event.Target != m_Known[i])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					m_Choosing = true;
					PresentDetail(AbilityOfRow(i), -1);
					PresentHelp();
					break;
				case UIEventType::Submit:
					Assign(AbilityOfRow(i));
					break;
				case UIEventType::Cancel:
					m_Choosing = false;
					Focus();
					break;
				default:
					break;
				}
				return true;
			}
			return false;
		}

		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const std::array<DataRowRef, SlotRows>& slots = Slots();
			int filled = 0;
			for (int i = 0; i < SlotRows; i++)
			{
				const AbilityDefinition& ability = BattleData::Ability(slots[(size_t)i]);
				const bool set = slots[(size_t)i].IsSet();
				filled += set ? 1 : 0;
				UI::SetText(EntityOf(m_SlotNames[i]), set ? ability.Name : std::string("-"));
				UI::SetText(EntityOf(m_SlotCosts[i]), set ? std::to_string(ability.Cost) + " MP" : std::string());
			}
			UI::SetText(EntityOf(m_SlotCount), std::to_string(filled) + " / " + std::to_string(SlotRows));

			const std::vector<DataRowRef>& known = Known();
			UI::SetText(EntityOf(m_KnownCount), std::to_string(known.size()));
			for (int i = 0; i < KnownRows; i++)
			{
				const bool used = i == 0 || i - 1 < (int)known.size();
				ViewHelpers::SetVisible(EntityOf(m_Known[i]), used);
				if (used == false)
				{
					continue;
				}
				if (i == 0)
				{
					UI::SetText(EntityOf(m_KnownNames[i]), "Empty the slot");
					UI::SetText(EntityOf(m_KnownSlots[i]), "");
					continue;
				}
				const DataRowRef& ability = known[(size_t)(i - 1)];
				UI::SetText(EntityOf(m_KnownNames[i]), BattleData::Ability(ability).Name);
				const auto in = std::find(slots.begin(), slots.end(), ability);
				UI::SetText(EntityOf(m_KnownSlots[i]), in != slots.end() ? "Slot " + std::to_string(in - slots.begin() + 1) : std::string());
			}
			PresentDetail(slots[(size_t)std::clamp(m_Slot, 0, SlotRows - 1)], m_Slot);
		}

	private:
		std::array<DataRowRef, SlotRows>& Slots() { return BattleState::Get().HeroAbilitySlots[0]; }
		const std::vector<DataRowRef>& Known() { return BattleState::Get().HeroAbilities[0]; }

		// The known list's row of an ability (0, the "empty" row, for an unset or unknown one).
		int RowOf(const DataRowRef& ability)
		{
			const std::vector<DataRowRef>& known = Known();
			const auto found = std::find(known.begin(), known.end(), ability);
			const int row = found == known.end() ? 0 : (int)(found - known.begin()) + 1;
			return row < KnownRows ? row : 0;
		}

		DataRowRef AbilityOfRow(int row)
		{
			const std::vector<DataRowRef>& known = Known();
			return row >= 1 && row - 1 < (int)known.size() ? known[(size_t)(row - 1)] : DataRowRef();
		}

		void Assign(const DataRowRef& ability)
		{
			PartyRules::SetAbilitySlot(Slots(), Known(), (size_t)std::clamp(m_Slot, 0, SlotRows - 1), ability);
			HY_INFO("Menu: ability slot {} = {}", m_Slot + 1, ability.IsSet() ? BattleData::Ability(ability).Name : std::string("(empty)"));
			m_Choosing = false;
			Present();
			Focus();
		}

		// The details of an ability (an unset one: the slot is empty).
		void PresentDetail(const DataRowRef& reference, int slot)
		{
			if (reference.IsSet() == false)
			{
				UI::SetText(EntityOf(m_DetailName), slot >= 0 ? "Slot " + std::to_string(slot + 1) : std::string("Empty the slot"));
				UI::SetText(EntityOf(m_DetailInfo), "");
				UI::SetText(EntityOf(m_DetailText), slot >= 0 ? "Empty. Press Enter to choose an ability for it." : "Leaves the slot empty.");
				return;
			}
			const AbilityDefinition& ability = BattleData::Ability(reference);
			UI::SetText(EntityOf(m_DetailName), ability.Name);
			std::string info = ability.Heals ? std::string("Heal") : std::string(GetTypeName(ability.Type));
			info += "   " + std::to_string(ability.Cost) + " MP";
			UI::SetText(EntityOf(m_DetailInfo), info);
			UI::SetText(EntityOf(m_DetailText), ability.Description);
		}

		void PresentHelp()
		{
			UI::SetText(EntityOf(m_Help), m_Choosing ? "Choose what goes in the slot." : "Choose a slot to change.");
		}

		static const char* GetTypeName(DamageType type)
		{
			switch (type)
			{
			case DamageType::Slash: return "Slash";
			case DamageType::Pierce: return "Pierce";
			case DamageType::Blunt: return "Blunt";
			case DamageType::Fire: return "Fire";
			case DamageType::Light: return "Light";
			default: return "";
			}
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		entt::entity m_Slots[SlotRows] = {};
		entt::entity m_SlotNames[SlotRows] = {};
		entt::entity m_SlotCosts[SlotRows] = {};
		entt::entity m_Known[KnownRows] = {};
		entt::entity m_KnownNames[KnownRows] = {};
		entt::entity m_KnownSlots[KnownRows] = {};
		entt::entity m_SlotCount = entt::null, m_KnownCount = entt::null;
		entt::entity m_DetailName = entt::null, m_DetailInfo = entt::null, m_DetailText = entt::null, m_Help = entt::null;
		int m_Slot = 0;          // the slot last focused: where a chosen ability goes
		bool m_Choosing = false; // the focus is in the known list
	};
}
