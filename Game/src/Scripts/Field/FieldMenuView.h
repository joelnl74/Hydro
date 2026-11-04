#pragma once

#include "Hydro.h"
#include "Scripts/Field/AbilitiesView.h"
#include "Scripts/Field/CharacterView.h"
#include "Scripts/Field/EquipmentView.h"
#include "Scripts/Field/InventoryView.h"
#include "Scripts/Field/PartyView.h"
#include "Scripts/Field/QuestsView.h"
#include "State/PartyState.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>

namespace Hydro
{
	// The field menu (Tab): a column of tabs and the selected tab's page, the party's status and a help bar. Enter moves into
	// a page, Esc goes back to the tabs and from there closes the menu.
	class FieldMenuView
	{
	public:
		enum class Tab : uint8_t { Inventory = 0, Quests, Character, Abilities, Equipment, Party };
		static constexpr int TabCount = 6;

		enum class Request : uint8_t { None, UseItem, Close };
		// The party's status column: the line-up (the hero pages also reach the reserve).
		static constexpr int StatusRows = PartyState::MaxFighters;

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
			for (int s = 0; s < StatusRows; s++)
			{
				// One block per place at the front of the party; empty places are hidden.
				const std::string status = "Menu Status " + std::to_string(s);
				Status& block = m_Status[s];
				block.Root = ViewHelpers::Find(scene, status, owner);
				block.Name = ViewHelpers::Find(scene, status + " Name", owner);
				block.HP = ViewHelpers::Find(scene, status + " HP", owner);
				block.MP = ViewHelpers::Find(scene, status + " MP", owner);
				block.HPFill = ViewHelpers::Find(scene, status + " HP Fill", owner);
				block.MPFill = ViewHelpers::Find(scene, status + " MP Fill", owner);
			}
			m_MoneyLabel = ViewHelpers::Find(scene, "Menu Money Label", owner);
			m_Money = ViewHelpers::Find(scene, "Menu Money Amount", owner);
			m_Items.Bind(scene);
			m_Quests.Bind(scene);
			m_Character.Bind(scene);
			m_Abilities.Bind(scene);
			m_Equipment.Bind(scene);
			m_Party.Bind(scene);
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
			case Tab::Party: return "Party";
			default: return "";
			}
		}

		InventoryView& Items() { return m_Items; }

		bool IsOpen() const
		{
			return m_Scene != nullptr && UI::IsVisible(EntityOf(m_Canvas));
		}

		// Opens on the tab chosen last time.
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

		// Returns whether the event was the menu's; `request` says what the director has to do.
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

			PartyView::Request party = PartyView::Request::None;
			if (m_Party.HandleEvent(event, party))
			{
				m_InPage = true;
				if (party == PartyView::Request::Back)
				{
					BackToTabs();
				}
				else
				{
					if (party == PartyView::Request::Changed)
					{
						// The status column shows the new line-up.
						Present();
					}
					PresentKeys();
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

		// The hero the hero pages show has a gold name.
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const PartyState& party = PartyState::Get();
			m_Hero = std::clamp(m_Hero, 0, std::max(0, party.GetSize() - 1));
			for (int s = 0; s < StatusRows; s++)
			{
				const Status& block = m_Status[s];
				const bool shown = s < party.GetLineupSize();
				ViewHelpers::SetVisible(EntityOf(block.Root), shown);
				if (shown == false)
				{
					continue;
				}
				const PartyMember& member = party.Members[(size_t)s];
				const HeroStats hero = member.GetStats();
				UI::SetText(EntityOf(block.Name), hero.Name);
				UI::SetText(EntityOf(block.HP), std::to_string(member.HP) + " / " + std::to_string(hero.MaxHP));
				UI::SetText(EntityOf(block.MP), std::to_string(member.MP) + " / " + std::to_string(hero.MaxMP));
				ViewHelpers::SetBarFill(EntityOf(block.HPFill), (float)member.HP / (float)std::max(1, hero.MaxHP));
				ViewHelpers::SetBarFill(EntityOf(block.MPFill), (float)member.MP / (float)std::max(1, hero.MaxMP));
				Entity name = EntityOf(block.Name);
				if (UITextComponent* text = name.IsValid() ? name.TryGetComponent<UITextComponent>() : nullptr)
				{
					const bool marked = s == m_Hero && IsHeroPage() && party.GetSize() > 1;
					text->Color = marked ? glm::vec4(1.0f, 0.84f, 0.5f, 1.0f) : glm::vec4(0.95f, 0.91f, 0.83f, 1.0f);
				}
			}
			UI::SetText(EntityOf(m_MoneyLabel), CurrencyName);
			UI::SetText(EntityOf(m_Money), std::to_string(Inventory::Get().Money));
			m_Items.Present(m_Selected == Tab::Inventory && m_InPage);
			m_Quests.Present();
			m_Character.Present(m_Hero);
			m_Abilities.SetHero(m_Hero);
			m_Equipment.SetHero(m_Hero);
			m_Party.Present();
			if (m_InPage == false)
			{
				PresentTabHelp();
			}
		}

		// The next or previous member, reserve included; the focus stays where it is.
		void CycleHero(int direction)
		{
			const int size = PartyState::Get().GetSize();
			if (IsOpen() == false || IsHeroPage() == false || size < 2)
			{
				return;
			}
			m_Hero = ((m_Hero + direction) % size + size) % size;
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

		// A page without a list (Character) leaves the focus on the tabs.
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
			else if (m_Selected == Tab::Party)
			{
				m_InPage = true;
				m_Party.Focus();
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
			case Tab::Party:
				UI::SetText(EntityOf(m_Help),
					"Choose who fights: up to " + std::to_string(PartyState::GetLineupLimit()) + " in the line-up.");
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
				else if (m_Selected == Tab::Party)
				{
					UI::SetText(EntityOf(m_Keys), std::string(m_Party.GetEnterKey()) + "Esc  Back        Tab  Close");
					return;
				}
				else if ((m_Selected == Tab::Abilities && m_Abilities.IsChoosing())
					|| (m_Selected == Tab::Equipment && m_Equipment.IsChoosing()))
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
				|| m_Selected == Tab::Abilities || m_Selected == Tab::Equipment || m_Selected == Tab::Party;
			UI::SetText(EntityOf(m_Keys), HeroKeys() + (opens ? "Enter  Open        Esc / Tab  Close" : "Esc / Tab  Close"));
		}

		// Shown on the hero pages while the party has more than one hero.
		std::string HeroKeys() const
		{
			return IsHeroPage() && PartyState::Get().GetSize() > 1 ? std::string("Q / E  Hero        ") : std::string();
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		InventoryView m_Items;
		QuestsView m_Quests;
		CharacterView m_Character;
		AbilitiesView m_Abilities;
		EquipmentView m_Equipment;
		PartyView m_Party;
		entt::entity m_Canvas = entt::null;
		entt::entity m_Title = entt::null;
		entt::entity m_Tabs[TabCount] = {};
		entt::entity m_TabNames[TabCount] = {};
		entt::entity m_TabMarks[TabCount] = {};
		entt::entity m_Pages[TabCount] = {};
		entt::entity m_Help = entt::null, m_Keys = entt::null;
		Status m_Status[StatusRows];
		entt::entity m_MoneyLabel = entt::null, m_Money = entt::null;
		int m_Hero = 0;                  // whose page the hero pages show (Q / E): an index into the party
		Tab m_Selected = Tab::Inventory; // kept between opens
		bool m_InPage = false;           // the focus is in the page, not on the tabs
	};
}
