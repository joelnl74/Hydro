// Game/src/Scripts/Common/CameraFollowScript.h
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
	// Put it on the camera: the camera glides after the first entity tagged "Player" (the zoom is left alone)
	// and its focus stays inside the bounds. V switches between top-down and the tilted diorama view, O
	// between perspective and orthographic in the diorama (DioramaCamera.h). It follows in play mode only,
	// because scripts only run there. Where the target stands in a camera zone (CameraZoneScript) the camera
	// takes the zone's framing and pan area, blending over the zone's BlendSeconds; elsewhere the scene's.
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

				// The framing: the zone the target stands in, else the scene's; eased so a new zone settles in about
				// its BlendSeconds (the yaw turns the short way).
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
				// The height of the ground the character stands on (a terrace, stairs), so it stays in frame.
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
				// A cutscene may hold the camera (DioramaView::CinematicWeight); the follow goes on underneath, so
				// letting go glides back to the target.
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
		float m_FocusHeight = 0.0f; // and how high that ground is (the target's z, smoothed)
		DioramaFraming m_Framing;   // the framing now, easing towards the zone's or the scene's
		float m_BlendSeconds = 0.8f; // how quickly: the last zone entered
	};
}
