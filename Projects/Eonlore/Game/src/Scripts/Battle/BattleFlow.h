#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Rules/BattleRules.h"
#include "Scripts/Battle/Combatants.h"
#include "State/BattleState.h"
#include "State/Inventory.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

// The battle's course on the model: rounds, the turn order, whose turn it is and what an action does to the numbers.
// No entities: BattleScript times it to the animations and shows what it returns.

namespace Hydro
{
	// The command list's rows (Attack to Run), then the kinds of action.
	enum class BattleCommand : uint8_t { Attack = 0, Abilities, Items, Guard, Run, Skill, Item, EnemyAttack };

	// One turn's action: who, what and at whom (IDs into Combatants).
	struct BattleAction
	{
		int Actor = -1;
		BattleCommand Kind = BattleCommand::Attack;
		int Target = -1;  // an enemy for strikes, a hero for heals, items and enemy attacks
		int Slot = 0;     // the ability slot of a skill
		DataRowRef Item;
		int Boost = 0;
		int Hits = 1;
		int HitsDone = 0;
	};

	class BattleFlow
	{
	public:
		enum class Phase : uint8_t { Intro, NextTurn, Command, EnemyThink, Acting, Victory, Defeat, Fleeing, Done };
		// What NextTurn found: a hero to command, an enemy to act, or nothing this frame (the next frame goes on).
		enum class TurnStart : uint8_t { Nothing, Hero, Enemy, Ended };
		enum class UseResult : uint8_t { Begun, NoMP, NoItem, Escaped, NoEscape };

		struct Hit
		{
			int Target = -1;
			int Damage = 0;
			bool Weak = false;     // on a weakness while not broken (the shield took it)
			bool Resisted = false;
			bool Critical = false;
			bool Broke = false;    // the shield broke with it
			bool Defeated = false;
			bool Strong = false;   // on a weakness (broken or not) or critical: bigger impact effects
		};

		struct EnemyStrike
		{
			int Target = -1;
			int Damage = 0;
			bool Guarded = false;
			bool Fell = false;
		};

		BattleFlow(Combatants& actors, std::function<float()> random)
			: m_Actors(actors), m_Random(std::move(random))
		{
		}

		Phase GetPhase() const { return m_Phase; }
		float GetPhaseTime() const { return m_PhaseTime; }
		void AddTime(float deltaTime) { m_PhaseTime += deltaTime; }
		int GetRound() const { return m_Round; }
		int GetTurn() const { return m_Turn; }
		const std::vector<int>& GetOrder() const { return m_Order; }
		BattleAction& GetAction() { return m_Action; }
		const BattleAction& GetAction() const { return m_Action; }
		bool IsOver() const
		{
			return m_Phase == Phase::Victory || m_Phase == Phase::Defeat || m_Phase == Phase::Fleeing || m_Phase == Phase::Done;
		}

		// Counts every start of a phase, the same one again included (a new turn), so views can tell.
		void SetPhase(Phase phase)
		{
			m_Phase = phase;
			m_PhaseTime = 0.0f;
			m_PhaseCount++;
		}

		int GetPhaseCount() const { return m_PhaseCount; }

		// Boost points back, broken shields mended, a new turn order of everyone standing.
		void StartRound()
		{
			m_Round++;
			for (Combatant& combatant : m_Actors.GetAll())
			{
				if (combatant.IsHero())
				{
					combatant.Momentum = BattleRules::RegainMomentum(combatant.Momentum, combatant.BoostedLastRound, m_Round,
						BattleState::MaxMomentum);
					combatant.BoostedLastRound = false;
				}
				else if (m_Actors.IsAlive(combatant.Id) && combatant.BrokenUntilRound >= 0 && m_Round > combatant.BrokenUntilRound)
				{
					combatant.BrokenUntilRound = -1;
					combatant.Shield = Combatants::EnemyOf(combatant).Shield;
					HY_INFO("Battle: {} recovers its shield", m_Actors.GetName(combatant.Id));
				}
			}
			std::vector<int> speeds;
			for (const Combatant& combatant : m_Actors.GetAll())
			{
				speeds.push_back(m_Actors.IsAlive(combatant.Id) ? std::max(0, m_Actors.GetSpeed(combatant.Id)) : -1);
			}
			m_Order = BattleRules::TurnOrder(speeds, m_Random);
			m_Turn = -1;
			HY_INFO("Battle: round {}", m_Round);
			SetPhase(Phase::NextTurn);
		}

		// The end when a side is down, else the next actor in the order (a new round after the last).
		TurnStart NextTurn()
		{
			if (m_Actors.AnyAlive(Side::Enemy) == false)
			{
				HY_INFO("Battle: victory!");
				SetPhase(Phase::Victory);
				return TurnStart::Ended;
			}
			if (m_Actors.AnyAlive(Side::Party) == false)
			{
				HY_INFO("Battle: the party is defeated...");
				SetPhase(Phase::Defeat);
				return TurnStart::Ended;
			}

			m_Turn++;
			if (m_Turn >= (int)m_Order.size())
			{
				StartRound();
				return TurnStart::Nothing;
			}
			const int actor = m_Order[(size_t)m_Turn];
			if (m_Actors.IsAlive(actor) == false)
			{
				return TurnStart::Nothing;
			}

			Combatant& combatant = m_Actors.Get(actor);
			m_Action = BattleAction{};
			m_Action.Actor = actor;
			if (combatant.IsHero())
			{
				combatant.Guarding = false;
				m_Action.Target = m_Actors.FirstLiving(Side::Enemy, m_LastTarget);
				SetPhase(Phase::Command);
				return TurnStart::Hero;
			}
			if (m_Actors.IsBroken(actor, m_Round))
			{
				HY_INFO("Battle: {} is broken and cannot act", m_Actors.GetName(actor));
				return TurnStart::Nothing;
			}
			m_Action.Kind = BattleCommand::EnemyAttack;
			SetPhase(Phase::EnemyThink);
			return TurnStart::Enemy;
		}

		const AbilityDefinition& GetActionSkill() const
		{
			return Combatants::MemberOf(m_Actors.Get(m_Action.Actor)).GetSkill(m_Action.Slot);
		}

		bool IsHealing() const { return m_Action.Kind == BattleCommand::Skill && GetActionSkill().Heals; }

		// The weapon's for Attack, the skill's for a skill.
		DamageType GetActionDamageType() const
		{
			return m_Action.Kind == BattleCommand::Attack ? m_Actors.GetHeroStats(m_Action.Actor).WeaponType : GetActionSkill().Type;
		}

		// Pays for the hero's command and starts it: the MP, the item, the boost; a heal or an item goes to the most hurt
		// hero. Run ends the battle or the turn here.
		UseResult Use(BattleCommand command)
		{
			const int actor = m_Action.Actor;
			if (command == BattleCommand::Skill)
			{
				const AbilityDefinition& skill = GetActionSkill();
				if (m_Actors.GetMP(actor) < skill.Cost)
				{
					return UseResult::NoMP;
				}
				m_Actors.SetMP(actor, m_Actors.GetMP(actor) - skill.Cost);
				if (skill.Heals)
				{
					m_Action.Target = m_Actors.MostHurtHero();
				}
			}
			else if (command == BattleCommand::Item)
			{
				if (Inventory::Get().Remove(m_Action.Item, 1) == false)
				{
					return UseResult::NoItem;
				}
				m_Action.Target = m_Actors.MostHurtHero();
			}
			else if (command == BattleCommand::Run)
			{
				if (m_Random() < BattleRules::EscapeChance)
				{
					HY_INFO("Battle: the party got away");
					SetPhase(Phase::Fleeing);
					return UseResult::Escaped;
				}
				HY_INFO("Battle: could not escape");
				SetPhase(Phase::NextTurn);
				return UseResult::NoEscape;
			}

			m_Action.Kind = command;
			Combatant& hero = m_Actors.Get(actor);
			if (m_Action.Boost > 0)
			{
				hero.Momentum -= m_Action.Boost;
				hero.BoostedLastRound = true;
			}
			if (command == BattleCommand::Guard)
			{
				hero.Guarding = true;
			}
			m_Action.Hits = command == BattleCommand::Attack ? BattleRules::AttackHits(m_Action.Boost) : 1;
			m_Action.HitsDone = 0;
			m_LastTarget = m_Action.Target;
			SetPhase(Phase::Acting);
			return UseResult::Begun;
		}

		// A standing hero at random.
		void BeginEnemyAction()
		{
			const std::vector<int> heroes = m_Actors.GetLiving(Side::Party);
			const int pick = BattleRules::PickByRoll((int)heroes.size(), m_Random());
			m_Action.Target = pick >= 0 ? heroes[(size_t)pick] : -1;
			m_Action.Hits = 1;
			SetPhase(Phase::Acting);
		}

		// The hero's strike lands: a heal on its target, or a hit on its enemy (the next standing one when it fell).
		// Returns the hit; Target -1 when there was no one to hit, Damage the HP healed for a heal (-1: fallen).
		Hit LandStrike()
		{
			Hit hit;
			const HeroStats hero = m_Actors.GetHeroStats(m_Action.Actor);
			const float boost = BattleRules::BoostMultiplier(m_Action.Boost);
			m_Action.HitsDone++;
			if (IsHealing())
			{
				hit.Target = m_Action.Target;
				hit.Damage = Heal(m_Action.Target, BattleRules::ResolveHeal(GetActionSkill().Power, hero.Magic, boost, m_Random()));
				return hit;
			}
			m_Action.Target = m_Actors.FirstLiving(Side::Enemy, m_Action.Target);
			if (m_Action.Target < 0)
			{
				return hit;
			}
			const DamageType type = GetActionDamageType();
			if (m_Action.Kind == BattleCommand::Attack)
			{
				return HitEnemy(m_Action.Target, type, (float)hero.Attack, 1.0f);
			}
			return HitEnemy(m_Action.Target, type, BattleRules::SkillStat(type, hero.Attack, hero.Magic), GetActionSkill().Power * boost);
		}

		// A weakness takes a shield point (none while broken); the last one breaks it until the end of the next round.
		Hit HitEnemy(int target, DamageType type, float stat, float power)
		{
			Combatant& enemy = m_Actors.Get(target);
			const EnemyDefinition& definition = Combatants::EnemyOf(enemy);
			const bool broken = m_Actors.IsBroken(target, m_Round);
			// The rolls in a fixed order: the critical, then the spread.
			const float criticalRoll = m_Random();
			const float spreadRoll = m_Random();
			const BattleRules::HitResult rolled = BattleRules::ResolveHit(definition, type, stat, power, broken, criticalRoll, spreadRoll);
			if (rolled.WeakSlot >= 0)
			{
				BattleState::Get().Revealed[enemy.Row][(size_t)rolled.WeakSlot] = true;
			}
			m_Actors.SetHP(target, m_Actors.GetHP(target) - rolled.Damage);

			Hit hit;
			hit.Target = target;
			hit.Damage = rolled.Damage;
			hit.Weak = rolled.Weak && broken == false;
			hit.Resisted = rolled.Resisted && hit.Weak == false;
			hit.Critical = rolled.Critical;
			hit.Defeated = m_Actors.GetHP(target) <= 0;
			hit.Strong = rolled.Weak || rolled.Critical;
			if (hit.Weak)
			{
				enemy.Shield = std::max(0, enemy.Shield - 1);
				if (enemy.Shield == 0 && hit.Defeated == false)
				{
					enemy.BrokenUntilRound = m_Round + 1;
					hit.Broke = true;
					HY_INFO("Battle: {} is BROKEN!", definition.Name);
				}
			}
			HY_INFO("Battle: {} takes {}{}{}{} ({} HP left, shield {})", definition.Name, hit.Damage, rolled.Weak ? " (weak)" : "",
				rolled.Resisted ? " (resisted)" : "", hit.Critical ? " (critical)" : "", m_Actors.GetHP(target), enemy.Shield);
			if (hit.Defeated)
			{
				HY_INFO("Battle: {} is defeated", definition.Name);
			}
			return hit;
		}

		// HP restored to a standing hero; -1 for a fallen one, who gets none.
		int Heal(int hero, int amount)
		{
			if (m_Actors.IsAlive(hero) == false)
			{
				return -1;
			}
			const int before = m_Actors.GetHP(hero);
			m_Actors.SetHP(hero, before + amount);
			HY_INFO("Battle: {} recovers {} HP", m_Actors.GetName(hero), m_Actors.GetHP(hero) - before);
			return m_Actors.GetHP(hero) - before;
		}

		// An item's MP, fallen or not.
		void RestoreMP(int hero, int amount)
		{
			m_Actors.SetMP(hero, m_Actors.GetMP(hero) + amount);
		}

		// The enemy's attack on its target. Enemies only strike for now: an ability's other effects are not applied yet.
		EnemyStrike StrikeHero()
		{
			EnemyStrike strike;
			strike.Target = m_Action.Target;
			m_Action.HitsDone = 1;
			if (m_Actors.IsAlive(strike.Target) == false)
			{
				return strike;
			}
			const EnemyDefinition& attacker = Combatants::EnemyOf(m_Actors.Get(m_Action.Actor));
			const DataRowRef action = attacker.Abilities.empty() ? attacker.BasicAttack : BattleRules::ChooseEnemyAction(attacker,
				m_Random());
			const float power = action.IsSet() ? BattleData::Ability(action).Power : 1.0f;
			strike.Guarded = m_Actors.Get(strike.Target).Guarding;
			strike.Damage = BattleRules::EnemyAttackDamage(attacker.Stats.Attack, power, strike.Guarded, m_Random());
			m_Actors.SetHP(strike.Target, m_Actors.GetHP(strike.Target) - strike.Damage);
			strike.Fell = m_Actors.GetHP(strike.Target) <= 0;
			if (strike.Fell)
			{
				HY_INFO("Battle: {} falls!", m_Actors.GetName(strike.Target));
			}
			HY_INFO("Battle: {} hits {} for {}{}", attacker.Name, m_Actors.GetName(strike.Target), strike.Damage, strike.Guarded
				? " (guarded)" : "");
			return strike;
		}

	private:
		Combatants& m_Actors;
		std::function<float()> m_Random;
		Phase m_Phase = Phase::Intro;
		float m_PhaseTime = 0.0f;
		int m_PhaseCount = 0;
		int m_Round = 0;
		int m_Turn = -1;
		std::vector<int> m_Order;
		BattleAction m_Action;
		int m_LastTarget = -1; // the last hero's target, kept for the next hero
	};
}
