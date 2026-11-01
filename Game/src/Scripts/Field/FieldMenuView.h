// Game/src/Scripts/Field/FieldMenuView.h
#pragma once

#include "Hydro.h"
#include "Scripts/Field/InventoryView.h"
#include "State/BattleState.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>

namespace Hydro
{
	// The field menu (Tab; the Menu plan's M3, Octopath-style): "Field Menu Canvas" with a column of tab ribbons
	// ("Tab 0" to "Tab 4": Inventory, Quests, Character, Abilities, Equipment), the selected tab's page ("Page <Tab>"),
	// the party's status ("Menu Status Miko") and a help bar ("Help Text", "Help Keys"); scripts/ui/build_game_ui.py
	// builds it. A view: it opens on the tabs; moving over a tab shows its page, Enter (or a click) moves into the page,
	// Esc goes back to the tabs and from the tabs closes the menu. The Inventory page is InventoryView; the other
	// pages come with M4 (Quests, Character) and M5 (Abilities, Equipment).
	class FieldMenuView
	{
	public:
		enum class Tab : uint8_t { Inventory = 0, Quests, Character, Abilities, Equipment };
		static constexpr int TabCount = 5;

		enum class Request : uint8_t { None, UseItem, Close };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Field menu";
			m_Canvas = ViewHelpers::Find(scene, "Field Menu Canvas", owner);
			m_Title = ViewHelpers::Find(scene, "Menu Title", owner);
			for (int t = 0; t < TabCount; t++)
			{
				const std::string tag = "Tab " + std::to_string(t);
				m_Tabs[t] = ViewHelpers::Find(scene, tag, owner);
				m_TabNames[t] = ViewHelpers::Find(scene, tag + " Name", owner);
				m_TabMarks[t] = ViewHelpers::Find(scene, tag + " Mark", owner);
				m_Pages[t] = ViewHelpers::Find(scene, std::string("Page ") + GetTabName((Tab)t), owner);
			}
			m_Help = ViewHelpers::Find(scene, "Help Text", owner);
			m_Keys = ViewHelpers::Find(scene, "Help Keys", owner);
			m_StatusName = ViewHelpers::Find(scene, "Menu Status Miko Name", owner);
			m_StatusHP = ViewHelpers::Find(scene, "Menu Status Miko HP", owner);
			m_StatusMP = ViewHelpers::Find(scene, "Menu Status Miko MP", owner);
			m_StatusHPFill = ViewHelpers::Find(scene, "Menu Status Miko HP Fill", owner);
			m_StatusMPFill = ViewHelpers::Find(scene, "Menu Status Miko MP Fill", owner);
			m_Items.Bind(scene);
		}

		static const char* GetTabName(Tab tab)
		{
			switch (tab)
			{
			case Tab::Inventory: return "Inventory";
			case Tab::Quests: return "Quests";
			case Tab::Character: return "Character";
			case Tab::Abilities: return "Abilities";
			case Tab::Equipment: return "Equipment";
			default: return "";
			}
		}

		InventoryView& Items() { return m_Items; }

		bool IsOpen() const
		{
			return m_Scene != nullptr && UI::IsVisible(EntityOf(m_Canvas));
		}

		// Opens on the tabs (the tab chosen last time selected and focused) or closes.
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
				ShowTab(m_Selected);
				UI::Focus(EntityOf(m_Tabs[(int)m_Selected]));
			}
		}

		// The menu's events (they bubble up to the director, which hands them here). Returns whether the event was the
		// menu's; request says what the director has to do.
		bool HandleEvent(const UIEvent& event, Request& request)
		{
			request = Request::None;
			for (int t = 0; t < TabCount; t++)
			{
				if ((entt::entity)event.Target != m_Tabs[t])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					ShowTab((Tab)t);
					return true;
				case UIEventType::Submit:
					ShowTab((Tab)t);
					EnterPage();
					return true;
				case UIEventType::Cancel:
					request = Request::Close;
					return true;
				default:
					return true;
				}
			}

			InventoryView::Request item = InventoryView::Request::None;
			if (m_Items.HandleEvent(event, item))
			{
				if (event.Type == UIEventType::FocusEnter)
				{
					m_InPage = true;
					PresentKeys();
				}
				if (item == InventoryView::Request::Use)
				{
					request = Request::UseItem;
				}
				else if (item == InventoryView::Request::Back)
				{
					BackToTabs();
				}
				return true;
			}

			if ((entt::entity)event.Target == m_Canvas && event.Type == UIEventType::Cancel)
			{
				// Nothing focused (a click on the backdrop): back to the tabs, or close from there.
				if (m_InPage)
				{
					BackToTabs();
				}
				else
				{
					request = Request::Close;
				}
				return true;
			}
			return false;
		}

		// Everything shown: the party's status (the first hero; the party is Miko alone) and the pages' contents.
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const HeroStats& hero = GetHero(0);
			const BattleState& party = BattleState::Get();
			UI::SetText(EntityOf(m_StatusName), hero.Name);
			UI::SetText(EntityOf(m_StatusHP), std::to_string(party.HeroHP[0]) + " / " + std::to_string(hero.MaxHP));
			UI::SetText(EntityOf(m_StatusMP), std::to_string(party.HeroMP[0]) + " / " + std::to_string(hero.MaxMP));
			ViewHelpers::SetBarFill(EntityOf(m_StatusHPFill), (float)party.HeroHP[0] / (float)std::max(1, hero.MaxHP));
			ViewHelpers::SetBarFill(EntityOf(m_StatusMPFill), (float)party.HeroMP[0] / (float)std::max(1, hero.MaxMP));
			m_Items.Present();
			if (m_InPage == false)
			{
				PresentTabHelp();
			}
		}

	private:
		// Selects a tab: its page shown (the others hidden), its mark shown and name gilded, the title.
		void ShowTab(Tab tab)
		{
			m_Selected = tab;
			m_InPage = false;
			for (int t = 0; t < TabCount; t++)
			{
				const bool selected = t == (int)tab;
				ViewHelpers::SetVisible(EntityOf(m_Pages[t]), selected);
				ViewHelpers::SetVisible(EntityOf(m_TabMarks[t]), selected);
				Entity name = EntityOf(m_TabNames[t]);
				if (UITextComponent* text = name.IsValid() ? name.TryGetComponent<UITextComponent>() : nullptr)
				{
					text->Color = selected ? glm::vec4(1.0f, 0.84f, 0.5f, 1.0f) : glm::vec4(0.95f, 0.91f, 0.83f, 1.0f);
				}
			}
			UI::SetText(EntityOf(m_Title), GetTabName(tab));
			if (tab == Tab::Inventory)
			{
				m_Items.PresentDetails();
			}
			PresentTabHelp();
		}

		// Enter on a tab: into its page (the inventory's list); the pages still to come stay on the tabs.
		void EnterPage()
		{
			if (m_Selected == Tab::Inventory && m_Items.HasItems())
			{
				m_InPage = true;
				m_Items.Focus();
				PresentKeys();
			}
		}

		void BackToTabs()
		{
			m_InPage = false;
			UI::Focus(EntityOf(m_Tabs[(int)m_Selected]));
			PresentTabHelp();
		}

		void PresentTabHelp()
		{
			switch (m_Selected)
			{
			case Tab::Inventory:
				UI::SetText(EntityOf(m_Help), m_Items.HasItems() ? "Look through and use your items." : "The bag is empty.");
				break;
			case Tab::Quests:
			case Tab::Character:
			case Tab::Abilities:
			case Tab::Equipment:
				UI::SetText(EntityOf(m_Help), "Not built yet: comes with a later step of the menu plan.");
				break;
			}
			PresentKeys();
		}

		void PresentKeys()
		{
			UI::SetText(EntityOf(m_Keys), m_InPage ? "Enter  Use        Esc  Back        Tab  Close" : "Enter  Open        Esc / Tab  Close");
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		InventoryView m_Items;
		entt::entity m_Canvas = entt::null;
		entt::entity m_Title = entt::null;
		entt::entity m_Tabs[TabCount] = {};
		entt::entity m_TabNames[TabCount] = {};
		entt::entity m_TabMarks[TabCount] = {};
		entt::entity m_Pages[TabCount] = {};
		entt::entity m_Help = entt::null, m_Keys = entt::null;
		entt::entity m_StatusName = entt::null, m_StatusHP = entt::null, m_StatusMP = entt::null, m_StatusHPFill = entt::null, m_StatusMPFill = entt::null;
		Tab m_Selected = Tab::Inventory; // kept between opens
		bool m_InPage = false;           // the focus is in the page, not on the tabs
	};
}
