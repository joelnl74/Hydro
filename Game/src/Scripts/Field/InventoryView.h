// Game/src/Scripts/Field/InventoryView.h
#pragma once

#include "Hydro.h"
#include "Data/ItemDefinition.h"
#include "State/BattleState.h"
#include "State/Inventory.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>

namespace Hydro
{
	// The field's inventory menu (Tab; "Inventory Canvas" with rows "Item 0" to "Item 19", the selected item's details,
	// a help line and the party's status; scripts/ui/build_game_ui.py builds it). A view (Scripting plan SC8f): it
	// shows the run's Inventory and the party, keeps the selection, and turns its rows' UI events into what the player
	// asked for; the director uses the item (Rules/ItemRules.h). The Menu plan's tab menu replaces it.
	class InventoryView
	{
	public:
		static constexpr int MaxRows = 20; // the rows in the scene; more items aren't shown

		enum class Request : uint8_t { None, Use, Close };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Inventory";
			m_Canvas = ViewHelpers::Find(scene, "Inventory Canvas", owner);
			for (int i = 0; i < MaxRows; i++)
			{
				m_Row[i] = ViewHelpers::Find(scene, "Item " + std::to_string(i), owner);
				m_Name[i] = ViewHelpers::Find(scene, "Item " + std::to_string(i) + " Name", owner);
				m_Count[i] = ViewHelpers::Find(scene, "Item " + std::to_string(i) + " Count", owner);
			}
			m_DetailName = ViewHelpers::Find(scene, "Item Detail Name", owner);
			m_DetailText = ViewHelpers::Find(scene, "Item Detail Text", owner);
			m_Help = ViewHelpers::Find(scene, "Help Text", owner);
			m_StatusName = ViewHelpers::Find(scene, "Menu Status Miko Name", owner);
			m_StatusHP = ViewHelpers::Find(scene, "Menu Status Miko HP", owner);
			m_StatusMP = ViewHelpers::Find(scene, "Menu Status Miko MP", owner);
			m_StatusHPFill = ViewHelpers::Find(scene, "Menu Status Miko HP Fill", owner);
			m_StatusMPFill = ViewHelpers::Find(scene, "Menu Status Miko MP Fill", owner);
		}

		bool IsOpen() const
		{
			return m_Scene != nullptr && UI::IsVisible(EntityOf(m_Canvas));
		}

		// Opens (showing the list, the selection kept between opens and focused) or closes it.
		void SetOpen(bool open)
		{
			Entity canvas = EntityOf(m_Canvas);
			if (canvas.IsValid() == false)
			{
				return;
			}
			UI::SetVisible(canvas, open);
			if (open)
			{
				Present();
				m_Selected = std::clamp(m_Selected, 0, std::max(0, std::min((int)Inventory::Get().Slots.size(), MaxRows) - 1));
				UI::Focus(EntityOf(m_Row[m_Selected]));
			}
		}

		int GetSelected() const { return m_Selected; }

		// A row's events (they bubble up to the director, which hands them here): the focus selects, Submit asks to
		// use, Cancel to close. Returns whether the event was the menu's.
		bool HandleEvent(const UIEvent& event, Request& request)
		{
			request = Request::None;
			for (int i = 0; i < MaxRows; i++)
			{
				if ((entt::entity)event.Target != m_Row[i])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					m_Selected = i;
					PresentDetails();
					return true;
				case UIEventType::Submit:
					m_Selected = i;
					request = Request::Use;
					return true;
				case UIEventType::Cancel:
					request = Request::Close;
					return true;
				default:
					return true;
				}
			}
			if ((entt::entity)event.Target == m_Canvas && event.Type == UIEventType::Cancel)
			{
				request = Request::Close;
				return true;
			}
			return false;
		}

		// The rows: one per item (rows past the list are hidden, so the panel fits it), name and count; items that
		// can't be used here are greyed, items used up dimmer still. An item whose row is gone from the table shows as
		// "Missing: <id>". Then the details and the party's status (the first hero; the party is Miko alone).
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			for (int i = 0; i < MaxRows; i++)
			{
				const bool used = i < (int)Inventory::Get().Slots.size();
				Entity row = EntityOf(m_Row[i]);
				ViewHelpers::SetVisible(row, used);
				if (used == false)
				{
					continue;
				}
				const Inventory::Slot& slot = Inventory::Get().Slots[(size_t)i];
				const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
				const bool usable = item.IsValid() && item->CanUseInField();
				Entity name = EntityOf(m_Name[i]);
				UI::SetText(name, item.IsValid() ? item->Name : "Missing: " + slot.Item.Row);
				UI::SetText(EntityOf(m_Count[i]), "x" + std::to_string(slot.Count));
				if (UITextComponent* text = name.IsValid() ? name.TryGetComponent<UITextComponent>() : nullptr)
				{
					const glm::vec3 color = usable == false ? glm::vec3(0.74f, 0.71f, 0.64f)
						: (slot.Count <= 0 ? glm::vec3(0.5f) : glm::vec3(0.95f, 0.91f, 0.83f));
					text->Color = glm::vec4(color, 1.0f);
				}
			}
			PresentDetails();

			const HeroStats& hero = GetHero(0);
			const BattleState& party = BattleState::Get();
			UI::SetText(EntityOf(m_StatusName), hero.Name);
			UI::SetText(EntityOf(m_StatusHP), std::to_string(party.HeroHP[0]) + " / " + std::to_string(hero.MaxHP));
			UI::SetText(EntityOf(m_StatusMP), std::to_string(party.HeroMP[0]) + " / " + std::to_string(hero.MaxMP));
			ViewHelpers::SetBarFill(EntityOf(m_StatusHPFill), (float)party.HeroHP[0] / (float)std::max(1, hero.MaxHP));
			ViewHelpers::SetBarFill(EntityOf(m_StatusMPFill), (float)party.HeroMP[0] / (float)std::max(1, hero.MaxMP));
		}

	private:
		// The selected item's name and description, and the help line.
		void PresentDetails()
		{
			if (m_Selected < 0 || m_Selected >= (int)Inventory::Get().Slots.size())
			{
				return;
			}
			const Inventory::Slot& slot = Inventory::Get().Slots[(size_t)m_Selected];
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
			if (item.IsValid() == false)
			{
				UI::SetText(EntityOf(m_DetailName), "Missing: " + slot.Item.Row);
				UI::SetText(EntityOf(m_DetailText), "This item is not in the items table any more.");
				UI::SetText(EntityOf(m_Help), "Can't be used here.");
				return;
			}
			UI::SetText(EntityOf(m_DetailName), item->Name);
			UI::SetText(EntityOf(m_DetailText), item->Description);
			UI::SetText(EntityOf(m_Help), item->CanUseInField() ? (slot.Count > 0 ? "Use it now." : "None left.") : "Can't be used here.");
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		entt::entity m_Canvas = entt::null;
		entt::entity m_Row[MaxRows] = {};
		entt::entity m_Name[MaxRows] = {};
		entt::entity m_Count[MaxRows] = {};
		entt::entity m_DetailName = entt::null, m_DetailText = entt::null, m_Help = entt::null;
		entt::entity m_StatusName = entt::null, m_StatusHP = entt::null, m_StatusMP = entt::null, m_StatusHPFill = entt::null, m_StatusMPFill = entt::null;
		int m_Selected = 0;
	};
}
