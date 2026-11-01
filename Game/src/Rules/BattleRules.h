// Game/src/Rules/BattleRules.h
#pragma once

#include "Data/BattleDefinitions.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace Hydro
{
	// The battle's rules as plain functions on data and numbers (Scripting plan SC8d; the Diorama RPG's J1): no scene,
	// no UI, no randomness of their own. Every random draw is passed in (0 to 1) or comes from a function the caller
	// gives, so a test can fix them. The battle script (Scripts/Battle/BattleScript.h) calls them and shows the result.
	class BattleRules
	{
	public:
		static constexpr float WeakMultiplier = 1.5f;
		static constexpr float ResistMultiplier = 0.5f;
		static constexpr float CriticalMultiplier = 1.5f;
		static constexpr float BrokenMultiplier = 2.0f;
		static constexpr float CriticalChance = 0.08f;

		struct HitResult
		{
			int Damage = 0;
			bool Weak = false;
			bool Resisted = false;
			bool Critical = false;
			int WeakSlot = -1; // which of the target's first three weaknesses it hit (revealed), or -1
		};

		// A hit on an enemy: `stat` (the attacker's Attack or Magic) times `power` (the ability's, with any boost),
		// 1.5x on a weakness (one of its first three), 0.5x when resisted (wins over a weakness), 1.5x on a critical
		// (`criticalRoll` below CriticalChance), 2x while broken, then a spread of 0.9 to 1.1 (`spreadRoll`). At
		// least 1.
		static HitResult ResolveHit(const EnemyDefinition& target, DamageType type, float stat, float power, bool broken, float criticalRoll, float spreadRoll)
		{
			HitResult result;
			float multiplier = 1.0f;
			for (int w = 0; w < std::min(3, (int)target.Weak.size()); w++)
			{
				if (target.Weak[(size_t)w] == type)
				{
					result.Weak = true;
					result.WeakSlot = w;
					multiplier = WeakMultiplier;
				}
			}
			for (DamageType resist : target.Resist)
			{
				if (resist == type)
				{
					result.Resisted = true;
					multiplier = ResistMultiplier;
				}
			}
			result.Critical = criticalRoll < CriticalChance;
			if (result.Critical)
			{
				multiplier *= CriticalMultiplier;
			}
			if (broken)
			{
				multiplier *= BrokenMultiplier;
			}
			result.Damage = std::max(1, (int)std::lround(stat * power * multiplier * (0.9f + 0.2f * spreadRoll)));
			return result;
		}

		// A heal: the ability's power times the healer's Magic times the boost, with a spread of 0.9 to 1.1.
		static int ResolveHeal(float power, int magic, float boost, float spreadRoll)
		{
			return std::max(0, (int)std::lround(power * (float)magic * boost * (0.9f + 0.2f * spreadRoll)));
		}

		// An enemy's plain attack: its Attack times the attack's power, with a spread of 0.85 to 1.25; halved (at
		// least 1) when the target guards.
		static int EnemyAttackDamage(int attack, float power, bool guarding, float spreadRoll)
		{
			int damage = (int)std::lround((float)attack * power * (0.85f + 0.4f * spreadRoll));
			if (guarding)
			{
				damage = std::max(1, damage / 2);
			}
			return damage;
		}

		// Broken while the round is at most the one the break lasts to.
		static bool IsBroken(int brokenUntilRound, int round)
		{
			return brokenUntilRound >= 0 && brokenUntilRound >= round;
		}

		// Who acts in what order this round: the actors' speeds plus up to 3 of luck (`random` once per actor, in
		// order), fastest first. Returns the actors' indices into `speeds`; a negative speed leaves an actor out.
		static std::vector<int> TurnOrder(const std::vector<int>& speeds, const std::function<float()>& random)
		{
			std::vector<std::pair<float, int>> rolled;
			for (int actor = 0; actor < (int)speeds.size(); actor++)
			{
				if (speeds[(size_t)actor] >= 0)
				{
					rolled.push_back({ (float)speeds[(size_t)actor] + 3.0f * random(), actor });
				}
			}
			std::stable_sort(rolled.begin(), rolled.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
			std::vector<int> order;
			for (const auto& entry : rolled)
			{
				order.push_back(entry.second);
			}
			return order;
		}

		// What an enemy does: its plain attack or one of its abilities, by weight (the plain attack weighs 1).
		// `roll` is 0 to 1. Returns the ability's row (the plain attack's when it has no others).
		static DataRowRef ChooseEnemyAction(const EnemyDefinition& enemy, float roll)
		{
			float total = 1.0f;
			for (const EnemyAbility& ability : enemy.Abilities)
			{
				total += ability.Ability.IsSet() ? std::max(0.0f, ability.Weight) : 0.0f;
			}
			float pick = roll * total;
			if (pick < 1.0f)
			{
				return enemy.BasicAttack;
			}
			pick -= 1.0f;
			for (const EnemyAbility& ability : enemy.Abilities)
			{
				const float weight = ability.Ability.IsSet() ? std::max(0.0f, ability.Weight) : 0.0f;
				if (pick < weight)
				{
					return ability.Ability;
				}
				pick -= weight;
			}
			return enemy.BasicAttack;
		}

		// The level a total of experience reaches on a curve ([0]: the total for level 2, ...).
		static int LevelFor(const std::vector<int>& experienceToLevel, int experience)
		{
			int level = 1;
			while (level - 1 < (int)experienceToLevel.size() && experience >= experienceToLevel[(size_t)(level - 1)])
			{
				level++;
			}
			return level;
		}

		// A hero's numbers at a level: Base + GrowthPerLevel for every level above 1 (HP at least 1, the rest 0).
		static CombatStats StatsAt(const HeroDefinition& hero, int level)
		{
			const int steps = std::max(0, level - 1);
			CombatStats stats;
			stats.MaxHP = std::max(1, hero.Base.MaxHP + steps * hero.GrowthPerLevel.MaxHP);
			stats.MaxMP = std::max(0, hero.Base.MaxMP + steps * hero.GrowthPerLevel.MaxMP);
			stats.Attack = std::max(0, hero.Base.Attack + steps * hero.GrowthPerLevel.Attack);
			stats.Magic = std::max(0, hero.Base.Magic + steps * hero.GrowthPerLevel.Magic);
			stats.Speed = std::max(0, hero.Base.Speed + steps * hero.GrowthPerLevel.Speed);
			return stats;
		}
	};
}
