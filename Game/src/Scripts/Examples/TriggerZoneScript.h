// Game/src/Scripts/Examples/TriggerZoneScript.h
#pragma once

#include "Hydro.h"

#include <string>

// An example (Scripting plan SC8b, rewritten from the deleted village's zone script): a trigger zone. It shows a
// script reacting to physics triggers, references set in the editor instead of names looked up in code, and state
// that belongs to the zone itself (who is inside) rather than to a static.
//
// Put it on an entity with a sensor collider (a 3D one with IsTrigger, or a 2D one). When an entity with the
// Watched tag (the player) enters, the Target entity is shown; when the last one leaves, it is hidden again. Leave
// Target empty to only log, which is enough to see triggers working.
namespace Hydro
{
	HY_SCRIPT()
	class TriggerZoneScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Tooltip("The tag of the entities the zone reacts to"))
		std::string Watched = "Player";
		HY_PROPERTY(Tooltip("Shown while a watched entity is inside (a light, a sign, an effect); empty: only log"))
		EntityRef Target;
		HY_PROPERTY(Tooltip("Hide the target again when the last watched entity leaves"))
		bool HideOnExit = true;

	protected:
		void OnCreate() override
		{
			SetTargetShown(false);
		}

		void OnTriggerEnter(Entity other) override
		{
			if (IsWatched(other) == false)
			{
				return;
			}
			m_Inside++;
			HY_INFO("{}: {} entered ({} inside)", GetEntity().GetName(), other.GetName(), m_Inside);
			SetTargetShown(true);
		}

		void OnTriggerExit(Entity other) override
		{
			if (IsWatched(other) == false || m_Inside == 0)
			{
				return;
			}
			m_Inside--;
			HY_INFO("{}: {} left ({} inside)", GetEntity().GetName(), other.GetName(), m_Inside);
			if (m_Inside == 0 && HideOnExit)
			{
				SetTargetShown(false);
			}
		}

	private:
		bool IsWatched(Entity other) const
		{
			return other.IsValid() && other.HasComponent<TagComponent>() && other.GetComponent<TagComponent>().Tag == Watched;
		}

		// A sprite's alpha, or a light's intensity (kept from the scene, so the zone only switches it).
		void SetTargetShown(bool shown)
		{
			Entity target = Target.Get();
			if ((bool)target == false)
			{
				return;
			}
			if (SpriteRendererComponent* sprite = target.TryGetComponent<SpriteRendererComponent>())
			{
				sprite->Color.a = shown ? 1.0f : 0.0f;
			}
			if (Light2DComponent* light = target.TryGetComponent<Light2DComponent>())
			{
				if (m_LightIntensity < 0.0f)
				{
					m_LightIntensity = light->Intensity;
				}
				light->Intensity = shown ? m_LightIntensity : 0.0f;
			}
		}

		int m_Inside = 0;
		float m_LightIntensity = -1.0f;
	};
}
