#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"

namespace Hydro
{
	// The battle's particle effects as presets for a short-lived entity: one burst, then it destroys itself. Sizes and
	// speeds are in layout units, like the rest of the battle; BattleScript::PlayEffect scales them to metres.
	class BattleEffects
	{
	public:
		// Lit but glowing (emission, so they bloom), sprayed in the plane facing the camera.
		static ParticleEmitter2DComponent HitSparks(DamageType type, bool strong)
		{
			ParticleEmitter2DComponent sparks;
			sparks.Looping = false;
			sparks.Duration = 0.05f;
			sparks.Rate = 0.0f;
			sparks.BurstCount = strong ? 30 : 18;
			sparks.MaxParticles = 48;
			sparks.Shape = EmitterShape::Circle;
			sparks.ShapeSize = { 0.25f, 0.25f };
			sparks.Lifetime = { 0.25f, 0.5f };
			sparks.Speed = { 4.0f, strong ? 10.0f : 7.5f };
			sparks.Direction = 90.0f;
			sparks.Spread = 180.0f;
			sparks.StartSize = { 0.12f, 0.28f };
			sparks.EndSizeScale = 0.2f;
			sparks.Gravity = { 0.0f, -9.0f };
			sparks.Drag = 3.0f;
			sparks.Lighting = ParticleLighting::Lit;
			sparks.EmissionIntensity = 3.0f;
			sparks.Blend = ParticleBlend::Additive;
			sparks.SortOrder = 200;
			sparks.DestroyWhenFinished = true;
			switch (type)
			{
				case DamageType::Fire:
					sparks.StartColor = { 1.0f, 0.75f, 0.3f, 1.0f };
					sparks.EndColor = { 1.0f, 0.25f, 0.05f, 0.0f };
					break;
				case DamageType::Light:
					sparks.StartColor = { 1.0f, 1.0f, 0.85f, 1.0f };
					sparks.EndColor = { 1.0f, 0.85f, 0.4f, 0.0f };
					break;
				default:
					sparks.StartColor = { 1.0f, 0.97f, 0.8f, 1.0f };
					sparks.EndColor = { 1.0f, 0.7f, 0.3f, 0.0f };
					break;
			}
			return sparks;
		}

		// Lit: smoke is a thing in the scene, not a light.
		static ParticleEmitter2DComponent DefeatPuff(const EnemyDefinition& enemy)
		{
			ParticleEmitter2DComponent puff;
			puff.Looping = false;
			puff.Duration = 0.05f;
			puff.Rate = 0.0f;
			puff.BurstCount = 26;
			puff.MaxParticles = 32;
			puff.Shape = EmitterShape::Circle;
			puff.ShapeSize = { 0.8f, 0.8f };
			puff.Lifetime = { 0.7f, 1.2f };
			puff.Speed = { 0.4f, 1.8f };
			puff.Direction = 90.0f;
			puff.Spread = 180.0f;
			puff.StartSize = { 0.6f, 1.1f };
			puff.EndSizeScale = 2.2f;
			puff.StartRotation = { 0.0f, 360.0f };
			puff.AngularVelocity = { -60.0f, 60.0f };
			puff.Gravity = { 0.0f, 0.9f };
			puff.Drag = 1.5f;
			puff.SortOrder = 25;
			puff.DestroyWhenFinished = true;
			// Darkening as it fades.
			puff.StartColor = glm::vec4(enemy.SmokeColor, 0.8f);
			puff.EndColor = glm::vec4(enemy.SmokeColor * 0.75f, 0.0f);
			return puff;
		}

		// On the floor (Ground space), when the hero sets off and stops.
		static ParticleEmitter2DComponent StepDust()
		{
			ParticleEmitter2DComponent dust;
			dust.Looping = false;
			dust.Duration = 0.05f;
			dust.Rate = 0.0f;
			dust.BurstCount = 10;
			dust.MaxParticles = 16;
			dust.Shape = EmitterShape::Circle;
			dust.ShapeSize = { 0.4f, 0.4f };
			dust.Lifetime = { 0.35f, 0.6f };
			dust.Speed = { 0.6f, 1.6f };
			dust.Direction = 90.0f;
			dust.Spread = 180.0f;
			dust.StartSize = { 0.3f, 0.55f };
			dust.EndSizeScale = 1.8f;
			dust.Drag = 3.0f;
			dust.StartColor = { 0.62f, 0.52f, 0.42f, 0.55f };
			dust.EndColor = { 0.55f, 0.48f, 0.4f, 0.0f };
			dust.SortOrder = 20;
			dust.DestroyWhenFinished = true;
			return dust;
		}

		static ParticleEmitter2DComponent KickDust()
		{
			ParticleEmitter2DComponent dust = StepDust();
			dust.BurstCount = 18;
			dust.MaxParticles = 24;
			dust.Speed = { 1.5f, 3.5f };
			dust.Direction = 180.0f; // away from the hero, who strikes from the right
			dust.Spread = 70.0f;
			dust.StartSize = { 0.35f, 0.7f };
			return dust;
		}

		static ParticleEmitter2DComponent HealMotes()
		{
			ParticleEmitter2DComponent motes;
			motes.Looping = false;
			motes.Duration = 0.4f;
			motes.Rate = 40.0f;
			motes.BurstCount = 10;
			motes.MaxParticles = 40;
			motes.Shape = EmitterShape::Box;
			motes.ShapeSize = { 1.6f, 0.6f };
			motes.Lifetime = { 0.7f, 1.1f };
			motes.Speed = { 1.0f, 2.2f };
			motes.Direction = 90.0f;
			motes.Spread = 12.0f;
			motes.StartSize = { 0.15f, 0.3f };
			motes.EndSizeScale = 0.4f;
			motes.Drag = 0.8f;
			motes.Turbulence = 2.0f;
			motes.StartColor = { 0.55f, 1.0f, 0.6f, 1.0f };
			motes.EndColor = { 0.8f, 1.0f, 0.7f, 0.0f };
			motes.EmissionIntensity = 2.0f;
			motes.SortOrder = 200;
			motes.DestroyWhenFinished = true;
			return motes;
		}
	};
}
