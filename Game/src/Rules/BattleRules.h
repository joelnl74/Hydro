#pragma once

#include "Data/BattleDefinitions.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace Hydro
{
	// The battle's rules as plain functions on data and numbers: no scene, no UI and no randomness of their own. Random
	// draws (0 to 1) are passed in, so a test can fix them.
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
			int WeakSlot = -1; // which of the target's first three weaknesses it hit, or -1
		};

		// `stat` times `power`, 1.5x on a weakness (one of the first three), 0.5x when resisted (wins over a weakness), 1.5x on a
		// critical, 2x while broken, then a 0.9 to 1.1 spread. At least 1.
		static HitResult ResolveHit(const EnemyDefinition& target, DamageType type, float stat, float power, bool broken, float criticalRoll,
			float spreadRoll)
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

		// The ability's power times the healer's Magic times the boost, with a 0.9 to 1.1 spread.
		static int ResolveHeal(float power, int magic, float boost, float spreadRoll)
		{
			return std::max(0, (int)std::lround(power * (float)magic * boost * (0.9f + 0.2f * spreadRoll)));
		}

		// Attack times the attack's power, with a 0.85 to 1.25 spread; halved (at least 1) when the target guards.
		static int EnemyAttackDamage(int attack, float power, bool guarding, float spreadRoll)
		{
			int damage = (int)std::lround((float)attack * power * (0.85f + 0.4f * spreadRoll));
			if (guarding)
			{
				damage = std::max(1, damage / 2);
			}
			return damage;
		}

		static bool IsBroken(int brokenUntilRound, int round)
		{
			return brokenUntilRound >= 0 && brokenUntilRound >= round;
		}

		static constexpr float EscapeChance = 0.5f;
		static constexpr int MaxBoost = 3;

		// A boosted skill or heal: +60% per point spent.
		static float BoostMultiplier(int boost)
		{
			return 1.0f + 0.6f * (float)std::max(0, boost);
		}

		// A boosted Attack strikes once more per point spent.
		static int AttackHits(int boost)
		{
			return 1 + std::max(0, boost);
		}

		// Fire and light skills strike with the better of Magic and Attack, the rest with Attack.
		static float SkillStat(DamageType type, int attack, int magic)
		{
			return type == DamageType::Light || type == DamageType::Fire ? (float)std::max(magic, attack) : (float)attack;
		}

		// A point back every round after the first, unless the hero boosted in the round before.
		static int RegainMomentum(int momentum, bool boostedLastRound, int round, int maxMomentum)
		{
			return boostedLastRound == false && round > 1 ? std::min(maxMomentum, momentum + 1) : momentum;
		}

		// One of `count` by a roll of 0 to 1 (the last for a roll of 1); -1 when there are none.
		static int PickByRoll(int count, float roll)
		{
			return count <= 0 ? -1 : std::clamp((int)(roll * (float)count), 0, count - 1);
		}

		// A share of a reward, rounded (the reserve's experience).
		static int ShareOf(int amount, float share)
		{
			return (int)std::lround((float)amount * std::clamp(share, 0.0f, 1.0f));
		}

		// Speeds plus up to 3 of luck (`random` once per actor, in order), fastest first. Returns indices into `speeds`; a
		// negative speed leaves an actor out.
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

		// By weight (the plain attack weighs 1); `roll` is 0 to 1. Returns the ability's row.
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

		// [0] of the curve is the total for level 2.
		static int LevelFor(const std::vector<int>& experienceToLevel, int experience)
		{
			int level = 1;
			while (level - 1 < (int)experienceToLevel.size() && experience >= experienceToLevel[(size_t)(level - 1)])
			{
				level++;
			}
			return level;
		}

		// Base + GrowthPerLevel for every level above 1 (HP at least 1, the rest 0).
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
