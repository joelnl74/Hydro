#pragma once
#include "Hydro.h"
#include "Scripts/Common/DioramaCamera.h"

#include <algorithm>

// Camera zones (Nov 1, 2026): an area of a field scene with its own camera framing. Put CameraZoneScript on an empty
// entity; its position is the zone's centre on the ground and Size its width and depth. While the player stands in
// it, the follow camera (CameraFollowScript) looks the zone's way, at its tilt and zoom, leads and rises by its
// amounts and pans only inside PanMin..PanMax, blending there over BlendSeconds; outside every zone it uses the
// scene's framing (the director, FieldSceneScript). Overlapping zones: the highest Priority wins, then the smallest.
// Typical use: a street framed along its length, a stair pulled back and higher, a plaza centred on its gate.
// Scene builders write the zones (scripts/japanese_city/build_city_slice.py).
namespace Hydro
{
	HY_SCRIPT()
	class CameraZoneScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Publish();
		}

		// Every frame, so the zone follows its entity and inspector edits show while playing.
		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			Publish();
		}

		void OnDestroy() override
		{
			Entity self = GetEntity();
			DioramaView::RemoveZone(self.GetScene(), self);
		}

	public:
		HY_PROPERTY(Min(0), Tooltip("Width (x) and depth (y) of the zone on the ground in metres, centred on the entity"))
		glm::vec2 Size{ 10.0f, 10.0f };
		HY_PROPERTY(Tooltip("Where zones overlap, the highest priority wins (then the smallest zone)"))
		int Priority = 0;
		HY_PROPERTY(Range(0, 89), Tooltip("Camera tilt in degrees from straight down (0 = the scene's)"))
		float TiltDegrees = 0.0f;
		HY_PROPERTY(Range(-180, 180), Tooltip("Direction the camera looks, degrees counter-clockwise seen from above (0 = north, +Y)"))
		float YawDegrees = 0.0f;
		HY_PROPERTY(Range(0, 120), Tooltip("Vertical field of view in degrees (0 = the scene's)"))
		float FieldOfView = 0.0f;
		HY_PROPERTY(Range(0, 4), Tooltip("How much the camera sees, times the camera's Orthographic Size (below 1 = closer; 0 = the scene's)"))
		float ViewScale = 1.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres the camera looks ahead of the player, along the view"))
		float FocusLead = 3.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres above the player's ground the camera aims at"))
		float FocusRise = 2.2f;
		HY_PROPERTY(Tooltip("Lowest x, y the camera's focus may reach, from the zone's centre (Pan Min = Pan Max: the scene's bounds)"))
		glm::vec2 PanMin{ 0.0f };
		HY_PROPERTY(Tooltip("Highest x, y the camera's focus may reach, from the zone's centre"))
		glm::vec2 PanMax{ 0.0f };
		HY_PROPERTY(Range(0, 10), Tooltip("Seconds the camera takes to settle into this zone's framing"))
		float BlendSeconds = 0.8f;

	private:
		void Publish()
		{
			Entity self = GetEntity();
			const glm::vec3 centre = GetWorldPosition();
			const glm::vec2 half = glm::max(Size, glm::vec2(0.0f)) * 0.5f;

			DioramaCameraZone zone;
			zone.Owner = self.GetScene();
			zone.Entity = self;
			zone.Min = glm::vec2(centre) - half;
			zone.Max = glm::vec2(centre) + half;
			zone.Priority = Priority;
			zone.BlendSeconds = BlendSeconds;
			zone.Framing.TiltDegrees = TiltDegrees;
			zone.Framing.YawDegrees = YawDegrees;
			zone.Framing.FieldOfView = FieldOfView;
			zone.Framing.ViewScale = ViewScale;
			zone.Framing.FocusLead = FocusLead;
			zone.Framing.FocusRise = FocusRise;
			zone.HasPanBounds = PanMin != PanMax;
			zone.PanMin = glm::vec2(centre) + glm::min(PanMin, PanMax);
			zone.PanMax = glm::vec2(centre) + glm::max(PanMin, PanMax);
			DioramaView::SetZone(zone);
		}
	};
}
