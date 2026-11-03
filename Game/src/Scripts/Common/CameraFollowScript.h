#pragma once

#include "Hydro.h"
#include "Scripts/Common/DioramaCamera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// On the camera: glides after the entity tagged "Player", keeping its focus inside the bounds and taking the framing of
	// the camera zone the target stands in. V and O switch the view (DioramaCamera.h).
	HY_SCRIPT()
	class CameraFollowScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();

			DioramaView::Update(deltaTime);
			if (m_Target == entt::null || scene->m_Registry.valid(m_Target) == false)
			{
				m_Target = FindTarget(*scene);
				m_Snap = true;
			}

			if (m_Target != entt::null && scene->m_Registry.has<TransformComponent>(m_Target))
			{
				const glm::vec3 position = scene->GetWorldPosition(Entity{ m_Target, scene });

				// The zone the target stands in, else the scene's; eased so a new zone settles in about its BlendSeconds.
				DioramaFraming goalFraming = DioramaView::GetSceneFraming();
				const DioramaCameraZone* zone = DioramaView::FindZone(scene, { position.x, position.y });
				if (zone != nullptr)
				{
					goalFraming = DioramaView::ResolveZoneFraming(zone->Framing, goalFraming);
					m_BlendSeconds = zone->BlendSeconds;
				}
				if (m_Snap)
				{
					m_Framing = goalFraming;
				}
				const float settle = 1.0f - std::exp(-3.0f * deltaTime / std::max(0.05f, m_BlendSeconds));
				m_Framing = DioramaFraming::Mix(m_Framing, goalFraming, settle);

				// Ahead of the target along the way the camera looks on the ground.
				const float yaw = glm::radians(m_Framing.YawDegrees) * DioramaView::Blend;
				const glm::vec2 ahead{ -std::sin(yaw), std::cos(yaw) };
				const float lead = LookAhead + m_Framing.FocusLead * DioramaView::Blend;
				glm::vec2 goal = glm::vec2(position.x, position.y) + ahead * lead;
				if (zone != nullptr && zone->HasPanBounds)
				{
					goal = glm::clamp(goal, zone->PanMin, zone->PanMax);
				}
				else
				{
					goal = DioramaView::ClampFocus(goal, Bounds);
				}

				// Frame-rate independent smoothing: closes most of the gap in about a third of a second.
				if (m_Snap)
				{
					m_Focus = goal;
					m_FocusHeight = position.z;
				}
				const float t = 1.0f - std::exp(-Smoothing * deltaTime);
				m_Focus += (goal - m_Focus) * t;
				// The height of the ground it stands on (a terrace, stairs), so it stays in frame.
				m_FocusHeight += (position.z - m_FocusHeight) * t;
				m_Snap = false;
			}
			else if (m_Snap)
			{
				m_Focus = glm::vec2(GetWorldPosition());
				m_Framing = DioramaView::GetSceneFraming();
			}

			if (self.HasComponent<CameraComponent>())
			{
				// The follow goes on under a cutscene's hold, so letting go glides back to the target.
				glm::vec2 focus = m_Focus;
				float height = m_FocusHeight + m_Framing.FocusRise * DioramaView::Blend;
				DioramaView::MixCinematic(focus, height);
				DioramaView::Apply(self, focus, height, m_Framing);
			}
		}

	public:
		HY_PROPERTY(Range(0, 50), Tooltip("How quickly the camera catches up with the target (higher is snappier)"))
		float Smoothing = 8.0f;
		HY_PROPERTY(Range(-10, 10), Tooltip("Metres ahead of the target's feet to aim at (its chest)"))
		float LookAhead = 1.5f;
		HY_PROPERTY(Min(0), Tooltip("Half-size of the area the focus stays in, when the scene sets no bounds of its own"))
		glm::vec2 Bounds{ 17.0f, 23.0f };

	private:
		entt::entity FindTarget(Scene& scene) const
		{
			auto view = scene.m_Registry.view<TagComponent, TransformComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == "Player")
				{
					return entity;
				}
			}

			return entt::null;
		}

		entt::entity m_Target = entt::null;
		bool m_Snap = true;
		glm::vec2 m_Focus{ 0.0f };  // where the camera looks, on the ground
		float m_FocusHeight = 0.0f; // and how high that ground is (smoothed)
		DioramaFraming m_Framing;   // easing towards the zone's or the scene's
		float m_BlendSeconds = 0.8f; // the last zone entered
	};
}
