#pragma once

#include "Hydro.h"
#include "Scripts/Common/DioramaCamera.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	// The Sacred Tree waking: the awake overlay and vein glow, its light and rising motes, and what the tremor shakes loose.
	// What happens, not when: ShrineQuestScript drives it.
	class TreeAwakening
	{
	public:
		void Bind(Scene& scene, const glm::vec3& glowColor, Entity (*find)(Scene&, const std::string&))
		{
			m_GlowColor = glowColor;
			m_Tree = find(scene, "Sacred Tree");
			m_TreeAwake = find(scene, "Sacred Tree Awake");
			m_TreeGlow = find(scene, "Sacred Tree Glow");
			if (m_TreeGlow.IsValid() && m_TreeGlow.HasComponent<SpriteRendererComponent>())
			{
				m_GlowEmission = m_TreeGlow.GetComponent<SpriteRendererComponent>().EmissionIntensity;
			}
		}

		glm::vec3 GetFoot(Scene& scene) const
		{
			return Foot(scene, m_Tree);
		}

		// Both stay once made: the tree stays awake.
		void CreateEffects(Scene& scene)
		{
			const glm::vec3 foot = Foot(scene, m_Tree);
			if (m_TreeLight.IsValid() == false)
			{
				m_TreeLight = scene.CreateEntity("Light Sacred Tree");
				m_TreeLight.AddComponent<TransformComponent>().Translation = foot + glm::vec3(0.0f, -0.4f, 0.0f);
				Light2DComponent& light = m_TreeLight.AddComponent<Light2DComponent>();
				light.Type = Light2DType::Point;
				light.Color = m_GlowColor;
				light.Intensity = 0.0f;
				light.Radius = 7.5f;
				light.Height = 2.5f;
			}
			if (m_Motes.IsValid() == false)
			{
				m_Motes = scene.CreateEntity("Sacred Tree Motes");
				// View-space particles are placed up the standing tree, along the camera's up.
				m_Motes.AddComponent<TransformComponent>().Translation = DioramaView::Above(foot + glm::vec3(0.0f, -0.6f, 0.0f), 0.8f);
				m_Motes.AddComponent<ParticleEmitter2DComponent>(Motes(0.0f));
			}
		}

		// `amount` 0..1 of the waking; `time` drives the breathing.
		void Set(float amount, float time)
		{
			const float breath = 0.5f + 0.5f * std::sin(time * 1.6f);
			if (m_TreeAwake.IsValid() && m_TreeAwake.HasComponent<SpriteRendererComponent>())
			{
				m_TreeAwake.GetComponent<SpriteRendererComponent>().Color.a = amount;
			}
			if (m_TreeGlow.IsValid() && m_TreeGlow.HasComponent<SpriteRendererComponent>())
			{
				SpriteRendererComponent& glow = m_TreeGlow.GetComponent<SpriteRendererComponent>();
				// The veins light up in pulses while it wakes, then settle into a slow breath.
				const float pulse = amount < 1.0f ? 0.6f + 0.4f * std::abs(std::sin(time * 3.2f)) : 0.75f + 0.25f * breath;
				glow.Color.a = std::min(1.0f, amount * 1.3f) * pulse;
				glow.EmissionIntensity = m_GlowEmission * (0.7f + 0.6f * pulse);
			}
			if (m_TreeLight.IsValid() && m_TreeLight.HasComponent<Light2DComponent>())
			{
				m_TreeLight.GetComponent<Light2DComponent>().Intensity = amount * (1.1f + 0.4f * breath);
			}
			if (m_Motes.IsValid() && m_Motes.HasComponent<ParticleEmitter2DComponent>())
			{
				m_Motes.GetComponent<ParticleEmitter2DComponent>().Rate = 2.0f + 10.0f * amount;
			}
		}

		void ShakeLoose(Scene& scene)
		{
			const glm::vec3 foot = Foot(scene, m_Tree);
			PlayEffect(scene, "Effect Falling Leaves", DioramaView::Above(foot, 7.5f), FallingLeaves());
			PlayEffect(scene, "Effect Root Dust", DioramaView::Above(foot + glm::vec3(0.0f, -0.6f, 0.0f), 0.2f), RootDust());
		}

		// An Upright sprite's entity sits at its middle, its feet half its height towards -Y.
		static glm::vec3 Foot(Scene& scene, Entity entity)
		{
			if (entity.IsValid() == false)
			{
				return glm::vec3(0.0f);
			}
			const glm::mat4 world = scene.GetWorldTransform(entity);
			const float height = glm::length(glm::vec3(world[1]));
			return glm::vec3(world[3]) - glm::vec3(0.0f, 0.5f * height, 0.0f);
		}

	private:
		// ---- Effects ---------------------------------------------------------------------------------------

		// Rate is set every frame.
		ParticleEmitter2DComponent Motes(float rate) const
		{
			ParticleEmitter2DComponent motes;
			motes.Looping = true;
			motes.Duration = 2.0f;
			motes.Rate = rate;
			motes.MaxParticles = 96;
			motes.Shape = EmitterShape::Box;
			motes.ShapeSize = { 3.6f, 0.8f };
			motes.Lifetime = { 2.5f, 4.5f };
			motes.Speed = { 0.4f, 1.1f };
			motes.Direction = 90.0f;
			motes.Spread = 14.0f;
			motes.StartSize = { 0.08f, 0.18f };
			motes.EndSizeScale = 0.3f;
			motes.Turbulence = 0.8f;
			motes.StartColor = glm::vec4(m_GlowColor, 0.9f);
			motes.EndColor = glm::vec4(m_GlowColor * 0.7f + glm::vec3(0.3f), 0.0f);
			motes.Space = ParticleSpace::View;
			motes.EmissionIntensity = 2.5f;
			motes.SortOrder = 200;
			return motes;
		}

		static ParticleEmitter2DComponent FallingLeaves()
		{
			ParticleEmitter2DComponent leaves;
			leaves.Looping = false;
			leaves.Duration = 2.5f;
			leaves.Rate = 22.0f;
			leaves.BurstCount = 12;
			leaves.MaxParticles = 96;
			leaves.Shape = EmitterShape::Box;
			leaves.ShapeSize = { 8.0f, 2.5f };
			leaves.Lifetime = { 2.0f, 3.5f };
			leaves.Speed = { 0.2f, 0.8f };
			leaves.Direction = -90.0f;
			leaves.Spread = 40.0f;
			leaves.StartSize = { 0.12f, 0.22f };
			leaves.EndSizeScale = 0.8f;
			leaves.StartRotation = { 0.0f, 360.0f };
			leaves.AngularVelocity = { -200.0f, 200.0f };
			leaves.Gravity = { 0.0f, -1.6f };
			leaves.Drag = 1.2f;
			leaves.Turbulence = 2.5f;
			leaves.StartColor = { 0.62f, 0.72f, 0.3f, 1.0f };
			leaves.EndColor = { 0.5f, 0.55f, 0.25f, 0.0f };
			leaves.Space = ParticleSpace::View;
			leaves.SortOrder = 190;
			leaves.DestroyWhenFinished = true;
			return leaves;
		}

		static ParticleEmitter2DComponent RootDust()
		{
			ParticleEmitter2DComponent dust;
			dust.Looping = false;
			dust.Duration = 1.5f;
			dust.Rate = 18.0f;
			dust.BurstCount = 16;
			dust.MaxParticles = 64;
			dust.Shape = EmitterShape::Box;
			dust.ShapeSize = { 5.0f, 0.6f };
			dust.Lifetime = { 0.8f, 1.6f };
			dust.Speed = { 0.3f, 1.0f };
			dust.Direction = 90.0f;
			dust.Spread = 70.0f;
			dust.StartSize = { 0.25f, 0.5f };
			dust.EndSizeScale = 2.0f;
			dust.Drag = 1.5f;
			dust.StartColor = { 0.66f, 0.58f, 0.46f, 0.5f };
			dust.EndColor = { 0.7f, 0.62f, 0.5f, 0.0f };
			dust.Space = ParticleSpace::View;
			dust.SortOrder = 180;
			dust.DestroyWhenFinished = true;
			return dust;
		}

		// A new entity is a root, so `position` is its place in the world.
		static void PlayEffect(Scene& scene, const char* name, const glm::vec3& position, const ParticleEmitter2DComponent& settings)
		{
			Entity effect = scene.CreateEntity(name);
			effect.AddComponent<TransformComponent>().Translation = position;
			effect.AddComponent<ParticleEmitter2DComponent>(settings);
		}

		Entity m_Tree, m_TreeAwake, m_TreeGlow, m_TreeLight, m_Motes;
		glm::vec3 m_GlowColor{ 0.35f, 0.9f, 1.0f };
		float m_GlowEmission = 1.6f;
	};
}
