// Game/src/Scripts/Field/EquipmentView.h
#pragma once

#include "Hydro.h"
#include "Data/ItemDefinition.h"
#include "Rules/PartyRules.h"
#include "State/BattleState.h"
#include "State/Inventory.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The field menu's Equipment page (the Menu plan's M5, after Octopath's equipment screen; "Page Equipment" in
	// "Field Menu Canvas"; built by scripts/ui/build_game_ui.py). Owned by FieldMenuView. What the hero wears per slot
	// on the left ("Gear Slot 0" to "5", EquipPosition), the stats below ("Gear Stat N": now, and with the focused
	// choice, gold when better, red when worse), and the bag's items that fit the slot last focused on the right
	// ("Gear Choice 0" to "9"; row 0 takes off what is worn). Enter on a slot moves to the choices; Enter on a choice
	// equips it (PartyRules::Equip: one comes out of the inventory, what was worn goes back) and goes back to the slot.
	class EquipmentView
	{
	public:
		static constexpr int SlotRows = BattleState::EquipSlotCount;
		static constexpr int ChoiceRows = 10; // row 0 takes off, then up to 9 items
		static constexpr int StatCount = 5;

		enum class Request : uint8_t { None, Back };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Equipment page";
			for (int i = 0; i < SlotRows; i++)
			{
				m_Slots[i] = ViewHelpers::Find(scene, "Gear Slot " + std::to_string(i), owner);
				m_SlotItems[i] = ViewHelpers::Find(scene, "Gear Slot " + std::to_string(i) + " Name", owner);
				m_SlotNames[i] = ViewHelpers::Find(scene, "Gear Slot " + std::to_string(i) + " Count", owner);
			}
			for (int i = 0; i < ChoiceRows; i++)
			{
				m_Choices[i] = ViewHelpers::Find(scene, "Gear Choice " + std::to_string(i), owner);
				m_ChoiceNames[i] = ViewHelpers::Find(scene, "Gear Choice " + std::to_string(i) + " Name", owner);
				m_ChoiceCounts[i] = ViewHelpers::Find(scene, "Gear Choice " + std::to_string(i) + " Count", owner);
			}
			for (int i = 0; i < StatCount; i++)
			{
				m_StatNow[i] = ViewHelpers::Find(scene, "Gear Stat " + std::to_string(i) + " Now", owner);
				m_StatNew[i] = ViewHelpers::Find(scene, "Gear Stat " + std::to_string(i) + " New", owner);
			}
			m_ChoiceHeader = ViewHelpers::Find(scene, "Gear Choice Panel Header Count", owner);
			m_Help = ViewHelpers::Find(scene, "Help Text", owner);
		}

		void Focus()
		{
			UI::Focus(EntityOf(m_Slots[std::clamp(m_Slot, 0, SlotRows - 1)]));
		}

		bool IsChoosing() const { return m_Choosing; }

		// Whether something was put on or taken off since the last call (the menu then refreshes the other pages).
		bool TakeChanged()
		{
			const bool changed = m_Changed;
			m_Changed = false;
			return changed;
		}

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
					m_Choosing = false;
					if (m_Slot != i)
					{
						m_Slot = i;
						PresentChoices();
					}
					PresentStats(Worn());
					UI::SetText(EntityOf(m_Help), "Choose a slot to change.");
					break;
				case UIEventType::Submit:
					m_Slot = i;
					m_Choosing = true;
					PresentChoices();
					UI::Focus(EntityOf(m_Choices[m_ChoiceItems.size() > 1 ? 1 : 0]));
					break;
				case UIEventType::Cancel:
					request = Request::Back;
					break;
				default:
					break;
				}
				return true;
			}
			for (int i = 0; i < ChoiceRows; i++)
			{
				if ((entt::entity)event.Target != m_Choices[i])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					m_Choosing = true;
					PresentChoice(i);
					break;
				case UIEventType::Submit:
					Choose(i);
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
			const std::array<DataRowRef, SlotRows>& worn = Worn();
			for (int i = 0; i < SlotRows; i++)
			{
				UI::SetText(EntityOf(m_SlotNames[i]), PartyRules::GetPositionName((EquipPosition)i));
				UI::SetText(EntityOf(m_SlotItems[i]), NameOf(worn[(size_t)i], "-"));
			}
			PresentChoices();
			PresentStats(worn);
		}

	private:
		std::array<DataRowRef, SlotRows>& Worn() { return BattleState::Get().HeroEquipment[0]; }

		static std::string NameOf(const DataRowRef& reference, const char* none)
		{
			if (reference.IsSet() == false)
			{
				return none;
			}
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(reference);
			return item.IsValid() ? item->Name : "Missing: " + reference.Row;
		}

		// The choices for the slot last focused: "Take off", then the bag's items that fit it.
		void PresentChoices()
		{
			const EquipPosition position = (EquipPosition)std::clamp(m_Slot, 0, SlotRows - 1);
			m_ChoiceItems.assign(1, DataRowRef());
			std::vector<int> counts(1, 0);
			for (const Inventory::Slot& slot : Inventory::Get().Slots)
			{
				const DataView<ItemDefinition> item = slot.Count > 0 ? Data::GetRow<ItemDefinition>(slot.Item) : DataView<ItemDefinition>();
				if (item.IsValid() && PartyRules::Fits(item->Slot, position) && (int)m_ChoiceItems.size() < ChoiceRows)
				{
					m_ChoiceItems.push_back(slot.Item);
					counts.push_back(slot.Count);
				}
			}
			UI::SetText(EntityOf(m_ChoiceHeader), PartyRules::GetPositionName(position));
			for (int i = 0; i < ChoiceRows; i++)
			{
				const bool used = i < (int)m_ChoiceItems.size();
				ViewHelpers::SetVisible(EntityOf(m_Choices[i]), used);
				if (used)
				{
					UI::SetText(EntityOf(m_ChoiceNames[i]), i == 0 ? std::string("Take off") : NameOf(m_ChoiceItems[(size_t)i], ""));
					UI::SetText(EntityOf(m_ChoiceCounts[i]), i == 0 ? std::string() : "x" + std::to_string(counts[(size_t)i]));
				}
			}
		}

		// The stats with choice `row` worn in the slot, and the help line (the item's description).
		void PresentChoice(int row)
		{
			std::array<DataRowRef, SlotRows> preview = Worn();
			const DataRowRef item = row >= 0 && row < (int)m_ChoiceItems.size() ? m_ChoiceItems[(size_t)row] : DataRowRef();
			preview[(size_t)std::clamp(m_Slot, 0, SlotRows - 1)] = item;
			PresentStats(preview);
			const DataView<ItemDefinition> definition = item.IsSet() ? Data::GetRow<ItemDefinition>(item) : DataView<ItemDefinition>();
			UI::SetText(EntityOf(m_Help), definition.IsValid() ? definition->Description : std::string("Take off what is worn here."));
		}

		// Now (what is worn) against `with`: gold where `with` is better, red where it is worse.
		void PresentStats(const std::array<DataRowRef, SlotRows>& with)
		{
			const int level = std::max(1, BattleState::Get().HeroLevel[0]);
			const HeroStats now = BattleData::StatsWith(0, level, Worn());
			const HeroStats then = BattleData::StatsWith(0, level, with);
			const int a[StatCount] = { now.MaxHP, now.MaxMP, now.Attack, now.Magic, now.Speed };
			const int b[StatCount] = { then.MaxHP, then.MaxMP, then.Attack, then.Magic, then.Speed };
			for (int i = 0; i < StatCount; i++)
			{
				UI::SetText(EntityOf(m_StatNow[i]), std::to_string(a[i]));
				Entity value = EntityOf(m_StatNew[i]);
				UI::SetText(value, std::to_string(b[i]));
				if (UITextComponent* text = value.IsValid() ? value.TryGetComponent<UITextComponent>() : nullptr)
				{
					text->Color = b[i] > a[i] ? glm::vec4(1.0f, 0.84f, 0.5f, 1.0f) : (b[i] < a[i] ? glm::vec4(0.95f, 0.45f, 0.38f, 1.0f) : glm::vec4(0.95f, 0.91f, 0.83f, 1.0f));
				}
			}
		}

		// Wears choice `row` (or takes off, row 0), then back to the slot.
		void Choose(int row)
		{
			const EquipPosition position = (EquipPosition)std::clamp(m_Slot, 0, SlotRows - 1);
			Inventory& inventory = Inventory::Get();
			auto give = [&inventory](const DataRowRef& item) { inventory.Add(item, 1); };
			if (row <= 0 || row >= (int)m_ChoiceItems.size())
			{
				PartyRules::Unequip(Worn(), position, give);
			}
			else
			{
				const DataRowRef item = m_ChoiceItems[(size_t)row];
				const DataView<ItemDefinition> definition = Data::GetRow<ItemDefinition>(item);
				auto take = [&inventory](const DataRowRef& taken) { return inventory.Remove(taken, 1); };
				if (definition.IsValid())
				{
					PartyRules::Equip(Worn(), position, item, definition->Slot, take, give);
				}
			}
			// A lower maximum can't leave more HP or MP than it allows.
			BattleState& party = BattleState::Get();
			party.HeroHP[0] = std::min(party.HeroHP[0], GetHero(0).MaxHP);
			party.HeroMP[0] = std::min(party.HeroMP[0], GetHero(0).MaxMP);
			HY_INFO("Menu: {} = {}", PartyRules::GetPositionName(position), NameOf(Worn()[(size_t)position], "(nothing)"));
			m_Choosing = false;
			m_Changed = true;
			Present();
			Focus();
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		entt::entity m_Slots[SlotRows] = {};
		entt::entity m_SlotItems[SlotRows] = {};
		entt::entity m_SlotNames[SlotRows] = {};
		entt::entity m_Choices[ChoiceRows] = {};
		entt::entity m_ChoiceNames[ChoiceRows] = {};
		entt::entity m_ChoiceCounts[ChoiceRows] = {};
		entt::entity m_StatNow[StatCount] = {};
		entt::entity m_StatNew[StatCount] = {};
		entt::entity m_ChoiceHeader = entt::null, m_Help = entt::null;
		std::vector<DataRowRef> m_ChoiceItems; // the choice rows' items (row 0: none, take off)
		int m_Slot = 0;
		bool m_Choosing = false;
		bool m_Changed = false;
	};
}
