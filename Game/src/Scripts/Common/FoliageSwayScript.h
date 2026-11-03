// Game/src/Scripts/Common/FoliageSwayScript.h
#pragma once

#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Hydro
{
	// Wind in the young bamboo and maples (Nov 3, 2026): the children of the entity this script is on lean a little
	// to either side about their feet, each on its own phase, with a slow gust over the group. An upright sprite card
	// leans in its own plane: the Z rotation turns the flat card about its foot before it stands up, so the top moves
	// and the foot stays. Put it on a group of foliage cards (no colliders on them: those would turn too). Runs while
	// playing only; the saved scene keeps the cards as placed.
	HY_SCRIPT()
	class FoliageSwayScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 10), Tooltip("Degrees a card leans either way at most"))
		float Degrees = 2.0f;
		HY_PROPERTY(Range(0.5, 20), Tooltip("Seconds of one sway"))
		float Period = 3.4f;
		HY_PROPERTY(Range(0, 1), Tooltip("How much a slow gust over the whole group adds (0 = a steady breeze)"))
		float Gust = 0.5f;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			std::vector<Entity> children;
			scene.GetChildren(GetEntity(), children);
			for (Entity child : children)
			{
				if (child.HasComponent<TransformComponent>() == false)
				{
					continue;
				}
				const TransformComponent& transform = child.GetComponent<TransformComponent>();
				// A phase from where it stands, so neighbours don't sway in step.
				const float phase = std::fmod(std::abs(transform.Translation.x * 1.37f + transform.Translation.y * 2.11f), 6.2831853f);
				m_Cards.push_back({ child, transform.Rotation.z, phase });
			}
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;
			const float omega = 6.2831853f / std::max(Period, 0.5f);
			// The gust: a slow swell, never below calm, sometimes up to (1 + Gust) times the breeze.
			const float gust = 1.0f + Gust * (0.5f + 0.5f * std::sin(m_Time * omega * 0.21f)) * (0.6f + 0.4f * std::sin(m_Time * omega * 0.53f + 1.3f));
			const float amplitude = glm::radians(Degrees) / (1.0f + Gust) * gust;
			for (Card& card : m_Cards)
			{
				if (card.Item.IsValid() == false)
				{
					continue;
				}
				const float sway = 0.75f * std::sin(m_Time * omega + card.Phase) + 0.25f * std::sin(m_Time * omega * 2.3f + card.Phase * 1.7f);
				card.Item.GetComponent<TransformComponent>().Rotation.z = card.Rest + amplitude * sway;
			}
		}

	private:
		struct Card
		{
			Entity Item;
			float Rest = 0.0f;  // its Z rotation as placed
			float Phase = 0.0f;
		};

		std::vector<Card> m_Cards;
		float m_Time = 0.0f;
	};
}
