#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Scripts/Battle/ActionSequence.h"
#include "Scripts/Battle/BattleActors.h"
#include "Scripts/Battle/BattleFlow.h"
#include "Scripts/Battle/BattleOverlay.h"
#include "Scripts/Battle/BattleResults.h"
#include "Scripts/Battle/BattleStage.h"
#include "Scripts/Battle/BattleText.h"
#include "Scripts/Battle/Combatants.h"
#include "Scripts/Battle/CommandMenuView.h"
#include "Scripts/Battle/PartyStatusView.h"
#include "Scripts/Battle/StrikeEffects.h"
#include "Scripts/Battle/TurnOrderView.h"
#include "State/BattleState.h"
#include "State/PartyState.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

// The party battle (Battle.hyscene) started by enemies in the field scenes. It conducts: the fighters and enemies
// (Combatants) go through BattleFlow, timed to the animations, and the stage and views show them (docs/Scripting.md).

namespace Hydro
{
	HY_SCRIPT()
	class BattleScript : public ScriptableEntity
	{
	public:
		// The stage camera. scripts/game/battle/build_arena.py baked the scene with these values (keep its CAMERA in step).
		HY_PROPERTY(Range(0, 89), Tooltip("Camera tilt in degrees from straight down (the field's is 50 to 70)"))
		float CameraTilt = 73.0f;
		HY_PROPERTY(Range(5, 90), Tooltip("Vertical field of view in degrees"))
		float CameraFieldOfView = 30.0f;
		HY_PROPERTY(Tooltip("The world point at the centre of the screen"))
		glm::vec3 CameraFocus{ 0.0f, 5.5f, 0.0f };
		HY_PROPERTY(Range(1, 100), Tooltip("Metres seen from the bottom of the screen to the top at the focus point"))
		float ViewHeight = 10.0f;

		// The heroes' strikes: walk up to the target, strike on the clip's contact frame, walk back.
		HY_PROPERTY(Range(0.5, 30), Tooltip("Metres per second walking up to the target"))
		float WalkSpeed = 4.5f;
		HY_PROPERTY(Range(0.5, 30), Tooltip("Metres per second walking back"))
		float ReturnSpeed = 5.5f;
		HY_PROPERTY(Range(0, 5), Tooltip("Metres right of the target where she stops to strike"))
		float StrikeDistance = 1.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("Seconds into the Punch clip when the fist lands (Miko: its last frame, 0.5 s at 10 fps)"))
		float PunchImpactTime = 0.5f;
		HY_PROPERTY(Range(0, 2), Tooltip("Seconds into the Kick clip when the foot lands (Miko: frame 4, 0.4 s at 10 fps)"))
		float KickImpactTime = 0.4f;
		HY_PROPERTY(Range(0, 3), Tooltip("Length of the Punch clip in seconds; the next strike or the walk back starts after it"))
		float PunchLength = 0.65f;
		HY_PROPERTY(Range(0, 3), Tooltip("Length of the Kick clip in seconds"))
		float KickLength = 0.75f;
		HY_PROPERTY(Range(0, 3), Tooltip("Camera shake on impacts (0 = none)"))
		float ShakeStrength = 1.0f;
		HY_PROPERTY(Tooltip("When the Battle scene is played on its own (not pushed by a field scene), every hero fights, so the whole party can be tested"))
		bool FullPartyWhenAlone = true;

	protected:
		void OnCreate() override
		{
			PartyState& party = PartyState::Get();
			party.Start(); // the Battle scene played on its own
			if (FullPartyWhenAlone && SceneTransition::GetDepth() == 0)
			{
				for (const std::string& id : BattleData::HeroIds())
				{
					party.Join(id);
				}
			}

			Scene& scene = *GetEntity().GetScene();
			m_Stage.Bind(scene);
			m_Stage.Update(GetStageSettings(), 0.0f, 0.0f);
			m_Stage.CaptureLayout();
			m_Actors.Bind(scene, m_Stage);

			// The fighters take the party slots in party order (a fallen one gets up for the battle); the enemies theirs.
			for (int m = 0; m < party.GetFighterCount(); m++)
			{
				party.Members[(size_t)m].HP = std::max(1, party.Members[(size_t)m].HP);
				m_Combatants.AddHero(m, m);
			}
			const std::vector<std::string>& encounter = BattleState::Get().Encounter;
			int enemies = 0;
			for (int e = 0; e < BattleState::MaxEnemies && e < (int)encounter.size(); e++)
			{
				if (encounter[(size_t)e].empty() == false)
				{
					m_Combatants.AddEnemy(encounter[(size_t)e], e);
					enemies++;
				}
			}
			m_Actors.Dress(m_Combatants);

			m_Status.Bind(scene);
			m_TurnOrder.Bind(scene);
			m_Menu.Bind(scene);
			m_Overlay.Bind(scene, m_Stage);
			m_Effects.Bind(scene, m_Stage);
			HY_INFO("Battle: {} heroes, {} enemies! (Up/Down + Enter or click: command, then target; Esc back; E/Q boost)",
				party.GetFighterCount(), enemies);
			m_Flow.SetPhase(BattleFlow::Phase::Intro);
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;
			m_DeltaTime = deltaTime;
			m_Stage.Update(GetStageSettings(), m_Time, deltaTime);
			m_Flow.AddTime(deltaTime);
			m_Overlay.Tick(deltaTime);
			m_Actors.Tick(deltaTime);

			const float phaseTime = m_Flow.GetPhaseTime();
			switch (m_Flow.GetPhase())
			{
				case BattleFlow::Phase::Intro:
					if (phaseTime >= 1.0f)
					{
						m_Flow.StartRound();
					}
					break;
				case BattleFlow::Phase::NextTurn:
					if (m_Flow.NextTurn() == BattleFlow::TurnStart::Ended)
					{
						OnEnded();
					}
					break;
				case BattleFlow::Phase::Command:
					UpdateCommand();
					break;
				case BattleFlow::Phase::EnemyThink:
					if (phaseTime >= 0.45f)
					{
						m_Flow.BeginEnemyAction();
					}
					break;
				case BattleFlow::Phase::Acting:
					UpdateAction();
					break;
				case BattleFlow::Phase::Victory:
					if (m_NextResult < (int)m_Results.size() && phaseTime >= ResultTime * (float)(m_NextResult + 1))
					{
						m_Overlay.ShowMessage(m_Results[(size_t)m_NextResult], ResultTime);
						m_NextResult++;
					}
					if (phaseTime >= std::max(2.0f, ResultTime * (float)(m_Results.size() + 1)))
					{
						Finish();
					}
					break;
				case BattleFlow::Phase::Defeat:
				case BattleFlow::Phase::Fleeing:
					if (phaseTime >= (m_Flow.GetPhase() == BattleFlow::Phase::Fleeing ? 0.9f : 2.0f))
					{
						Finish();
					}
					break;
				case BattleFlow::Phase::Done:
					break;
			}

			// A new phase or turn starts on the commands.
			if (m_Flow.GetPhaseCount() != m_PhasesSeen)
			{
				m_PhasesSeen = m_Flow.GetPhaseCount();
				m_Menu.ResetChoice();
			}
			Present();
		}

		bool OnUIEvent(const UIEvent& event) override
		{
			return m_Menu.HandleEvent(event, m_Flow.GetPhase() == BattleFlow::Phase::Command, m_Flow.GetAction(), m_Combatants);
		}

	private:
		BattleStage::Settings GetStageSettings() const
		{
			BattleStage::Settings settings;
			settings.Tilt = CameraTilt;
			settings.FieldOfView = CameraFieldOfView;
			settings.Focus = CameraFocus;
			settings.ViewHeight = ViewHeight;
			settings.ShakeStrength = ShakeStrength;
			return settings;
		}

		// ---- Turns ---------------------------------------------------------------------------------------

		void UpdateCommand()
		{
			const CommandMenuView::Result result = m_Menu.Update(m_Flow.GetAction(), m_Combatants);
			if (result.Kind == CommandMenuView::Choice::NoMP)
			{
				m_Overlay.ShowMessage(BattleLabel::NoMP);
			}
			else if (result.Kind == CommandMenuView::Choice::Use)
			{
				UseCommand(result.Command);
			}
		}

		void UseCommand(BattleCommand command)
		{
			switch (m_Flow.Use(command))
			{
				case BattleFlow::UseResult::NoMP:
					m_Overlay.ShowMessage(BattleLabel::NoMP);
					return;
				case BattleFlow::UseResult::NoItem:
					m_Overlay.ShowMessage(BattleLabel::NoPotions);
					return;
				case BattleFlow::UseResult::Escaped:
					m_Overlay.ShowMessage(BattleLabel::Escaped);
					return;
				case BattleFlow::UseResult::NoEscape:
					m_Overlay.ShowMessage(BattleLabel::NoEscape);
					return;
				case BattleFlow::UseResult::Begun:
					BeginHeroAction();
					return;
			}
		}

		// Spells (Holy Palm) strike from where the hero stands; Attack and the strike skills walk up to the target first.
		void BeginHeroAction()
		{
			const BattleAction& action = m_Flow.GetAction();
			const Entity sprite = m_Actors.SpriteOf(m_Combatants.Get(action.Actor));
			const HeroStats hero = m_Combatants.GetHeroStats(action.Actor);
			m_Sequence.Reset();
			switch (action.Kind)
			{
				case BattleCommand::Attack:
					BeginApproach();
					break;
				case BattleCommand::Skill:
				{
					const AbilityDefinition& skill = m_Flow.GetActionSkill();
					m_Overlay.ShowMessage(skill.Name, 0.9f);
					if (skill.Heals == false && skill.Approach)
					{
						BeginApproach();
					}
					else
					{
						BeginStrike();
					}
					break;
				}
				case BattleCommand::Item:
				{
					SpriteAnimator::Play(sprite, hero.ItemClip, true);
					const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(action.Item);
					m_Overlay.ShowMessage(item.IsValid() ? item->Name : std::string(GetLabelText(BattleLabel::Potion)), 0.9f);
					break;
				}
				case BattleCommand::Guard:
					SpriteAnimator::Play(sprite, hero.IdleClip, true);
					m_Overlay.ShowMessage(BattleLabel::Guarding, 0.7f);
					break;
				default:
					break;
			}
		}

		void UpdateAction()
		{
			const BattleAction& action = m_Flow.GetAction();
			const float phaseTime = m_Flow.GetPhaseTime();
			if (m_Combatants.Get(action.Actor).IsHero())
			{
				if (action.Kind == BattleCommand::Attack || action.Kind == BattleCommand::Skill)
				{
					UpdateHeroStrikes();
					return;
				}
				if (action.Kind == BattleCommand::Item && action.HitsDone == 0 && phaseTime >= ItemTime)
				{
					UseItem();
				}
				// An item's clip plays out; a guard only takes a moment.
				if (phaseTime >= (action.Kind == BattleCommand::Item ? 1.0f : 0.6f))
				{
					EndAction();
				}
				return;
			}

			if (action.HitsDone == 0 && phaseTime >= 0.45f)
			{
				const BattleFlow::EnemyStrike strike = m_Flow.StrikeHero();
				if (m_Combatants.IsValid(strike.Target))
				{
					BattleActors::Actor& target = m_Actors.Of(m_Combatants.Get(strike.Target));
					target.Flash = 0.35f;
					m_Overlay.SpawnPopup(m_Stage.ScreenAbove(target.Base, BattleActors::HeroPopupHeight), strike.Damage, glm::vec4(1.0f));
					m_Effects.PlayBlow(target.Base, strike.Guarded);
					if (strike.Fell)
					{
						SpriteAnimator::Play(m_Actors.SpriteOf(m_Combatants.Get(strike.Target)), "Death", true);
					}
				}
			}
			if (phaseTime >= 1.0f)
			{
				EndAction();
			}
		}

		// The item restores its MP and the HP of its target, if standing.
		void UseItem()
		{
			BattleAction& action = m_Flow.GetAction();
			action.HitsDone = 1;
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(action.Item);
			if (item.IsValid() == false)
			{
				return;
			}
			if (item->RestoreMP > 0)
			{
				m_Flow.RestoreMP(action.Target, item->RestoreMP);
			}
			if (item->RestoreHP > 0)
			{
				ShowHeal(action.Target, m_Flow.Heal(action.Target, item->RestoreHP));
			}
		}

		// The acting hero back in their stance, everyone standing too, and the next turn.
		void EndAction()
		{
			const int actor = m_Flow.GetAction().Actor;
			const Combatant& combatant = m_Combatants.Get(actor);
			if (combatant.IsHero())
			{
				m_Actors.Of(combatant).Offset = glm::vec2(0.0f);
				if (m_Combatants.IsAlive(actor))
				{
					SpriteAnimator::Play(m_Actors.SpriteOf(combatant), m_Combatants.GetHeroStats(actor).IdleClip);
				}
			}
			m_Actors.PlayIdle(m_Combatants, actor);
			m_Flow.SetPhase(BattleFlow::Phase::NextTurn);
		}

		// ---- The heroes' strikes ---------------------------------------------------------------------------

		// Attack alternates punches and kicks; a skill strikes with its clip's move.
		StrikeMove MoveFor(int strike) const
		{
			if (m_Flow.GetAction().Kind == BattleCommand::Attack)
			{
				return strike % 2 == 0 ? StrikeMove::Punch : StrikeMove::Kick;
			}
			return m_Flow.GetActionSkill().Clip == "Kick" ? StrikeMove::Kick : StrikeMove::Punch;
		}

		float ImpactTime(StrikeMove move) const { return move == StrikeMove::Punch ? PunchImpactTime : KickImpactTime; }
		float StrikeLength(StrikeMove move) const { return move == StrikeMove::Punch ? PunchLength : KickLength; }

		BattleActors::Actor& ActingHero() { return m_Actors.Of(m_Combatants.Get(m_Flow.GetAction().Actor)); }

		// StrikeDistance to the target's right, on the floor.
		glm::vec2 StrikeOffset(int target)
		{
			const glm::vec3 enemy = m_Stage.GroundPoint(m_Actors.Of(m_Combatants.Get(target)).Base);
			return m_Stage.LayoutOf(enemy + glm::vec3(StrikeDistance, 0.0f, 0.0f)) - ActingHero().Base;
		}

		// Measured on the floor; `speed` in metres per second.
		float WalkTime(const glm::vec2& from, const glm::vec2& to, float speed)
		{
			const glm::vec2 base = ActingHero().Base;
			const float metres = glm::length(m_Stage.GroundPoint(base + to) - m_Stage.GroundPoint(base + from));
			return metres / std::max(speed, 0.1f);
		}

		void BeginApproach()
		{
			BattleActors::Actor& hero = ActingHero();
			const glm::vec2 to = StrikeOffset(m_Flow.GetAction().Target);
			m_Sequence.BeginApproach(hero.Offset, to, WalkTime(hero.Offset, to, WalkSpeed));
			SpriteAnimator::Play(Entity{ hero.Sprite, GetEntity().GetScene() }, to.x - hero.Offset.x <= 0.0f ? "WalkLeft" : "WalkRight",
				true);
			m_Effects.PlayStepDust(hero.Base + hero.Offset);
		}

		// A sheet without the skill's own clip falls back to the move's. The strike is timed and drawn as the move after
		// the clip's (on Attack the first punch lands with the kick's timing), as the battle has always done.
		void BeginStrike()
		{
			const BattleAction& action = m_Flow.GetAction();
			const Entity sprite{ ActingHero().Sprite, GetEntity().GetScene() };
			const char* moveClip = MoveFor(m_Sequence.GetStrikes()) == StrikeMove::Punch ? "Punch" : "Kick";
			const std::string clip = action.Kind == BattleCommand::Skill ? m_Flow.GetActionSkill().Clip : std::string(moveClip);
			if (SpriteAnimator::Play(sprite, clip, true) == false)
			{
				SpriteAnimator::Play(sprite, moveClip, true);
			}
			m_StrikeMove = MoveFor(m_Sequence.GetStrikes() + 1);
			const float impact = ImpactTime(m_StrikeMove);
			const float lead = StrikeEffects::EffectLead(m_StrikeMove, m_Flow.GetActionDamageType());
			m_Sequence.BeginStrike(impact - lead, impact, StrikeLength(m_StrikeMove));
		}

		void BeginReturn()
		{
			BattleActors::Actor& hero = ActingHero();
			const float duration = WalkTime(hero.Offset, glm::vec2(0.0f), ReturnSpeed);
			m_Sequence.BeginReturn(hero.Offset, duration);
			if (duration > 0.01f)
			{
				SpriteAnimator::Play(Entity{ hero.Sprite, GetEntity().GetScene() }, hero.Offset.x > 0.0f ? "WalkLeft" : "WalkRight", true);
			}
		}

		void UpdateHeroStrikes()
		{
			BattleAction& action = m_Flow.GetAction();
			BattleActors::Actor& hero = ActingHero();
			const ActionSequence::Events events = m_Sequence.Update(m_DeltaTime, hero.Offset);
			if (events.Arrived)
			{
				m_Effects.PlayStepDust(hero.Base + hero.Offset);
				BeginStrike();
				return;
			}
			if (events.EffectDue && m_Flow.IsHealing() == false)
			{
				const int target = m_Combatants.FirstLiving(Side::Enemy, action.Target);
				if (target >= 0)
				{
					const BattleActors::Actor& enemy = m_Actors.Of(m_Combatants.Get(target));
					m_Effects.PlayStrike(m_StrikeMove, m_Flow.GetActionDamageType(), enemy.Base, enemy.Size);
				}
			}
			if (events.Impact)
			{
				LandStrike();
			}
			if (events.StrikeOver)
			{
				const bool moreHits = action.Kind == BattleCommand::Attack && action.HitsDone < action.Hits;
				const int next = m_Combatants.FirstLiving(Side::Enemy, action.Target);
				if (moreHits && next >= 0 && next != action.Target)
				{
					action.Target = next;
					BeginApproach();
				}
				else if (moreHits && next >= 0)
				{
					BeginStrike();
				}
				else
				{
					BeginReturn();
				}
			}
			if (events.Returned)
			{
				EndAction();
			}
		}

		// Weak and Break, Resist or Critical, the number, the defeat puff, and the impact (bigger when strong).
		void LandStrike()
		{
			const BattleFlow::Hit hit = m_Flow.LandStrike();
			if (m_Flow.IsHealing())
			{
				ShowHeal(hit.Target, hit.Damage);
				return;
			}
			if (hit.Target < 0)
			{
				return;
			}
			glm::vec4 color{ 1.0f };
			if (hit.Weak)
			{
				color = { 1.0f, 0.7f, 0.25f, 1.0f };
				if (hit.Broke)
				{
					m_Overlay.ShowMessage(BattleLabel::Break);
				}
				else
				{
					m_Overlay.ShowMessage(BattleLabel::Weak, 0.6f);
				}
			}
			else if (hit.Resisted)
			{
				color = { 0.6f, 0.7f, 0.85f, 1.0f };
				m_Overlay.ShowMessage(BattleLabel::Resist, 0.6f);
			}
			else if (hit.Critical)
			{
				m_Overlay.ShowMessage(BattleLabel::Critical, 0.6f);
			}
			const Combatant& target = m_Combatants.Get(hit.Target);
			BattleActors::Actor& enemy = m_Actors.Of(target);
			enemy.Flash = 0.3f;
			m_Overlay.SpawnPopup(m_Stage.ScreenAbove(enemy.Base, BattleActors::EnemyPopupHeight), hit.Damage, color);
			if (hit.Defeated)
			{
				m_Effects.PlayDefeat(enemy.Base, Combatants::EnemyOf(target));
			}
			const bool skill = m_Flow.GetAction().Kind != BattleCommand::Attack;
			m_Effects.PlayImpact(m_StrikeMove, m_Flow.GetActionDamageType(), enemy.Base, enemy.Size, hit.Strong, skill);
		}

		// A green number and the heal's sprite and motes; nothing for a fallen hero (`amount` -1).
		void ShowHeal(int hero, int amount)
		{
			if (amount < 0 || m_Combatants.IsValid(hero) == false)
			{
				return;
			}
			const BattleActors::Actor& actor = m_Actors.Of(m_Combatants.Get(hero));
			m_Overlay.SpawnPopup(m_Stage.ScreenAbove(actor.Base, BattleActors::HeroPopupHeight), amount, glm::vec4(0.45f, 1.0f, 0.55f,
				1.0f));
			m_Effects.PlayHeal(actor.Base);
		}

		// ---- The end ------------------------------------------------------------------------------------------

		void OnEnded()
		{
			if (m_Flow.GetPhase() == BattleFlow::Phase::Victory)
			{
				m_Overlay.ShowMessage(BattleLabel::Victory);
				m_Results = BattleResults::Give(m_Combatants, [this]() { return Random01(); });
				m_NextResult = 0;
			}
			else
			{
				m_Overlay.ShowMessage(BattleLabel::Defeated);
			}
		}

		// Tells the field how it went and goes back to it. A lost battle restores the party; no one leaves with 0 HP.
		void Finish()
		{
			BattleState& battle = BattleState::Get();
			if (m_Flow.GetPhase() == BattleFlow::Phase::Victory)
			{
				battle.Outcome = BattleState::Result::Won;
			}
			else if (m_Flow.GetPhase() == BattleFlow::Phase::Defeat)
			{
				PartyState::Get().Restore();
				battle.Outcome = BattleState::Result::Lost;
			}
			else
			{
				battle.Outcome = BattleState::Result::Fled;
			}
			for (PartyMember& member : PartyState::Get().Members)
			{
				member.HP = std::max(1, member.HP);
			}
			m_Flow.SetPhase(BattleFlow::Phase::Done);
			SceneTransition::Pop();
		}

		// ---- Presentation ---------------------------------------------------------------------------------

		void Present()
		{
			const bool choosing = m_Flow.GetPhase() == BattleFlow::Phase::Command;
			m_Actors.Present(m_Combatants, m_Flow, m_Time, m_DeltaTime, m_Stage);
			m_Status.Present(m_Combatants, m_Flow, m_Time);
			m_Menu.Present(m_Flow.GetAction(), m_Combatants, choosing);
			m_TurnOrder.Present(m_Flow, m_Combatants, m_Actors);
			m_Overlay.Present(m_Flow, m_Combatants, m_Actors, m_Menu.IsTargeting(), m_Time);
			m_Effects.Present(m_DeltaTime);
			m_Stage.ApplyLayout();
		}

		float Random01() { return (float)(m_Random() & 0xFFFFFF) / (float)0x1000000; }

		// An item is used this far into its clip; each victory line shows this long.
		static constexpr float ItemTime = 0.35f;
		static constexpr float ResultTime = 1.3f;

		std::mt19937 m_Random{ std::random_device{}() };
		Combatants m_Combatants;
		BattleFlow m_Flow{ m_Combatants, [this]() { return Random01(); } };
		ActionSequence m_Sequence;
		StrikeMove m_StrikeMove = StrikeMove::Punch; // the move the current strike is timed and drawn as
		BattleStage m_Stage;
		BattleActors m_Actors;
		StrikeEffects m_Effects;
		CommandMenuView m_Menu;
		PartyStatusView m_Status;
		TurnOrderView m_TurnOrder;
		BattleOverlay m_Overlay;
		float m_Time = 0.0f;
		float m_DeltaTime = 0.0f;
		int m_PhasesSeen = 0;
		std::vector<std::string> m_Results;
		int m_NextResult = 0;
	};
}
