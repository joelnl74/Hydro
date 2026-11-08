#pragma once

#include "Hydro.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Battle/BattleFlow.h"
#include "Scripts/Battle/BattleStage.h"
#include "Scripts/Battle/BattleText.h"
#include "Scripts/Battle/Combatants.h"
#include "State/BattleState.h"
#include "State/PartyState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// The combatants on the stage: the party slots ("Party Slot <n>") and enemy slots ("Enemy <n>") of Battle.hyscene,
// dressed from data, placed, tinted and moved each frame.

namespace Hydro
{
	class BattleActors
	{
	public:
		// The feet's height in a cell (fraction from the bottom) and the sheet's pivot, for BattleStage::StandOffset.
		// Keep in step with scripts/game/battle/build_arena.py.
		static constexpr float HeroFoot = 15.0f / 96.0f;
		static constexpr float HeroPivot = 0.5f;
		static constexpr float EnemyFoot = 4.0f / 32.0f;
		static constexpr float EnemyPivot = 0.125f;
		// Metres above the floor.
		static constexpr float HeroPopupHeight = 1.7f;
		static constexpr float HeroMarkerHeight = 2.05f;
		static constexpr float EnemyPopupHeight = 1.35f;
		static constexpr float EnemyCursorHeight = 1.5f;

		// A slot's entities and how it moves.
		struct Actor
		{
			entt::entity Sprite = entt::null;
			entt::entity Shadow = entt::null; // the contact shadow on the floor
			entt::entity Light = entt::null;  // an enemy's glow
			// An enemy's info: a World canvas pinned under its feet.
			entt::entity Info = entt::null, HPFill = entt::null, ShieldIcon = entt::null, ShieldCount = entt::null, WeakLabel = entt::null;
			std::array<entt::entity, 3> WeakIcons = { entt::null, entt::null, entt::null };
			glm::vec2 Base{ 0.0f };   // where it stands, in layout units
			glm::vec2 Offset{ 0.0f }; // from Base while a hero walks to a target and back
			float Size = 1.0f;        // sprite height in metres
			float Flash = 0.0f;       // seconds left of the hit flash
			float Fade = 1.0f;        // 1 alive, falls to 0 when an enemy is defeated
		};

		// After the stage's first Update, so the slots' places map to layout units.
		void Bind(Scene& scene, const BattleStage& stage)
		{
			m_Scene = &scene;
			for (int s = 0; s < PartyState::MaxFighters; s++)
			{
				const std::string prefix = "Party Slot " + std::to_string(s);
				Actor& hero = m_PartySlots[(size_t)s];
				hero.Sprite = BattleEntities::Find(scene, prefix);
				hero.Shadow = BattleEntities::Find(scene, prefix + " Shadow");
				hero.Base = stage.ActorLayout(hero.Sprite, HeroFoot, HeroPivot);
				hero.Size = SizeOf(hero.Sprite);
			}
			for (int s = 0; s < BattleState::MaxEnemies; s++)
			{
				const std::string prefix = "Enemy " + std::to_string(s);
				Actor& enemy = m_EnemySlots[(size_t)s];
				enemy.Sprite = BattleEntities::Find(scene, prefix);
				enemy.Shadow = BattleEntities::Find(scene, prefix + " Shadow");
				enemy.Light = BattleEntities::Find(scene, prefix + " Light");
				enemy.Info = BattleEntities::Find(scene, prefix + " Info");
				enemy.HPFill = BattleEntities::Find(scene, prefix + " HP Fill");
				enemy.ShieldIcon = BattleEntities::Find(scene, prefix + " Shield");
				enemy.ShieldCount = BattleEntities::Find(scene, prefix + " Shield Count");
				enemy.WeakLabel = BattleEntities::Find(scene, prefix + " Weak Label");
				for (int w = 0; w < 3; w++)
				{
					enemy.WeakIcons[(size_t)w] = BattleEntities::Find(scene, prefix + " Weak " + std::to_string(w));
				}
				enemy.Base = stage.ActorLayout(enemy.Sprite, EnemyFoot, EnemyPivot);
				enemy.Size = SizeOf(enemy.Sprite);
			}
		}

		Actor& Of(const Combatant& combatant)
		{
			return combatant.IsHero() ? m_PartySlots[(size_t)std::clamp(combatant.Slot, 0, PartyState::MaxFighters - 1)]
				: m_EnemySlots[(size_t)std::clamp(combatant.Slot, 0, BattleState::MaxEnemies - 1)];
		}

		const Actor& Of(const Combatant& combatant) const { return const_cast<BattleActors*>(this)->Of(combatant); }
		Entity SpriteOf(const Combatant& combatant) const { return Entity{ Of(combatant).Sprite, m_Scene }; }

		// Dresses the slots that have a combatant and hides the rest. The animator only plays sets already loaded, and the
		// scene names one set per slot, so each combatant's set is loaded here.
		void Dress(const Combatants& actors)
		{
			std::array<bool, PartyState::MaxFighters> heroes = {};
			std::array<bool, BattleState::MaxEnemies> enemies = {};
			for (const Combatant& combatant : actors.GetAll())
			{
				if (combatant.IsHero())
				{
					heroes[(size_t)std::clamp(combatant.Slot, 0, PartyState::MaxFighters - 1)] = true;
					const PartyMember& member = Combatants::MemberOf(combatant);
					const HeroDefinition& definition = member.GetData();
					PlayAnimations(Of(combatant).Sprite, definition.BattleAnimations, definition.IdleClip, definition.Name);
				}
				else if (combatant.Row.empty() == false)
				{
					enemies[(size_t)std::clamp(combatant.Slot, 0, BattleState::MaxEnemies - 1)] = true;
					const EnemyDefinition& definition = Combatants::EnemyOf(combatant);
					const Actor& enemy = Of(combatant);
					PlayAnimations(enemy.Sprite, definition.Animations, definition.IdleClip, definition.Name);
					if (BattleEntities::Alive(*m_Scene, enemy.Light) && m_Scene->m_Registry.has<Light2DComponent>(enemy.Light))
					{
						m_Scene->m_Registry.get<Light2DComponent>(enemy.Light).Color = definition.GlowColor;
					}
				}
			}
			// No one in it: hidden (an enemy slot's info and glow follow in Present).
			for (int s = 0; s < PartyState::MaxFighters; s++)
			{
				if (heroes[(size_t)s] == false)
				{
					BattleEntities::SetColor(*m_Scene, m_PartySlots[(size_t)s].Sprite, glm::vec4(0.0f));
					BattleEntities::SetShadowStrength(*m_Scene, m_PartySlots[(size_t)s].Shadow, 0.0f);
				}
			}
			m_EnemyUsed = enemies;
		}

		void Tick(float deltaTime)
		{
			for (Actor& actor : m_PartySlots)
			{
				actor.Flash = std::max(0.0f, actor.Flash - deltaTime);
			}
			for (Actor& actor : m_EnemySlots)
			{
				actor.Flash = std::max(0.0f, actor.Flash - deltaTime);
			}
		}

		// Every standing hero back to their stance, except `except` (-1: none).
		void PlayIdle(const Combatants& actors, int except)
		{
			for (int id : actors.GetLiving(Side::Party))
			{
				if (id != except)
				{
					SpriteAnimator::Play(SpriteOf(actors.Get(id)), actors.GetHeroStats(id).IdleClip);
				}
			}
		}

		// Places and tints everyone; an acting enemy lunges at its target. Enemies' info shows their shield (a "Break"
		// label while broken), the weaknesses found so far ("?" for the rest) and HP.
		void Present(const Combatants& actors, const BattleFlow& flow, float time, float deltaTime, const BattleStage& stage)
		{
			Scene& scene = *m_Scene;
			const BattleAction& action = flow.GetAction();
			const bool acting = flow.GetPhase() == BattleFlow::Phase::Acting;
			for (const Combatant& combatant : actors.GetAll())
			{
				if (combatant.IsHero() == false)
				{
					continue;
				}
				Actor& hero = Of(combatant);
				glm::vec2 offset = hero.Offset;
				if (hero.Flash > 0.0f)
				{
					offset.x += std::sin(time * 70.0f) * 0.2f;
				}
				stage.PlaceActor(hero.Sprite, hero.Shadow, hero.Base + offset, 0.0f, HeroFoot, HeroPivot);
				const float heat = std::clamp(hero.Flash / 0.35f, 0.0f, 1.0f);
				BattleEntities::SetColor(scene, hero.Sprite, glm::vec4(1.0f, 1.0f - 0.6f * heat, 1.0f - 0.6f * heat, 1.0f));
			}

			for (int s = 0; s < BattleState::MaxEnemies; s++)
			{
				const int id = actors.FindInSlot(Side::Enemy, s);
				const bool present = m_EnemyUsed[(size_t)s] && id >= 0;
				Actor& enemy = m_EnemySlots[(size_t)s];
				const int hp = present ? actors.GetHP(id) : 0;
				const EnemyDefinition& definition = present ? Combatants::EnemyOf(actors.Get(id)) : BattleData::Enemy("");
				if (present && hp <= 0)
				{
					enemy.Fade = std::max(0.0f, enemy.Fade - deltaTime * 2.5f);
				}

				glm::vec2 offset{ 0.0f };
				if (present && acting && action.Actor == id && actors.IsValid(action.Target))
				{
					const float t = std::clamp((flow.GetPhaseTime() - 0.2f) / 0.6f, 0.0f, 1.0f);
					const glm::vec2 goal = Of(actors.Get(action.Target)).Base + glm::vec2(-3.0f, 0.0f);
					offset = (goal - enemy.Base) * std::sin(t * 3.1415927f) * 0.85f;
				}
				if (enemy.Flash > 0.0f)
				{
					offset.x += std::sin(time * 70.0f) * 0.2f;
				}
				const float bob = definition.FloatHeight > 0.0f ? definition.FloatHeight + std::sin(time * 2.2f + (float)s) * 0.12f : 0.0f;
				BattleEntities::SetShadowStrength(scene, enemy.Shadow, present ? 0.45f * enemy.Fade : 0.0f);

				const float heat = std::clamp(enemy.Flash / 0.3f, 0.0f, 1.0f);
				const bool broken = present && actors.IsBroken(id, flow.GetRound()) && hp > 0;
				stage.PlaceActor(enemy.Sprite, enemy.Shadow, enemy.Base + offset, bob, EnemyFoot, EnemyPivot);
				const float tint = broken ? 0.65f : 1.0f;
				BattleEntities::SetColor(scene, enemy.Sprite, present
					? glm::vec4(tint + (1.0f - tint) * heat, tint * (1.0f - 0.6f * heat), tint * (1.0f - 0.6f * heat), enemy.Fade)
					: glm::vec4(0.0f));
				if (BattleEntities::Alive(scene, enemy.Light) && scene.m_Registry.has<Light2DComponent>(enemy.Light))
				{
					stage.PlaceActor(enemy.Light, entt::null, enemy.Base + offset, bob, 0.0f, 0.0f);
					scene.m_Registry.get<Light2DComponent>(enemy.Light).Intensity = present
						? definition.GlowIntensity * enemy.Fade * (0.85f + 0.15f * std::sin(time * 3.0f))
						: 0.0f;
				}

				Entity info = EntityOf(enemy.Info);
				const bool showInfo = present && hp > 0;
				BattleEntities::SetVisible(info, showInfo);
				if (showInfo == false)
				{
					continue;
				}
				if (info.IsValid() && info.HasComponent<TransformComponent>())
				{
					scene.SetWorldPosition(info, stage.GroundPoint(enemy.Base + offset));
				}
				const Combatant& combatant = actors.Get(id);
				BattleEntities::SetFill(EntityOf(enemy.HPFill), (float)hp / (float)std::max(1, definition.Stats.MaxHP),
					glm::vec4(0.86f, 0.36f, 0.28f, 1.0f));
				BattleEntities::SetUIColor(EntityOf(enemy.ShieldIcon), broken ? glm::vec4(0.75f, 0.32f, 0.28f, 1.0f) : glm::vec4(1.0f));
				UI::SetText(EntityOf(enemy.ShieldCount), broken ? std::string() : std::to_string(combatant.Shield));
				UI::SetText(EntityOf(enemy.WeakLabel), broken ? "BREAK" : "WEAK");
				BattleEntities::SetUIColor(EntityOf(enemy.WeakLabel),
					broken ? glm::vec4(1.0f, 0.5f, 0.38f, 1.0f) : glm::vec4(1.0f, 0.84f, 0.5f, 1.0f));
				for (int w = 0; w < 3; w++)
				{
					const std::vector<DamageType>& weak = definition.Weak;
					const DamageType type = w < (int)weak.size() ? weak[(size_t)w] : DamageType::None;
					const bool used = type != DamageType::None;
					const bool known = used && BattleState::Get().IsRevealed(combatant.Row, w);
					Entity icon = EntityOf(enemy.WeakIcons[(size_t)w]);
					BattleEntities::SetVisible(icon, used);
					if (icon.IsValid() && icon.HasComponent<UIImageComponent>())
					{
						icon.GetComponent<UIImageComponent>().SpriteIndex = known ? BattleIcon::ForType(type) : BattleIcon::Unknown;
					}
				}
			}
		}

	private:
		float SizeOf(entt::entity entity) const
		{
			return BattleEntities::Alive(*m_Scene, entity) ? m_Scene->m_Registry.get<TransformComponent>(entity).Scale.y : 1.0f;
		}

		// No set leaves the slot's own (with a warning); one that doesn't load is still given, as the data asks.
		void PlayAnimations(entt::entity sprite, AssetHandle animations, const std::string& idleClip, const std::string& name)
		{
			Scene& scene = *m_Scene;
			if (BattleEntities::Alive(scene, sprite) == false || scene.m_Registry.has<SpriteAnimatorComponent>(sprite) == false)
			{
				return;
			}
			if ((uint64_t)animations == 0)
			{
				HY_WARN("Battle: {} has no battle animations", name);
				return;
			}
			if (AssetManager::GetAsset<SpriteAnimationSet>(animations) == nullptr)
			{
				HY_WARN("Battle: {}'s battle animations ({}) couldn't be loaded", name, (uint64_t)animations);
			}
			SpriteAnimatorComponent& animator = scene.m_Registry.get<SpriteAnimatorComponent>(sprite);
			animator.Animations = animations;
			animator.StartClip = idleClip;
			animator.PlayOnStart = true;
			SpriteAnimator::ResetPlayback(Entity{ sprite, &scene });
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		std::array<Actor, PartyState::MaxFighters> m_PartySlots;
		std::array<Actor, BattleState::MaxEnemies> m_EnemySlots;
		std::array<bool, BattleState::MaxEnemies> m_EnemyUsed = {};
	};
}
