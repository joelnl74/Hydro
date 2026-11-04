#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Data/ItemDefinition.h"
#include "Rules/BattleRules.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Battle/BattleFlow.h"
#include "Scripts/Battle/BattleText.h"
#include "Scripts/Battle/Combatants.h"
#include "State/Inventory.h"
#include "State/PartyState.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

// The hero's command list (Attack to Run), the Abilities and Items lists in its place, and the target list; with the
// keys and pointer that drive them. The menus' buttons bubble their events up to the Battle Director (OnUIEvent).

namespace Hydro
{
	class CommandMenuView
	{
	public:
		static constexpr int CommandCount = 5;
		static constexpr int SubRows = PartyMember::AbilitySlotCount;
		static constexpr int TargetRows = BattleState::MaxEnemies;

		// What the hero chose this frame.
		enum class Choice : uint8_t { None, Use, NoMP };

		struct Result
		{
			Choice Kind = Choice::None;
			BattleCommand Command = BattleCommand::Attack;
		};

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			for (int c = 0; c < CommandCount; c++)
			{
				const std::string row = "Command " + std::to_string(c);
				m_Commands[(size_t)c] = BattleEntities::Find(scene, row);
				m_CommandNames[(size_t)c] = BattleEntities::Find(scene, row + " Name");
				m_CommandCosts[(size_t)c] = BattleEntities::Find(scene, row + " Cost");
			}
			// The sub-menu: the hero's ability slots, or the items usable in battle.
			m_SubPanel = BattleEntities::Find(scene, "Sub Panel");
			m_SubTitle = BattleEntities::Find(scene, "Sub Title");
			m_SubHelp = BattleEntities::Find(scene, "Sub Help");
			for (int r = 0; r < SubRows; r++)
			{
				const std::string row = "Sub Command " + std::to_string(r);
				m_SubButtons[(size_t)r] = BattleEntities::Find(scene, row);
				m_SubNames[(size_t)r] = BattleEntities::Find(scene, row + " Name");
				m_SubCosts[(size_t)r] = BattleEntities::Find(scene, row + " Cost");
			}
			m_CommandPanel = BattleEntities::Find(scene, "Command Panel");
			m_CommandHeader = BattleEntities::Find(scene, "Command Hero");
			m_CommandHelp = BattleEntities::Find(scene, "Command Help");
			m_CommandBoost = BattleEntities::Find(scene, "Command Boost");
			m_TargetPanel = BattleEntities::Find(scene, "Target Panel");
			m_TargetHeader = BattleEntities::Find(scene, "Target Command");
			for (int e = 0; e < TargetRows; e++)
			{
				const std::string row = "Target " + std::to_string(e);
				m_TargetButtons[(size_t)e] = BattleEntities::Find(scene, row);
				m_TargetNames[(size_t)e] = BattleEntities::Find(scene, row + " Name");
				m_TargetHP[(size_t)e] = BattleEntities::Find(scene, row + " HP");
			}
			UI::SetVisible(EntityOf(m_CommandPanel), false);
			UI::SetVisible(EntityOf(m_TargetPanel), false);
			UI::SetVisible(EntityOf(m_SubPanel), false);
			// Keys held when the battle opens (the player was still walking) do not count.
			m_Previous.fill(true);
		}

		// A new phase or turn starts on the commands.
		void ResetChoice()
		{
			m_Targeting = false;
			m_SubMenu = SubMenu::None;
			m_ChosenSub = -1;
			m_CancelSub = false;
		}

		// A command, a sub-menu row, the focused or chosen target, or Cancel; true when the event was the menu's.
		bool HandleEvent(const UIEvent& event, bool commanding, BattleAction& action, const Combatants& actors)
		{
			const bool targeting = commanding && m_Targeting;
			if (event.Type == UIEventType::Cancel)
			{
				if (targeting)
				{
					m_CancelTarget = true;
					return true;
				}
				if (commanding && m_SubMenu != SubMenu::None)
				{
					m_CancelSub = true;
					return true;
				}
				return false;
			}
			for (int r = 0; r < SubRows; r++)
			{
				if ((entt::entity)event.Target != m_SubButtons[(size_t)r])
				{
					continue;
				}
				if (event.Type == UIEventType::FocusEnter)
				{
					m_HelpSub = r;
				}
				else if (event.Type == UIEventType::Submit && commanding)
				{
					m_ChosenSub = r;
				}
				return true;
			}
			for (int e = 0; e < TargetRows; e++)
			{
				if ((entt::entity)event.Target != m_TargetButtons[(size_t)e])
				{
					continue;
				}
				const int enemy = actors.FindInSlot(Side::Enemy, e);
				if (targeting && actors.IsAlive(enemy))
				{
					if (event.Type == UIEventType::FocusEnter)
					{
						action.Target = enemy; // the cursor follows the focus (keys or the pointer)
					}
					else if (event.Type == UIEventType::Submit)
					{
						m_ChosenTarget = enemy;
					}
				}
				return true;
			}
			for (int c = 0; c < CommandCount; c++)
			{
				if ((entt::entity)event.Target != m_Commands[(size_t)c])
				{
					continue;
				}
				if (event.Type == UIEventType::FocusEnter)
				{
					m_HelpCommand = c;
				}
				else if (event.Type == UIEventType::Submit && commanding)
				{
					m_ChosenCommand = c;
				}
				return true;
			}
			return false;
		}

		// The hero's turn: the boost keys, then the target list, the sub-menu or the commands, whichever is open. Up / Down
		// move the menu's focus, so the boost and the target steps have keys of their own.
		Result Update(BattleAction& action, const Combatants& actors)
		{
			Result result;
			const Combatant& hero = actors.Get(action.Actor);
			PartyMember& member = Combatants::MemberOf(hero);
			if (Pressed(0, KeyCode::E))
			{
				action.Boost = std::min({ BattleRules::MaxBoost, hero.Momentum, action.Boost + 1 });
			}
			if (Pressed(1, KeyCode::Q))
			{
				action.Boost = std::max(0, action.Boost - 1);
			}

			// Read every frame, so a key held since the commands doesn't fire when the list opens.
			const int chosenTarget = m_ChosenTarget;
			const bool cancel = m_CancelTarget;
			m_ChosenTarget = -1;
			m_CancelTarget = false;
			const bool leftArrow = Pressed(2, KeyCode::Left);
			const bool leftKey = Pressed(3, KeyCode::A);
			const bool rightArrow = Pressed(4, KeyCode::Right);
			const bool rightKey = Pressed(5, KeyCode::D);
			const bool left = leftArrow || leftKey;
			const bool right = rightArrow || rightKey;
			if (m_Targeting)
			{
				// Left / Right step along the enemies as they stand, Up / Down move through the list.
				if (left || right)
				{
					action.Target = actors.Step(Side::Enemy, action.Target, left ? -1 : 1);
					UI::Focus(EntityOf(TargetButtonOf(actors, action.Target)));
				}
				if (cancel)
				{
					m_Targeting = false; // back to the sub-menu or the commands
					m_ChosenCommand = -1;
					return result;
				}
				if (chosenTarget >= 0 && actors.IsAlive(chosenTarget))
				{
					m_Targeting = false;
					m_SubMenu = SubMenu::None;
					action.Target = chosenTarget;
					result.Kind = Choice::Use;
					result.Command = m_TargetCommand;
				}
				return result;
			}

			if (m_SubMenu != SubMenu::None)
			{
				return UpdateSub(action, actors, member);
			}

			const int chosen = m_ChosenCommand;
			m_ChosenCommand = -1;
			if (chosen < 0 || chosen >= CommandCount)
			{
				return result;
			}
			member.LastCommand = chosen;
			switch ((BattleCommand)chosen)
			{
				case BattleCommand::Attack:
					OpenTargets(BattleCommand::Attack, action, actors);
					return result;
				case BattleCommand::Abilities:
					m_SubMenu = SubMenu::Abilities;
					m_HelpSub = member.LastAbility;
					return result;
				case BattleCommand::Items:
					m_SubMenu = SubMenu::Items;
					m_HelpSub = 0;
					return result;
				default:
					result.Kind = Choice::Use;
					result.Command = (BattleCommand)chosen;
					return result;
			}
		}

		// Empty slots and abilities the hero can't pay for are greyed out (not Interactable), so neither keys nor the
		// pointer pick them. Each list opens in the commands' place; rows past a list are hidden, so it closes up.
		void Present(const BattleAction& action, const Combatants& actors, bool choosing)
		{
			const bool hasHero = actors.IsValid(action.Actor) && actors.Get(action.Actor).IsHero();
			const PartyMember& member = PartyState::Get().GetMember(hasHero ? actors.Get(action.Actor).Member : 0);
			const std::string labels[CommandCount] = { GetLabelText(BattleLabel::Attack), GetLabelText(BattleLabel::Abilities),
				GetLabelText(BattleLabel::Items), GetLabelText(BattleLabel::Guard), GetLabelText(BattleLabel::Run) };
			const std::vector<int> items = GetBattleItems();
			const bool usable[CommandCount] = { true, true, items.empty() == false, true, true };
			for (int c = 0; c < CommandCount; c++)
			{
				UI::SetText(EntityOf(m_CommandNames[(size_t)c]), labels[c]);
				UI::SetText(EntityOf(m_CommandCosts[(size_t)c]),
					c == (int)BattleCommand::Items ? "x" + std::to_string(items.size()) : std::string());
				UI::SetInteractable(EntityOf(m_Commands[(size_t)c]), usable[c]);
			}
			UI::SetText(EntityOf(m_CommandHeader), member.GetStats().Name);
			UI::SetText(EntityOf(m_CommandBoost),
				action.Boost > 0 ? "Boost " + std::to_string(action.Boost) + "   (E / Q)" : std::string("E  Boost"));
			UI::SetText(EntityOf(m_CommandHelp), GetCommandHelp(std::clamp(m_HelpCommand, 0, CommandCount - 1)));

			std::array<bool, SubRows> subUsable = {};
			if (m_SubMenu != SubMenu::None)
			{
				PresentSub(member, items, subUsable);
				const int title = m_SubMenu == SubMenu::Abilities ? (int)BattleCommand::Abilities : (int)BattleCommand::Items;
				UI::SetText(EntityOf(m_SubTitle), labels[title]);
				UI::SetText(EntityOf(m_SubHelp), GetSubHelp(member, std::clamp(m_HelpSub, 0, SubRows - 1), items));
			}

			// One row per enemy still standing, in slot order (the others are hidden), with a letter when two share a name.
			for (int e = 0; e < TargetRows; e++)
			{
				const int enemy = actors.FindInSlot(Side::Enemy, e);
				const bool standing = actors.IsAlive(enemy);
				BattleEntities::SetVisible(EntityOf(m_TargetButtons[(size_t)e]), standing);
				if (standing)
				{
					UI::SetText(EntityOf(m_TargetNames[(size_t)e]), actors.GetTargetName(enemy));
					UI::SetText(EntityOf(m_TargetHP[(size_t)e]), "HP " + std::to_string(actors.GetHP(enemy)));
				}
			}
			if (m_Targeting)
			{
				UI::SetText(EntityOf(m_TargetHeader),
					m_TargetCommand == BattleCommand::Skill ? member.GetSkill(action.Slot).Name : labels[0]);
			}

			const bool showCommands = choosing && m_Targeting == false && m_SubMenu == SubMenu::None;
			const bool showSub = choosing && m_Targeting == false && m_SubMenu != SubMenu::None;
			const bool showTargets = choosing && m_Targeting;
			ShowPanel(m_SubPanel, showSub, m_SubShown);
			ShowPanel(m_CommandPanel, showCommands, m_CommandsShown);
			ShowPanel(m_TargetPanel, showTargets, m_TargetsShown);
			// Opening a list, or another menu took the focus away: put it on the hero's last command or the enemy under the cursor.
			if (choosing && (bool)UI::GetFocused(*m_Scene) == false)
			{
				if (m_Targeting)
				{
					UI::Focus(EntityOf(TargetButtonOf(actors, action.Target)));
				}
				else if (m_SubMenu != SubMenu::None)
				{
					int row = std::clamp(m_HelpSub, 0, SubRows - 1);
					if (subUsable[(size_t)row] == false)
					{
						row = (int)(std::find(subUsable.begin(), subUsable.end(), true) - subUsable.begin()) % SubRows;
					}
					UI::Focus(EntityOf(m_SubButtons[(size_t)row]));
				}
				else
				{
					const int last = std::clamp(member.LastCommand, 0, CommandCount - 1);
					UI::Focus(EntityOf(m_Commands[(size_t)(usable[last] ? last : 0)]));
				}
			}
		}

		bool IsTargeting() const { return m_Targeting; }

	private:
		enum class SubMenu : uint8_t { None, Abilities, Items };

		// A damaging ability or Attack picks its enemy first.
		void OpenTargets(BattleCommand command, BattleAction& action, const Combatants& actors)
		{
			m_Targeting = true;
			m_TargetCommand = command;
			action.Target = actors.FirstLiving(Side::Enemy, action.Target);
		}

		Result UpdateSub(BattleAction& action, const Combatants& actors, PartyMember& member)
		{
			Result result;
			const int sub = m_ChosenSub;
			const bool back = m_CancelSub;
			m_ChosenSub = -1;
			m_CancelSub = false;
			if (back)
			{
				m_SubMenu = SubMenu::None;
				return result;
			}
			if (sub < 0)
			{
				return result;
			}
			if (m_SubMenu == SubMenu::Abilities)
			{
				const AbilityDefinition& skill = member.GetSkill(sub);
				if (skill.Name.empty())
				{
					return result; // an empty slot
				}
				if (member.MP < skill.Cost)
				{
					result.Kind = Choice::NoMP;
					return result;
				}
				action.Slot = sub;
				member.LastAbility = sub;
				if (skill.Heals == false)
				{
					OpenTargets(BattleCommand::Skill, action, actors);
					return result;
				}
				m_SubMenu = SubMenu::None;
				result.Kind = Choice::Use;
				result.Command = BattleCommand::Skill;
				return result;
			}
			const std::vector<int> items = GetBattleItems();
			if (sub >= (int)items.size())
			{
				return result;
			}
			action.Item = Inventory::Get().Slots[(size_t)items[(size_t)sub]].Item;
			m_SubMenu = SubMenu::None;
			result.Kind = Choice::Use;
			result.Command = BattleCommand::Item;
			return result;
		}

		void PresentSub(const PartyMember& member, const std::vector<int>& items, std::array<bool, SubRows>& usable)
		{
			const Inventory& inventory = Inventory::Get();
			for (int r = 0; r < SubRows; r++)
			{
				std::string name;
				std::string cost;
				bool shown = true;
				if (m_SubMenu == SubMenu::Abilities)
				{
					const AbilityDefinition& skill = member.GetSkill(r);
					name = skill.Name.empty() ? std::string("-") : skill.Name;
					cost = skill.Name.empty() ? std::string() : std::to_string(skill.Cost) + " MP";
					usable[(size_t)r] = skill.Name.empty() == false && member.MP >= skill.Cost;
				}
				else if (r < (int)items.size())
				{
					const Inventory::Slot& slot = inventory.Slots[(size_t)items[(size_t)r]];
					const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
					name = item.IsValid() ? item->Name : std::string("?");
					cost = "x" + std::to_string(slot.Count);
					usable[(size_t)r] = true;
				}
				else
				{
					shown = false;
				}
				Entity row = EntityOf(m_SubButtons[(size_t)r]);
				BattleEntities::SetVisible(row, shown);
				UI::SetText(EntityOf(m_SubNames[(size_t)r]), name);
				UI::SetText(EntityOf(m_SubCosts[(size_t)r]), cost);
				UI::SetInteractable(row, usable[(size_t)r]);
			}
		}

		void ShowPanel(entt::entity panel, bool show, bool& shown)
		{
			if (show != shown)
			{
				shown = show;
				UI::SetVisible(EntityOf(panel), show);
			}
		}

		entt::entity TargetButtonOf(const Combatants& actors, int enemy) const
		{
			const int slot = actors.IsValid(enemy) ? actors.Get(enemy).Slot : 0;
			return m_TargetButtons[(size_t)std::clamp(slot, 0, TargetRows - 1)];
		}

		// Lines of the run's inventory that restore HP or MP, as indices into Inventory::Slots, at most SubRows.
		static std::vector<int> GetBattleItems()
		{
			std::vector<int> found;
			const std::vector<Inventory::Slot>& slots = Inventory::Get().Slots;
			for (int i = 0; i < (int)slots.size() && (int)found.size() < SubRows; i++)
			{
				const DataView<ItemDefinition> item = slots[(size_t)i].Count > 0 ? Data::GetRow<ItemDefinition>(slots[(size_t)i].Item)
					: DataView<ItemDefinition>();
				if (item.IsValid() && item->Kind == ItemKind::Consumable && (item->RestoreHP > 0 || item->RestoreMP > 0))
				{
					found.push_back(i);
				}
			}
			return found;
		}

		static std::string GetCommandHelp(int command)
		{
			switch ((BattleCommand)command)
			{
			case BattleCommand::Attack:
				return "Strike one foe with your weapon. Boost: more blows.";
			case BattleCommand::Abilities:
				return "Use one of your abilities (Abilities in the menu sets them).";
			case BattleCommand::Items:
				return "Use an item from the bag on the most hurt ally.";
			case BattleCommand::Guard:
				return "Brace yourself: less damage until your next turn.";
			case BattleCommand::Run:
				return "Try to get away. Works half the time.";
			default:
				return "";
			}
		}

		std::string GetSubHelp(const PartyMember& member, int row, const std::vector<int>& items) const
		{
			if (m_SubMenu == SubMenu::Abilities)
			{
				const AbilityDefinition& skill = member.GetSkill(row);
				if (skill.Name.empty())
				{
					return "An empty slot.";
				}
				if (skill.Description.empty() == false)
				{
					return skill.Description;
				}
				if (skill.Heals)
				{
					return "Restore the most hurt ally's HP.";
				}
				return std::string(GetDamageTypeName(skill.Type)) + " damage to one foe. Boost: stronger.";
			}
			if (row >= (int)items.size())
			{
				return "";
			}
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(Inventory::Get().Slots[(size_t)items[(size_t)row]].Item);
			return item.IsValid() ? item->Description : std::string();
		}

		static const char* GetDamageTypeName(DamageType type)
		{
			switch (type)
			{
			case DamageType::Slash: return "Slashing";
			case DamageType::Pierce: return "Piercing";
			case DamageType::Blunt: return "Blunt";
			case DamageType::Fire: return "Fire";
			case DamageType::Light: return "Light";
			default: return "No";
			}
		}

		// Pressed this frame. Keys don't count while a UI canvas captures the keyboard, but are still tracked so a held key
		// doesn't fire when it closes.
		bool Pressed(int index, KeyCode key)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && m_Previous[(size_t)index] == false && UI::CapturesInput(*m_Scene) == false;
			m_Previous[(size_t)index] = down;
			return pressed;
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		std::array<entt::entity, CommandCount> m_Commands = {};
		std::array<entt::entity, CommandCount> m_CommandNames = {};
		std::array<entt::entity, CommandCount> m_CommandCosts = {};
		entt::entity m_CommandPanel = entt::null;
		entt::entity m_CommandHeader = entt::null;
		entt::entity m_CommandBoost = entt::null;
		entt::entity m_CommandHelp = entt::null;
		entt::entity m_TargetPanel = entt::null;
		entt::entity m_TargetHeader = entt::null;
		std::array<entt::entity, TargetRows> m_TargetButtons = {};
		std::array<entt::entity, TargetRows> m_TargetNames = {};
		std::array<entt::entity, TargetRows> m_TargetHP = {};
		entt::entity m_SubPanel = entt::null;
		entt::entity m_SubTitle = entt::null;
		entt::entity m_SubHelp = entt::null;
		std::array<entt::entity, SubRows> m_SubButtons = {};
		std::array<entt::entity, SubRows> m_SubNames = {};
		std::array<entt::entity, SubRows> m_SubCosts = {};
		int m_HelpCommand = 0;              // the focused command, for the help line
		int m_HelpSub = 0;                  // the focused row, for the help line
		int m_ChosenCommand = -1;           // submitted by the menu, used by the next Update
		int m_ChosenSub = -1;               // submitted by the sub-menu
		int m_ChosenTarget = -1;            // submitted by the target list
		bool m_CancelSub = false;           // back to the commands
		bool m_CancelTarget = false;        // back to where the target list was opened from
		bool m_Targeting = false;           // the target list is open for m_TargetCommand
		BattleCommand m_TargetCommand = BattleCommand::Attack;
		SubMenu m_SubMenu = SubMenu::None;
		bool m_CommandsShown = false;
		bool m_TargetsShown = false;
		bool m_SubShown = false;
		std::array<bool, 6> m_Previous = {};
	};
}
