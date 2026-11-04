#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Scripts/Battle/BattleEffects.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Battle/BattleStage.h"

#include <algorithm>
#include <string>

// What a strike or a heal looks like: the one-shot effect sprites ("FX Strike", "FX Heal") with a light that flares,
// the particle bursts (BattleEffects) and the camera shake.

namespace Hydro
{
	enum class StrikeMove : uint8_t { Punch, Kick };

	class StrikeEffects
	{
	public:
		void Bind(Scene& scene, BattleStage& stage)
		{
			m_Scene = &scene;
			m_Stage = &stage;
			m_Strike.Entity = BattleEntities::Find(scene, "FX Strike");
			m_Heal.Entity = BattleEntities::Find(scene, "FX Heal");
		}

		// The kick's swoosh first follows the leg over the top (4 frames at 14 fps); the punch's burst is on its second frame.
		static float EffectLead(StrikeMove move, DamageType type)
		{
			const std::string clip = StrikeClip(move, type);
			if (clip == "Kick" || clip == "Flame")
			{
				return 4.0f / 14.0f;
			}
			return clip == "Holy" ? 1.0f / 12.0f : 1.0f / 14.0f;
		}

		// Lifted towards the camera so it draws over the enemy, `size` metres tall at `ground`. The kick's burst is left of
		// its cell's centre, so the cell sits a little to the right.
		void PlayStrike(StrikeMove move, DamageType type, const glm::vec2& ground, float size)
		{
			const std::string clip = StrikeClip(move, type);
			const BattleStage::View& view = m_Stage->GetView();
			glm::vec3 position = m_Stage->Above(ground, 0.55f * size) + view.Back * 0.6f;
			float duration = 6.0f / 14.0f;
			if (clip == "Kick" || clip == "Flame")
			{
				position += view.Right * 0.5f;
				duration = 7.0f / 14.0f;
			}
			else if (clip == "Holy")
			{
				position = m_Stage->Above(ground, 0.75f * size) + view.Back * 0.6f;
				duration = 8.0f / 12.0f;
			}
			PlaySprite(m_Strike, position, clip.c_str(), duration, StrikeLightColor(type, move), 0.0f);
		}

		// Sparks, the strike light flaring, a dust kick for a kick, and the camera shake (stronger when `strong`, more for
		// a skill).
		void PlayImpact(StrikeMove move, DamageType type, const glm::vec2& ground, float size, bool strong, bool skill)
		{
			Play("Effect Hit Sparks", ground, 0.55f * size, BattleEffects::HitSparks(type, strong), ParticleSpace::View);
			if (move == StrikeMove::Kick)
			{
				Play("Effect Dust", ground, 0.05f, BattleEffects::KickDust(), ParticleSpace::Ground);
			}
			m_Strike.LightPeak = (skill ? 7.0f : 4.0f) * (strong ? 1.3f : 1.0f);
			m_Strike.LightTime = LightFlareTime;
			float shake = move == StrikeMove::Kick ? 0.09f : 0.06f;
			shake *= skill ? 1.8f : 1.0f;
			m_Stage->Shake(strong ? shake * 1.4f : shake);
		}

		// An enemy's blow on a hero.
		void PlayBlow(const glm::vec2& ground, bool guarded)
		{
			Play("Effect Hit Sparks", ground, 0.9f, BattleEffects::HitSparks(DamageType::Blunt, guarded == false), ParticleSpace::View);
			m_Stage->Shake(guarded ? 0.04f : 0.08f);
		}

		void PlayHeal(const glm::vec2& ground)
		{
			Play("Effect Heal", ground, 0.35f, BattleEffects::HealMotes(), ParticleSpace::View);
			PlaySprite(m_Heal, m_Stage->Above(ground, 0.6f) + m_Stage->GetView().Back * 0.6f, "Heal", HealTime, glm::vec3(0.55f, 1.0f,
				0.6f), 2.5f);
		}

		void PlayDefeat(const glm::vec2& ground, const EnemyDefinition& enemy)
		{
			Play("Effect Defeat Puff", ground, 0.5f, BattleEffects::DefeatPuff(enemy), ParticleSpace::View);
		}

		void PlayStepDust(const glm::vec2& ground)
		{
			Play("Effect Dust", ground, 0.1f, BattleEffects::StepDust(), ParticleSpace::Ground);
		}

		void Present(float deltaTime)
		{
			PresentSprite(m_Strike, deltaTime);
			PresentSprite(m_Heal, deltaTime);
		}

	private:
		struct SpriteEffect
		{
			entt::entity Entity = entt::null;
			float Time = 0.0f;      // seconds left
			float LightPeak = 0.0f; // the light's intensity at its flare
			float LightTime = 0.0f; // seconds left of the flare
		};

		static constexpr float LightFlareTime = 0.35f;
		static constexpr float HealTime = 0.66f; // the heal sprite's clip

		// The clip on FX Strike for a strike (Battle/FX_Strikes.hyanim, made by scripts/game/battle/fx_strikes.py).
		static const char* StrikeClip(StrikeMove move, DamageType type)
		{
			if (type == DamageType::Light)
			{
				return "Holy";
			}
			if (move == StrikeMove::Kick)
			{
				return type == DamageType::Fire ? "Flame" : "Kick";
			}
			return type == DamageType::Fire ? "Flame" : "Punch";
		}

		static glm::vec3 StrikeLightColor(DamageType type, StrikeMove move)
		{
			switch (type)
			{
				case DamageType::Fire: return { 1.0f, 0.55f, 0.2f };
				case DamageType::Light: return { 1.0f, 0.93f, 0.7f };
				default: return move == StrikeMove::Kick ? glm::vec3(0.85f, 0.95f, 1.0f) : glm::vec3(1.0f, 0.85f, 0.55f);
			}
		}

		void PlaySprite(SpriteEffect& effect, const glm::vec3& position, const char* clip, float duration, const glm::vec3& lightColor,
			float lightIntensity)
		{
			Scene& scene = *m_Scene;
			if (BattleEntities::Alive(scene, effect.Entity) == false)
			{
				return;
			}
			scene.SetWorldPosition(Entity{ effect.Entity, &scene }, position);
			SpriteAnimator::Play(Entity{ effect.Entity, &scene }, clip, true);
			effect.Time = duration;
			effect.LightPeak = lightIntensity;
			effect.LightTime = lightIntensity > 0.0f ? LightFlareTime : 0.0f;
			if (scene.m_Registry.has<Light2DComponent>(effect.Entity))
			{
				scene.m_Registry.get<Light2DComponent>(effect.Entity).Color = lightColor;
			}
		}

		void PresentSprite(SpriteEffect& effect, float deltaTime)
		{
			Scene& scene = *m_Scene;
			if (BattleEntities::Alive(scene, effect.Entity) == false)
			{
				return;
			}
			effect.Time = std::max(0.0f, effect.Time - deltaTime);
			effect.LightTime = std::max(0.0f, effect.LightTime - deltaTime);
			BattleEntities::SetColor(scene, effect.Entity, glm::vec4(1.0f, 1.0f, 1.0f, effect.Time > 0.0f ? 1.0f : 0.0f));
			if (scene.m_Registry.has<Light2DComponent>(effect.Entity))
			{
				const float k = effect.LightTime / LightFlareTime;
				scene.m_Registry.get<Light2DComponent>(effect.Entity).Intensity = effect.LightPeak * k * k;
			}
		}

		// A short-lived entity with one particle burst `height` metres above `ground` (layout units), a little towards the
		// camera; it destroys itself when finished. The preset's lengths are layout units, scaled to metres here.
		void Play(const char* name, const glm::vec2& ground, float height, ParticleEmitter2DComponent settings, ParticleSpace space)
		{
			const float k = m_Stage->GetScale();
			settings.Space = space;
			settings.ShapeSize *= k;
			settings.Speed.Min *= k;
			settings.Speed.Max *= k;
			settings.StartSize.Min *= k;
			settings.StartSize.Max *= k;
			settings.Gravity *= k;
			settings.Turbulence *= k;
			Entity effect = m_Scene->CreateEntity(name);
			// A new entity is a root, so its Translation is its place in the world.
			effect.AddComponent<TransformComponent>().Translation = m_Stage->Above(ground, height) + m_Stage->GetView().Back * 0.4f;
			effect.AddComponent<ParticleEmitter2DComponent>(settings);
		}

		Scene* m_Scene = nullptr;
		BattleStage* m_Stage = nullptr;
		SpriteEffect m_Strike;
		SpriteEffect m_Heal;
	};
}
