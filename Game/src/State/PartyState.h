#pragma once

#include "Hydro.h"
#include "State/BattleData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

// The party: who has joined, in order, and what each member keeps between battles (run state, on the game session).

namespace Hydro
{
	// One hero who has joined. Everything is looked up by its hero ID, so a new hero in Heroes.hytable shifts no one.
	struct PartyMember
	{
		static constexpr int AbilitySlotCount = 8;
		static constexpr int EquipSlotCount = 6;

		std::string Hero;   // its row of Heroes.hytable
		int HP = 0;
		int MP = 0;
		int Level = 1;
		int Experience = 0; // in total, since level 1
		std::vector<DataRowRef> Abilities; // known, in the order learned
		// Set in the field menu's Abilities page; a newly learned ability takes the first empty one. Empty: an unset row.
		std::array<DataRowRef, AbilitySlotCount> AbilitySlots;
		// Empty: an unset row. Equipped items are not in the inventory (Rules/PartyRules.h moves them).
		std::array<DataRowRef, EquipSlotCount> Equipment;
		int LastCommand = 0; // where its battle command list opens
		int LastAbility = 0; // the ability slot it used last
		bool Reserve = false; // waits out of the line-up (the field menu's Party page)

		bool IsStanding() const { return HP > 0; }
		const HeroDefinition& GetData() const { return BattleData::HeroData(Hero); }
		// At its level, with what it wears.
		HeroStats GetStats() const { return BattleData::StatsWith(Hero, Level, Equipment); }

		// An empty slot gives the defaults (an empty Name).
		const AbilityDefinition& GetSkill(int slot) const
		{
			static const AbilityDefinition none;
			return slot >= 0 && slot < AbilitySlotCount ? BattleData::Ability(AbilitySlots[(size_t)slot]) : none;
		}
	};

	// The line-up first, in battle order, then the reserve; the first GetFighterCount() fight. One per run.
	struct PartyState
	{
		// Only while a game runs.
		static PartyState& Get();

		// The party slots in Battle.hyscene (scripts/game/battle/party_slots.py) and the most in the line-up;
		// Party.hydef's BattleSize is at most this.
		static constexpr int MaxFighters = 4;

		std::vector<PartyMember> Members;
		bool Started = false; // the starting members joined (the data isn't loaded when the program starts)

		// Only the first time: Party.hydef's starting members at level 1, with full HP and MP.
		void Start()
		{
			if (Started)
			{
				return;
			}
			Started = true;
			for (const DataRowRef& hero : BattleData::Party().StartingMembers)
			{
				if (hero.IsSet() && Find(hero.Row) == nullptr)
				{
					PartyMember member = MakeMember(hero.Row, 1);
					member.Reserve = IsLineupFull();
					Add(std::move(member));
				}
			}
			if (Members.empty())
			{
				HY_WARN("Party: Party.hydef has no starting members");
			}
		}

		void Restore()
		{
			for (PartyMember& member : Members)
			{
				const HeroStats stats = member.GetStats();
				member.HP = stats.MaxHP;
				member.MP = stats.MaxMP;
			}
		}

		PartyMember* Find(const std::string& hero)
		{
			auto found = std::find_if(Members.begin(), Members.end(), [&hero](const PartyMember& member) { return member.Hero == hero; });
			return found != Members.end() ? &*found : nullptr;
		}

		const PartyMember* Find(const std::string& hero) const
		{
			return const_cast<PartyState*>(this)->Find(hero);
		}

		bool Has(const std::string& hero) const { return Find(hero) != nullptr; }

		// Clamped into the party; with no one in it, a blank stand-in (a view drawn before the party started).
		PartyMember& GetMember(int index)
		{
			if (Members.empty())
			{
				static PartyMember none;
				none = PartyMember{};
				return none;
			}
			return Members[(size_t)std::clamp(index, 0, GetSize() - 1)];
		}

		const PartyMember& GetMember(int index) const { return const_cast<PartyState*>(this)->GetMember(index); }
		int GetSize() const { return (int)Members.size(); }

		// The most the line-up holds: Party.hydef's BattleSize, at most MaxFighters.
		static int GetLineupLimit() { return std::clamp(BattleData::Party().BattleSize, 1, MaxFighters); }

		// Members out of the reserve; they come first in Members.
		int GetLineupSize() const
		{
			return (int)std::count_if(Members.begin(), Members.end(), [](const PartyMember& member) { return member.Reserve == false; });
		}

		int GetFighterCount() const { return std::min(GetLineupSize(), GetLineupLimit()); }
		bool IsFighter(int index) const { return index >= 0 && index < GetFighterCount(); }
		bool IsLineupFull() const { return GetLineupSize() >= GetLineupLimit(); }

		// Into the line-up at its end (false when it is full), or out to the reserve (false for the last one in it).
		bool SetReserve(int index, bool reserve)
		{
			if (index < 0 || index >= GetSize() || Members[(size_t)index].Reserve == reserve)
			{
				return false;
			}
			if ((reserve && GetLineupSize() <= 1) || (reserve == false && IsLineupFull()))
			{
				return false;
			}
			PartyMember member = Members[(size_t)index];
			Members.erase(Members.begin() + index);
			member.Reserve = reserve;
			HY_INFO("Party: {} {}", member.GetStats().Name, reserve ? "waits in reserve" : "joins the line-up");
			Add(std::move(member));
			return true;
		}

		// Where a member now sits in Members (it moves when it changes sides); -1 when gone.
		int IndexOf(const std::string& hero) const
		{
			for (int m = 0; m < GetSize(); m++)
			{
				if (Members[(size_t)m].Hero == hero)
				{
					return m;
				}
			}
			return -1;
		}

		// Joins at the back, at the party's highest level, knowing what its levels so far teach, with full HP and MP.
		// False when already in or not in Heroes.hytable.
		bool Join(const std::string& hero)
		{
			Start();
			if (Has(hero) || BattleData::HeroData(hero).Name.empty())
			{
				return false;
			}
			int level = 1;
			for (const PartyMember& member : Members)
			{
				level = std::max(level, member.Level);
			}
			PartyMember member = MakeMember(hero, level);
			member.Reserve = IsLineupFull();
			HY_INFO("Party: {} joins at level {}{}", member.GetStats().Name, level, member.Reserve ? ", in reserve" : "");
			Add(std::move(member));
			return true;
		}

		// For a guest who only fought one battle. The starting members always stay; a reserve member steps into a gap.
		bool Leave(const std::string& hero)
		{
			for (const DataRowRef& starter : BattleData::Party().StartingMembers)
			{
				if (starter.Row == hero)
				{
					return false;
				}
			}
			const auto found = std::find_if(Members.begin(), Members.end(),
				[&hero](const PartyMember& member) { return member.Hero == hero; });
			if (found == Members.end())
			{
				return false;
			}
			Members.erase(found);
			for (int m = 0; m < GetSize() && GetLineupSize() == 0; m++)
			{
				SetReserve(m, false);
			}
			return true;
		}

		// A won battle's experience: the fighters still standing get it all, the reserve Party.hydef's share of it.
		std::vector<std::string> GainBattleExperience(int amount)
		{
			const int reserve = BattleRules::ShareOf(amount, BattleData::Party().ReserveExperience);
			std::vector<std::string> messages;
			for (int m = 0; m < GetSize(); m++)
			{
				AddExperience(Members[(size_t)m], IsFighter(m) ? amount : reserve, messages);
			}
			return messages;
		}

		// Every member still standing, fighting or not (a quest's reward). Returns a message per level up.
		std::vector<std::string> GainExperience(int amount)
		{
			std::vector<std::string> messages;
			for (PartyMember& member : Members)
			{
				AddExperience(member, amount, messages);
			}
			return messages;
		}

	private:
		// The line-up stays first: a member of it goes after the last one in it, a reserve member at the end.
		void Add(PartyMember member)
		{
			if (member.Reserve)
			{
				Members.push_back(std::move(member));
				return;
			}
			Members.insert(Members.begin() + GetLineupSize(), std::move(member));
		}

		// The experience of that level's mark, the abilities up to it in the battle slots, nothing worn, full HP and MP.
		static PartyMember MakeMember(const std::string& hero, int level)
		{
			PartyMember member;
			member.Hero = hero;
			const HeroDefinition& data = member.GetData();
			const std::vector<int>& curve = BattleData::ExperienceToLevel();
			member.Level = std::max(1, level);
			member.Experience = member.Level >= 2 && member.Level - 2 < (int)curve.size() ? curve[(size_t)(member.Level - 2)] : 0;
			member.Abilities = data.StartingAbilities;
			for (const LearnedAbility& learned : data.Learns)
			{
				if (learned.Level <= member.Level && learned.Ability.IsSet() && Knows(member, learned.Ability) == false)
				{
					member.Abilities.push_back(learned.Ability);
				}
			}
			for (size_t a = 0; a < member.Abilities.size() && a < (size_t)PartyMember::AbilitySlotCount; a++)
			{
				member.AbilitySlots[a] = member.Abilities[a];
			}
			const HeroStats stats = member.GetStats();
			member.HP = stats.MaxHP;
			member.MP = stats.MaxMP;
			return member;
		}

		static bool Knows(const PartyMember& member, const DataRowRef& ability)
		{
			return std::find(member.Abilities.begin(), member.Abilities.end(), ability) != member.Abilities.end();
		}

		// A level up raises the maximums (and HP and MP by as much) and teaches its abilities.
		static void AddExperience(PartyMember& member, int amount, std::vector<std::string>& messages)
		{
			if (member.IsStanding() == false || amount <= 0)
			{
				return;
			}
			const std::vector<int>& curve = BattleData::ExperienceToLevel();
			member.Experience += amount;
			while (member.Level - 1 < (int)curve.size() && member.Experience >= curve[(size_t)(member.Level - 1)])
			{
				const HeroStats before = BattleData::StatsAt(member.Hero, member.Level);
				member.Level++;
				const HeroStats after = BattleData::StatsAt(member.Hero, member.Level);
				member.HP = std::min(after.MaxHP, member.HP + std::max(0, after.MaxHP - before.MaxHP));
				member.MP = std::min(after.MaxMP, member.MP + std::max(0, after.MaxMP - before.MaxMP));
				std::string message = after.Name + " reached level " + std::to_string(member.Level) + "!";
				for (const LearnedAbility& learned : member.GetData().Learns)
				{
					if (learned.Level == member.Level && learned.Ability.IsSet() && Knows(member, learned.Ability) == false)
					{
						member.Abilities.push_back(learned.Ability);
						// Into the first empty battle slot, so it can be used at once.
						for (DataRowRef& slot : member.AbilitySlots)
						{
							if (slot.IsSet() == false)
							{
								slot = learned.Ability;
								break;
							}
						}
						message += " Learned " + BattleData::Ability(learned.Ability).Name + ".";
					}
				}
				HY_INFO("Party: {}", message);
				messages.push_back(message);
			}
		}
	};
}
