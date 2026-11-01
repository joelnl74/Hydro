// Game/src/Scripts/Field/FieldMenuView.h
#pragma once

#include "Hydro.h"
#include "Scripts/Field/AbilitiesView.h"
#include "Scripts/Field/CharacterView.h"
#include "Scripts/Field/EquipmentView.h"
#include "Scripts/Field/InventoryView.h"
#include "Scripts/Field/QuestsView.h"
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
	// Esc goes back to the tabs and from the tabs closes the menu. The pages: InventoryView, QuestsView,
	// CharacterView (M4; it takes no focus: Enter stays on the tabs), AbilitiesView and EquipmentView (M5).
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
			for (int h = 0; h < HeroCount; h++)
			{
				// "Menu Status <Id>": one block per hero who can be in the party; those not in it are hidden.
				const std::string status = std::string("Menu Status ") + BattleData::HeroIds[h];
				Status& block = m_Status[h];
				block.Root = ViewHelpers::Find(scene, status, owner);
				block.Name = ViewHelpers::Find(scene, status + " Name", owner);
				block.HP = ViewHelpers::Find(scene, status + " HP", owner);
				block.MP = ViewHelpers::Find(scene, status + " MP", owner);
				block.HPFill = ViewHelpers::Find(scene, status + " HP Fill", owner);
				block.MPFill = ViewHelpers::Find(scene, status + " MP Fill", owner);
			}
			m_Items.Bind(scene);
			m_Quests.Bind(scene);
			m_Character.Bind(scene);
			m_Abilities.Bind(scene);
			m_Equipment.Bind(scene);
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

			AbilitiesView::Request abilities = AbilitiesView::Request::None;
			if (m_Abilities.HandleEvent(event, abilities))
			{
				m_InPage = true;
				if (abilities == AbilitiesView::Request::Back)
				{
					BackToTabs();
				}
				else
				{
					PresentKeys();
				}
				return true;
			}

			EquipmentView::Request equipment = EquipmentView::Request::None;
			if (m_Equipment.HandleEvent(event, equipment))
			{
				m_InPage = true;
				if (equipment == EquipmentView::Request::Back)
				{
					BackToTabs();
				}
				else
				{
					PresentKeys();
				}
				if (m_Equipment.TakeChanged())
				{
					// The status, the Character page and the bag's counts follow what is worn.
					Present();
				}
				return true;
			}

			QuestsView::Request quest = QuestsView::Request::None;
			if (m_Quests.HandleEvent(event, quest))
			{
				if (event.Type == UIEventType::FocusEnter)
				{
					m_InPage = true;
					PresentKeys();
				}
				if (quest == QuestsView::Request::Back)
				{
					BackToTabs();
				}
				return true;
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

		// Everything shown: the party's status (a block per hero in the party; the one the hero pages show has a gold
		// name) and the pages' contents.
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const BattleState& party = BattleState::Get();
			if (party.InParty(m_Hero) == false)
			{
				m_Hero = 0;
			}
			for (int h = 0; h < HeroCount; h++)
			{
				const Status& block = m_Status[h];
				const bool shown = party.InParty(h);
				ViewHelpers::SetVisible(EntityOf(block.Root), shown);
				if (shown == false)
				{
					continue;
				}
				const HeroStats& hero = GetHero(h);
				UI::SetText(EntityOf(block.Name), hero.Name);
				UI::SetText(EntityOf(block.HP), std::to_string(party.HeroHP[h]) + " / " + std::to_string(hero.MaxHP));
				UI::SetText(EntityOf(block.MP), std::to_string(party.HeroMP[h]) + " / " + std::to_string(hero.MaxMP));
				ViewHelpers::SetBarFill(EntityOf(block.HPFill), (float)party.HeroHP[h] / (float)std::max(1, hero.MaxHP));
				ViewHelpers::SetBarFill(EntityOf(block.MPFill), (float)party.HeroMP[h] / (float)std::max(1, hero.MaxMP));
				Entity name = EntityOf(block.Name);
				if (UITextComponent* text = name.IsValid() ? name.TryGetComponent<UITextComponent>() : nullptr)
				{
					const bool marked = h == m_Hero && IsHeroPage() && party.PartySize() > 1;
					text->Color = marked ? glm::vec4(1.0f, 0.84f, 0.5f, 1.0f) : glm::vec4(0.95f, 0.91f, 0.83f, 1.0f);
				}
			}
			m_Items.Present(m_Selected == Tab::Inventory && m_InPage);
			m_Quests.Present();
			m_Character.Present(m_Hero);
			m_Abilities.SetHero(m_Hero);
			m_Equipment.SetHero(m_Hero);
			if (m_InPage == false)
			{
				PresentTabHelp();
			}
		}

		// Q / E on the Character, Abilities and Equipment pages: the next or previous hero in the party. The focus
		// stays where it is (a page's list keeps its row).
		void CycleHero(int direction)
		{
			const BattleState& party = BattleState::Get();
			if (IsOpen() == false || IsHeroPage() == false || party.PartySize() < 2)
			{
				return;
			}
			for (int step = 1; step <= HeroCount; step++)
			{
				const int next = ((m_Hero + direction * step) % HeroCount + HeroCount) % HeroCount;
				if (party.InParty(next))
				{
					m_Hero = next;
					break;
				}
			}
			Present();
			PresentKeys();
		}

	private:
		struct Status
		{
			entt::entity Root = entt::null, Name = entt::null, HP = entt::null, MP = entt::null, HPFill = entt::null, MPFill = entt::null;
		};

		bool IsHeroPage() const
		{
			return m_Selected == Tab::Character || m_Selected == Tab::Abilities || m_Selected == Tab::Equipment;
		}

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
			else if (tab == Tab::Quests)
			{
				m_Quests.PresentDetails();
			}
			Present();
			PresentTabHelp();
		}

		// Enter on a tab: into its page's list (items, quests); a page without one (Character, the pages still to
		// come) leaves the focus on the tabs.
		void EnterPage()
		{
			if (m_Selected == Tab::Inventory && m_Items.HasItems())
			{
				m_InPage = true;
				m_Items.Focus();
				PresentKeys();
			}
			else if (m_Selected == Tab::Quests && m_Quests.HasQuests())
			{
				m_InPage = true;
				m_Quests.Focus();
				PresentKeys();
			}
			else if (m_Selected == Tab::Abilities)
			{
				m_InPage = true;
				m_Abilities.Focus();
				PresentKeys();
			}
			else if (m_Selected == Tab::Equipment)
			{
				m_InPage = true;
				m_Equipment.Focus();
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
				UI::SetText(EntityOf(m_Help), m_Quests.HasQuests() ? "Look back over your quests." : "No quests yet.");
				break;
			case Tab::Character:
				UI::SetText(EntityOf(m_Help), "Level, stats and equipment.");
				break;
			case Tab::Abilities:
				UI::SetText(EntityOf(m_Help), "Choose the abilities the battle's Abilities command offers.");
				break;
			case Tab::Equipment:
				UI::SetText(EntityOf(m_Help), "Change what is worn.");
				break;
			}
			PresentKeys();
		}

		void PresentKeys()
		{
			if (m_InPage)
			{
				const char* keys = "Esc  Back        Tab  Close";
				if (m_Selected == Tab::Inventory)
				{
					keys = "Enter  Use        Esc  Back        Tab  Close";
				}
				else if ((m_Selected == Tab::Abilities && m_Abilities.IsChoosing()) || (m_Selected == Tab::Equipment && m_Equipment.IsChoosing()))
				{
					keys = "Enter  Choose        Esc  Back        Tab  Close";
				}
				else if (m_Selected == Tab::Abilities || m_Selected == Tab::Equipment)
				{
					keys = "Enter  Change        Esc  Back        Tab  Close";
				}
				UI::SetText(EntityOf(m_Keys), HeroKeys() + keys);
				return;
			}
			const bool opens = (m_Selected == Tab::Inventory && m_Items.HasItems()) || (m_Selected == Tab::Quests && m_Quests.HasQuests())
				|| m_Selected == Tab::Abilities || m_Selected == Tab::Equipment;
			UI::SetText(EntityOf(m_Keys), HeroKeys() + (opens ? "Enter  Open        Esc / Tab  Close" : "Esc / Tab  Close"));
		}

		// "Q / E  Hero" on the hero pages while the party has more than one.
		std::string HeroKeys() const
		{
			return IsHeroPage() && BattleState::Get().PartySize() > 1 ? std::string("Q / E  Hero        ") : std::string();
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		InventoryView m_Items;
		QuestsView m_Quests;
		CharacterView m_Character;
		AbilitiesView m_Abilities;
		EquipmentView m_Equipment;
		entt::entity m_Canvas = entt::null;
		entt::entity m_Title = entt::null;
		entt::entity m_Tabs[TabCount] = {};
		entt::entity m_TabNames[TabCount] = {};
		entt::entity m_TabMarks[TabCount] = {};
		entt::entity m_Pages[TabCount] = {};
		entt::entity m_Help = entt::null, m_Keys = entt::null;
		Status m_Status[HeroCount];
		int m_Hero = 0;                  // whose page the hero pages show (Q / E)
		Tab m_Selected = Tab::Inventory; // kept between opens
		bool m_InPage = false;           // the focus is in the page, not on the tabs
	};
}
