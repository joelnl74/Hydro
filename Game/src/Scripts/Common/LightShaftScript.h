#pragma once

#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Hydro
{
	// Faked god rays: the children are additive light-shaft sprites that breathe slowly, take the sun's colour and fade
	// with its intensity, so night has none. Runs while playing only.
	HY_SCRIPT()
	class LightShaftScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 1), Tooltip("How much a shaft's strength swells and fades (0 = steady)"))
		float Breathe = 0.3f;
		HY_PROPERTY(Range(1, 30), Tooltip("Seconds of one breath"))
		float Period = 7.0f;
		HY_PROPERTY(Range(0, 4), Tooltip("Sun intensity at or below which the shafts are gone (night)"))
		float HiddenBelow = 0.25f;
		HY_PROPERTY(Range(0, 4), Tooltip("Sun intensity from which the shafts show fully"))
		float FullFrom = 0.7f;
		HY_PROPERTY(Tooltip("Tint the shafts with the main sun's colour"))
		bool FollowSunColor = true;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			std::vector<Entity> children;
			scene.GetChildren(GetEntity(), children);
			for (Entity child : children)
			{
				if (child.HasComponent<SpriteRendererComponent>() == false || child.HasComponent<TransformComponent>() == false)
				{
					continue;
				}
				const TransformComponent& transform = child.GetComponent<TransformComponent>();
				// A phase from where it stands, so neighbours don't breathe in step.
				const float phase = std::fmod(std::abs(transform.Translation.x * 1.91f + transform.Translation.y * 0.73f), 6.2831853f);
				m_Shafts.push_back({ child, child.GetComponent<SpriteRendererComponent>().Color, phase });
			}
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;

			// The sun's colour is scaled so its brightest channel is 1; its strength is the fade.
			float strength = 1.0f;
			glm::vec3 tint(1.0f);
			Entity sun = GetEntity().GetScene()->GetMainLight();
			if ((bool)sun && sun.HasComponent<DirectionalLightComponent>())
			{
				const DirectionalLightComponent& light = sun.GetComponent<DirectionalLightComponent>();
				const float range = std::max(FullFrom - HiddenBelow, 1e-3f);
				const float t = std::clamp((light.Intensity - HiddenBelow) / range, 0.0f, 1.0f);
				strength = t * t * (3.0f - 2.0f * t);
				if (FollowSunColor)
				{
					tint = light.Color / std::max({ light.Color.r, light.Color.g, light.Color.b, 1e-3f });
				}
			}

			const float omega = 6.2831853f / std::max(Period, 1.0f);
			for (Shaft& shaft : m_Shafts)
			{
				if (shaft.Item.IsValid() == false)
				{
					continue;
				}
				// Two slow waves, so the breath never repeats exactly; 1 - Breathe at the faintest.
				const float wave = 0.65f * std::sin(m_Time * omega + shaft.Phase)
					+ 0.35f * std::sin(m_Time * omega * 0.37f + shaft.Phase * 2.3f);
				const float breath = 1.0f - Breathe * (0.5f - 0.5f * wave);
				SpriteRendererComponent& sprite = shaft.Item.GetComponent<SpriteRendererComponent>();
				sprite.Color = glm::vec4(glm::vec3(shaft.Rest) * tint, shaft.Rest.a * strength * breath);
			}
		}

	private:
		struct Shaft
		{
			Entity Item;
			glm::vec4 Rest{ 1.0f }; // its colour as placed (alpha = full strength)
			float Phase = 0.0f;
		};

		std::vector<Shaft> m_Shafts;
		float m_Time = 0.0f;
	};
}
