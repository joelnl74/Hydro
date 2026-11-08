#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "State/BattleData.h"
#include "State/BattleState.h"
#include "State/PartyState.h"

#include <algorithm>
#include <string>
#include <vector>

// Who fights in a battle: one list of heroes and enemies with stable IDs, no entities (the stage shows them).

namespace Hydro
{
	enum class Side : uint8_t { Party, Enemy };

	// A hero keeps its HP and MP on its party member, so they carry over between battles; an enemy keeps them here.
	struct Combatant
	{
		int Id = 0;               // its index in Combatants, never reused in a battle
		Side Team = Side::Party;
		int Slot = 0;             // its place on the stage: "Party Slot <n>" or "Enemy <n>"
		int Member = -1;          // a hero's index into PartyState::Members
		std::string Row;          // an enemy's row of Enemies.hytable
		int HP = 0;               // an enemy's
		int Shield = 0;
		int BrokenUntilRound = -1; // broken while the round is at most this
		int Momentum = 1;          // a hero's boost points
		bool BoostedLastRound = false;
		bool Guarding = false;
		bool Gone = false;         // left the battle; its ID stays taken

		bool IsHero() const { return Team == Side::Party; }
	};

	class Combatants
	{
	public:
		// Joins at the end of the list (mid-battle too); returns its ID.
		int AddHero(int member, int slot)
		{
			Combatant hero;
			hero.Id = (int)m_List.size();
			hero.Team = Side::Party;
			hero.Slot = slot;
			hero.Member = member;
			m_List.push_back(hero);
			return hero.Id;
		}

		int AddEnemy(const std::string& row, int slot)
		{
			Combatant enemy;
			enemy.Id = (int)m_List.size();
			enemy.Team = Side::Enemy;
			enemy.Slot = slot;
			enemy.Row = row;
			const EnemyDefinition& definition = BattleData::Enemy(row);
			enemy.HP = definition.Stats.MaxHP;
			enemy.Shield = definition.Shield;
			m_List.push_back(enemy);
			return enemy.Id;
		}

		int GetCount() const { return (int)m_List.size(); }
		bool IsValid(int id) const { return id >= 0 && id < GetCount(); }
		// Clamped into the list (ask IsValid first where it matters); with no one in it, a blank stand-in.
		Combatant& Get(int id)
		{
			if (m_List.empty())
			{
				static Combatant none;
				none = Combatant{};
				return none;
			}
			return m_List[(size_t)std::clamp(id, 0, GetCount() - 1)];
		}

		const Combatant& Get(int id) const { return const_cast<Combatants*>(this)->Get(id); }
		const std::vector<Combatant>& GetAll() const { return m_List; }
		std::vector<Combatant>& GetAll() { return m_List; }

		// -1 when no one of that side stands in it.
		int FindInSlot(Side side, int slot) const
		{
			for (const Combatant& combatant : m_List)
			{
				if (combatant.Team == side && combatant.Slot == slot && combatant.Gone == false)
				{
					return combatant.Id;
				}
			}
			return -1;
		}

		static PartyMember& MemberOf(const Combatant& hero) { return PartyState::Get().GetMember(hero.Member); }
		static const EnemyDefinition& EnemyOf(const Combatant& enemy) { return BattleData::Enemy(enemy.Row); }

		// A hero's numbers with what it wears.
		HeroStats GetHeroStats(int id) const { return MemberOf(Get(id)).GetStats(); }

		int GetHP(int id) const
		{
			const Combatant& combatant = Get(id);
			return combatant.IsHero() ? MemberOf(combatant).HP : combatant.HP;
		}

		void SetHP(int id, int hp)
		{
			Combatant& combatant = Get(id);
			const int clamped = std::clamp(hp, 0, GetMaxHP(id));
			if (combatant.IsHero())
			{
				MemberOf(combatant).HP = clamped;
			}
			else
			{
				combatant.HP = clamped;
			}
		}

		int GetMaxHP(int id) const
		{
			const Combatant& combatant = Get(id);
			return std::max(1, combatant.IsHero() ? GetHeroStats(id).MaxHP : EnemyOf(combatant).Stats.MaxHP);
		}

		// Enemies have no MP.
		int GetMP(int id) const
		{
			const Combatant& combatant = Get(id);
			return combatant.IsHero() ? MemberOf(combatant).MP : 0;
		}

		void SetMP(int id, int mp)
		{
			Combatant& combatant = Get(id);
			if (combatant.IsHero())
			{
				MemberOf(combatant).MP = std::clamp(mp, 0, GetHeroStats(id).MaxMP);
			}
		}

		int GetSpeed(int id) const
		{
			const Combatant& combatant = Get(id);
			return combatant.IsHero() ? GetHeroStats(id).Speed : EnemyOf(combatant).Stats.Speed;
		}

		std::string GetName(int id) const
		{
			const Combatant& combatant = Get(id);
			return combatant.IsHero() ? GetHeroStats(id).Name : EnemyOf(combatant).Name;
		}

		// The target list's name: the short name, with a letter when two enemies share it ("Slime A").
		std::string GetTargetName(int id) const
		{
			const Combatant& enemy = Get(id);
			const EnemyDefinition& definition = EnemyOf(enemy);
			std::string name = definition.ShortName.empty() ? definition.Name : definition.ShortName;
			int same = 0;
			int before = 0;
			for (const Combatant& other : m_List)
			{
				if (other.Team == Side::Enemy && other.Row == enemy.Row && other.Row.empty() == false)
				{
					same++;
					before += other.Slot < enemy.Slot ? 1 : 0;
				}
			}
			if (same > 1)
			{
				name += std::string(" ") + (char)('A' + before);
			}
			return name;
		}

		bool IsAlive(int id) const
		{
			if (IsValid(id) == false)
			{
				return false;
			}
			const Combatant& combatant = Get(id);
			if (combatant.Gone || (combatant.IsHero() == false && combatant.Row.empty()))
			{
				return false;
			}
			return GetHP(id) > 0;
		}

		bool IsBroken(int id, int round) const { return BattleRules::IsBroken(Get(id).BrokenUntilRound, round); }

		bool AnyAlive(Side side) const
		{
			for (const Combatant& combatant : m_List)
			{
				if (combatant.Team == side && IsAlive(combatant.Id))
				{
					return true;
				}
			}
			return false;
		}

		// Standing ones of a side in slot order.
		std::vector<int> GetLiving(Side side) const
		{
			std::vector<int> living;
			for (const Combatant& combatant : m_List)
			{
				if (combatant.Team == side && IsAlive(combatant.Id))
				{
					living.push_back(combatant.Id);
				}
			}
			std::sort(living.begin(), living.end(), [this](int a, int b) { return Get(a).Slot < Get(b).Slot; });
			return living;
		}

		// `preferred` when it still stands, else the first standing one in slot order; -1 when none stands.
		int FirstLiving(Side side, int preferred) const
		{
			if (IsAlive(preferred) && Get(preferred).Team == side)
			{
				return preferred;
			}
			const std::vector<int> living = GetLiving(side);
			return living.empty() ? -1 : living.front();
		}

		// The next standing one along the slots (wrapping), as they stand on the stage.
		int Step(Side side, int current, int direction) const
		{
			const std::vector<int> living = GetLiving(side);
			if (living.empty())
			{
				return current;
			}
			const auto found = std::find(living.begin(), living.end(), current);
			const int at = found != living.end() ? (int)(found - living.begin()) : 0;
			const int count = (int)living.size();
			return living[(size_t)(((at + direction) % count + count) % count)];
		}

		// The standing hero with the lowest share of their HP; -1 when none stands.
		int MostHurtHero() const
		{
			int best = -1;
			float lowest = 2.0f;
			for (int id : GetLiving(Side::Party))
			{
				const float fraction = (float)GetHP(id) / (float)GetMaxHP(id);
				if (fraction < lowest)
				{
					lowest = fraction;
					best = id;
				}
			}
			return best;
		}

	private:
		std::vector<Combatant> m_List;
	};
}
