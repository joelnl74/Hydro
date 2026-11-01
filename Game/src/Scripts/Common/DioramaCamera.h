// Game/src/Scripts/Common/DioramaCamera.h
#pragma once

#include "Hydro.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// The diorama camera (Diorama RPG H2): the field cameras (CameraFollow, VillageCamera) switch between
// today's top-down view and a tilted view of the same scene. The ground stays on the XY plane with +Z up,
// so Box2D, the 2D lights and the blob shadows work unchanged in both; sprites marked Upright stand up and
// face the camera (SpriteFacing, SceneRenderer).
//
// Keys (play mode):
//   V   top-down <-> diorama (eased over about 0.4 s)
//   O   in the diorama: perspective <-> orthographic (the GDD's open question; both tilt the same)
// The camera's OrthographicSize stays the zoom in both views: the world units seen vertically where the
// camera looks (the focus point on the ground), so - and = keep working and world-space UI keeps its size.
//
// Camera zones (Scripts/Common/CameraZoneScript.h, Nov 1, 2026): areas of a scene with their own framing (the
// direction the camera looks, its tilt, zoom, lead and rise, and how far it may pan). The follow camera
// (CameraFollowScript) takes the framing of the zone its target stands in and blends to the next one, so a
// street, a stair and a plaza can each be framed for what the player needs to see.

namespace Hydro
{
	// What the tilted camera looks like: the tilt from straight down, the yaw (degrees counter-clockwise seen from
	// above; 0 = the camera looks north, along +Y), the perspective field of view, a scale on the camera's
	// OrthographicSize (below 1 = closer), how far ahead of the target the focus sits along the ground and how far
	// above the target's ground it aims.
	struct DioramaFraming
	{
		float TiltDegrees = 50.0f;
		float YawDegrees = 0.0f;
		float FieldOfView = 25.0f;
		float ViewScale = 1.0f;
		float FocusLead = 0.0f;
		float FocusRise = 0.0f;

		// t of the way from a to b; the yaw turns the short way round.
		static DioramaFraming Mix(const DioramaFraming& a, const DioramaFraming& b, float t)
		{
			DioramaFraming mixed;
			mixed.TiltDegrees = a.TiltDegrees + (b.TiltDegrees - a.TiltDegrees) * t;
			const float turn = std::remainder(b.YawDegrees - a.YawDegrees, 360.0f);
			mixed.YawDegrees = std::remainder(a.YawDegrees + turn * t, 360.0f);
			mixed.FieldOfView = a.FieldOfView + (b.FieldOfView - a.FieldOfView) * t;
			mixed.ViewScale = a.ViewScale + (b.ViewScale - a.ViewScale) * t;
			mixed.FocusLead = a.FocusLead + (b.FocusLead - a.FocusLead) * t;
			mixed.FocusRise = a.FocusRise + (b.FocusRise - a.FocusRise) * t;
			return mixed;
		}
	};

	// A camera zone as the follow camera sees it (written by CameraZoneScript every frame): the area on the ground
	// (world x, y), which zone wins where they overlap, how long the camera takes to settle into it, its framing
	// (TiltDegrees, FieldOfView and ViewScale at 0 = the scene's) and, when HasPanBounds, the area the focus stays in.
	struct DioramaCameraZone
	{
		Scene* Owner = nullptr;
		entt::entity Entity = entt::null;
		glm::vec2 Min{ 0.0f };
		glm::vec2 Max{ 0.0f };
		int Priority = 0;
		float BlendSeconds = 0.8f;
		DioramaFraming Framing;
		bool HasPanBounds = false;
		glm::vec2 PanMin{ 0.0f };
		glm::vec2 PanMax{ 0.0f };
	};

	struct DioramaView
	{
		// From straight down: 0 = top-down, 90 = looking along the ground. 50 degrees from straight down is
		// the GDD's 40 degree pitch above the ground.
		static constexpr float TiltDegrees = 50.0f;
		// Vertical, degrees: narrow and from further away, so the scene looks like a model with little
		// perspective distortion.
		static constexpr float FieldOfView = 25.0f;
		static constexpr float BlendSpeed = 2.5f; // of the whole transition, per second

		// Shared by every field scene and kept across scene changes (a battle and back stays in the same view).
		static inline bool Enabled = false;
		static inline bool Perspective = true;
		static inline float Blend = 0.0f; // 0 top-down .. 1 diorama

		// A scene's own framing (set by its director script when it starts, reset by ResetSceneFraming when it
		// ends): the tilt in degrees (0 = TiltDegrees), the perspective camera's vertical field of view in
		// degrees (0 = FieldOfView), how far ahead of the target, along the ground, the camera looks in the
		// diorama view (added to the follow camera's look-ahead; 0 in top-down), how far above the target's
		// ground it looks (a low camera then still shows what stands on the terraces above), and the area the follow
		// camera's focus stays in (the map's walkable part, so the view never shows past its dressed edges;
		// when HasSceneBounds is false the camera script's own bounds apply).
		static inline float SceneTiltDegrees = 0.0f;
		static inline float SceneFieldOfView = 0.0f;
		static inline float SceneFocusLead = 0.0f;
		static inline float SceneFocusRise = 0.0f;
		static inline bool HasSceneBounds = false;
		static inline glm::vec2 SceneBoundsMin{ 0.0f };
		static inline glm::vec2 SceneBoundsMax{ 0.0f };
		// The scene's view direction (degrees counter-clockwise seen from above, 0 = looking north) and a scale on
		// the camera's OrthographicSize (0 = 1).
		static inline float SceneYawDegrees = 0.0f;
		static inline float SceneViewScale = 0.0f;
		static void ResetSceneFraming()
		{
			SceneTiltDegrees = 0.0f;
			SceneFieldOfView = 0.0f;
			SceneFocusLead = 0.0f;
			SceneFocusRise = 0.0f;
			HasSceneBounds = false;
			SceneBoundsMin = glm::vec2(0.0f);
			SceneBoundsMax = glm::vec2(0.0f);
			SceneYawDegrees = 0.0f;
			SceneViewScale = 0.0f;
			ResetCinematic();
		}

		// The scene's framing with the defaults filled in.
		static DioramaFraming GetSceneFraming()
		{
			DioramaFraming framing;
			framing.TiltDegrees = SceneTiltDegrees > 0.0f ? SceneTiltDegrees : TiltDegrees;
			framing.YawDegrees = SceneYawDegrees;
			framing.FieldOfView = SceneFieldOfView > 0.0f ? SceneFieldOfView : FieldOfView;
			framing.ViewScale = SceneViewScale > 0.0f ? SceneViewScale : 1.0f;
			framing.FocusLead = SceneFocusLead;
			framing.FocusRise = SceneFocusRise;
			return framing;
		}

		// A zone's framing with its unset values (0) taken from `scene`.
		static DioramaFraming ResolveZoneFraming(const DioramaFraming& zone, const DioramaFraming& scene)
		{
			DioramaFraming framing = zone;
			framing.TiltDegrees = zone.TiltDegrees > 0.0f ? zone.TiltDegrees : scene.TiltDegrees;
			framing.FieldOfView = zone.FieldOfView > 0.0f ? zone.FieldOfView : scene.FieldOfView;
			framing.ViewScale = zone.ViewScale > 0.0f ? zone.ViewScale : scene.ViewScale;
			return framing;
		}

		// The camera zones of the running scenes (CameraZoneScript adds and updates its own, removes it when it
		// ends). Keyed by scene and entity, so a scene being left and one being entered never mix.
		static inline std::vector<DioramaCameraZone> Zones;
		static void SetZone(const DioramaCameraZone& zone)
		{
			for (DioramaCameraZone& existing : Zones)
			{
				if (existing.Owner == zone.Owner && existing.Entity == zone.Entity)
				{
					existing = zone;
					return;
				}
			}
			Zones.push_back(zone);
		}
		static void RemoveZone(Scene* owner, entt::entity entity)
		{
			Zones.erase(std::remove_if(Zones.begin(), Zones.end(), [owner, entity](const DioramaCameraZone& zone)
				{
					return zone.Owner == owner && zone.Entity == entity;
				}), Zones.end());
		}
		// The zone of `owner` that contains `point` on the ground: the highest priority, then the smallest; nullptr
		// when there is none. Valid until the zones change (the next zone script update).
		static const DioramaCameraZone* FindZone(const Scene* owner, const glm::vec2& point)
		{
			const DioramaCameraZone* best = nullptr;
			float bestArea = 0.0f;
			for (const DioramaCameraZone& zone : Zones)
			{
				if (zone.Owner != owner || point.x < zone.Min.x || point.y < zone.Min.y || point.x > zone.Max.x || point.y > zone.Max.y)
				{
					continue;
				}
				const float area = (zone.Max.x - zone.Min.x) * (zone.Max.y - zone.Min.y);
				if (best == nullptr || zone.Priority > best->Priority || (zone.Priority == best->Priority && area < bestArea))
				{
					best = &zone;
					bestArea = area;
				}
			}
			return best;
		}

		// The yaw the camera was placed with (radians, 0 top-down), and a direction on the screen (x right, y up)
		// turned into one on the ground, so walking keys follow the view.
		static inline float Yaw = 0.0f;
		static glm::vec2 ScreenToGround(const glm::vec2& screen)
		{
			const float c = std::cos(Yaw);
			const float s = std::sin(Yaw);
			return { screen.x * c - screen.y * s, screen.x * s + screen.y * c };
		}

		// A cutscene's hold on the follow camera (ShrineQuestScript): while CinematicWeight > 0 the camera looks at
		// CinematicFocus (a point on the ground, CinematicHeight above z 0) instead of its target, mixed by the
		// weight (0 = the target, 1 = the cutscene's point; the caller eases it), outside the scene's bounds too.
		// CinematicZoom scales how much the camera sees (below 1 = closer) and CinematicShake is how far it
		// jitters in metres, in the view plane (an earthquake). Reset with the scene's framing.
		static inline float CinematicWeight = 0.0f;
		static inline glm::vec2 CinematicFocus{ 0.0f };
		static inline float CinematicHeight = 0.0f;
		static inline float CinematicZoom = 1.0f;
		static inline float CinematicShake = 0.0f;
		static void ResetCinematic()
		{
			CinematicWeight = 0.0f;
			CinematicFocus = glm::vec2(0.0f);
			CinematicHeight = 0.0f;
			CinematicZoom = 1.0f;
			CinematicShake = 0.0f;
		}

		// The follow camera's focus and height with the cutscene's mixed in.
		static void MixCinematic(glm::vec2& focus, float& height)
		{
			const float w = std::clamp(CinematicWeight, 0.0f, 1.0f);
			if (w <= 0.0f)
			{
				return;
			}
			focus += (CinematicFocus - focus) * w;
			height += (CinematicHeight - height) * w;
		}

		// The focus point kept inside the scene's bounds, or inside the given ones when the scene has none.
		static glm::vec2 ClampFocus(const glm::vec2& focus, const glm::vec2& fallbackHalfSize)
		{
			if (HasSceneBounds)
			{
				return glm::clamp(focus, SceneBoundsMin, SceneBoundsMax);
			}
			return glm::clamp(focus, -fallbackHalfSize, fallbackHalfSize);
		}

		// Where the camera looks this frame, for world-space UI (FieldDirectorScript): the focus point on the ground
		// and the camera's right, up and back (towards the camera). Top-down: the camera's x, y at z 0; +X; +Y; +Z.
		static inline glm::vec3 Focus{ 0.0f };
		static inline glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
		static inline glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
		static inline glm::vec3 Back{ 0.0f, 0.0f, 1.0f };

		// A point Offset.x right and Offset.y up from the focus point on the camera's view plane, Lift towards
		// the camera: what the screen calls "Offset from the centre" for a Billboard sprite at the focus distance.
		static glm::vec3 OnScreen(const glm::vec2& offset, float lift = 0.5f)
		{
			return Focus + Right * offset.x + Up * offset.y + Back * lift;
		}

		// Above a point on the ground, as a point on an Upright sprite: Height up the screen top-down, up the
		// standing sprite when tilted. Lift moves it towards the camera, so it is drawn in front.
		static glm::vec3 Above(const glm::vec3& ground, float height, float lift = 0.5f)
		{
			return ground + Up * height + Back * lift;
		}

		// Once per frame, from the scene's camera script.
		static void Update(float deltaTime)
		{
			if (KeyPressed(KeyCode::V, s_PreviousV))
			{
				Enabled = Enabled == false;
				HY_INFO("Diorama camera: {}", Enabled ? "tilted" : "top-down");
			}
			if (KeyPressed(KeyCode::O, s_PreviousO) && Enabled)
			{
				Perspective = Perspective == false;
				HY_INFO("Diorama camera: {}", Perspective ? "perspective" : "orthographic");
			}

			const float target = Enabled ? 1.0f : 0.0f;
			const float step = BlendSpeed * deltaTime;
			Blend = Blend < target ? std::min(target, Blend + step) : std::max(target, Blend - step);
			s_ShakeTime += deltaTime;
		}

		// Places the camera entity looking at focus (a point on the ground, `height` above z 0: a raised terrace the
		// player stands on) for the current blend. Works in the world, so the camera may sit under a parent.
		static void Apply(Entity cameraEntity, const glm::vec2& focus, float height = 0.0f)
		{
			Apply(cameraEntity, focus, height, GetSceneFraming());
		}

		// The same with a framing of its own (a camera zone's, CameraFollowScript).
		static void Apply(Entity cameraEntity, const glm::vec2& focus, float height, const DioramaFraming& framing)
		{
			Scene& scene = *cameraEntity.GetScene();
			TransformComponent placed = cameraEntity.GetComponent<TransformComponent>();
			placed.Translation = scene.GetWorldPosition(cameraEntity);
			Apply(placed, cameraEntity.GetComponent<CameraComponent>(), focus, height, framing);
			placed.Translation += GetShakeOffset();
			PlaceInWorld(cameraEntity, placed.Translation, placed.Rotation);
		}

		// Puts the entity at `translation` with `rotation` (Euler angles, radians) in the world, whatever its parents.
		// A root gets them written as they are; under a parent its local transform is worked out (Scene::
		// SetWorldTransform) and its scale in the world kept.
		static void PlaceInWorld(Entity entity, const glm::vec3& translation, const glm::vec3& rotation)
		{
			Scene& scene = *entity.GetScene();
			TransformComponent& transform = entity.GetComponent<TransformComponent>();
			if ((bool)scene.GetParent(entity) == false)
			{
				transform.Translation = translation;
				transform.Rotation = rotation;
				return;
			}
			const glm::mat4 current = scene.GetWorldTransform(entity);
			TransformComponent world;
			world.Translation = translation;
			world.Rotation = rotation;
			world.Scale = { glm::length(glm::vec3(current[0])), glm::length(glm::vec3(current[1])), glm::length(glm::vec3(current[2])) };
			scene.SetWorldTransform(entity, world.GetTransform());
		}

		// The same on a transform holding world values (the entity is a root, or the caller converts).
		static void Apply(TransformComponent& transform, CameraComponent& cameraComponent, const glm::vec2& focus, float height = 0.0f)
		{
			Apply(transform, cameraComponent, focus, height, GetSceneFraming());
		}

		static void Apply(TransformComponent& transform, CameraComponent& cameraComponent, const glm::vec2& focus, float height,
			const DioramaFraming& framing)
		{
			SceneCamera& camera = cameraComponent.Camera;

			if (Blend <= 0.0f)
			{
				// Top-down, exactly as the scene was authored; remember its settings for the way back.
				if (s_Tilted)
				{
					camera.Projection = s_TopDown.Projection;
					camera.OrthographicNear = s_TopDown.OrthographicNear;
					camera.OrthographicFar = s_TopDown.OrthographicFar;
					transform.Translation.z = s_TopDownZ;
					transform.Rotation = glm::vec3(0.0f);
					camera.FocusDistance = 0.0f;
					s_Tilted = false;
				}
				s_TopDown = camera;
				s_TopDownZ = transform.Translation.z;
				transform.Translation.x = focus.x;
				transform.Translation.y = focus.y;

				Focus = { focus.x, focus.y, 0.0f };
				Right = { 1.0f, 0.0f, 0.0f };
				Up = { 0.0f, 1.0f, 0.0f };
				Back = { 0.0f, 0.0f, 1.0f };
				Yaw = 0.0f;
				return;
			}

			// Ease in and out, so the tilt starts and stops softly.
			const float eased = Blend * Blend * (3.0f - 2.0f * Blend);
			const float tilt = glm::radians(framing.TiltDegrees) * eased;
			// Turned by the yaw about +Z: "north" is where the camera looks along the ground.
			const float yaw = glm::radians(framing.YawDegrees) * eased;
			const glm::vec3 right{ std::cos(yaw), std::sin(yaw), 0.0f };
			const glm::vec3 north{ -std::sin(yaw), std::cos(yaw), 0.0f };
			const glm::vec3 forward = north * std::sin(tilt) + glm::vec3(0.0f, 0.0f, -std::cos(tilt));
			const glm::vec3 up = north * std::cos(tilt) + glm::vec3(0.0f, 0.0f, std::sin(tilt));

			// The distance at which the perspective camera sees OrthographicSize units vertically, so both
			// projections show the same amount of the scene at the focus point.
			const float fieldOfView = framing.FieldOfView;
			const float viewHeight = camera.OrthographicSize * std::max(0.05f, framing.ViewScale) * std::max(0.1f, CinematicZoom);
			const float distance = viewHeight / (2.0f * std::tan(glm::radians(fieldOfView) * 0.5f));

			camera.Projection = Perspective ? ProjectionType::Perspective : ProjectionType::Orthographic;
			camera.PerspectiveFOV = fieldOfView;
			camera.PerspectiveNear = 0.5f;
			camera.PerspectiveFar = distance + 200.0f;
			camera.OrthographicNear = 0.1f;
			camera.OrthographicFar = distance + 200.0f;
			// The depth of field focuses on what the camera looks at (the target's terrace plus the rise), not on where
			// the view meets z = 0, which lies metres behind it when the camera aims above the ground.
			camera.FocusDistance = distance;

			const glm::vec3 focusPoint{ focus.x, focus.y, height };
			transform.Translation = focusPoint - forward * distance;
			// A camera turned by +tilt about X looks from -Z towards +Y (north) and down; the yaw then turns it about
			// +Z (Euler angles apply X first, then Z).
			transform.Rotation = { tilt, 0.0f, yaw };
			s_Tilted = true;

			Focus = focusPoint;
			Right = right;
			Up = up;
			Back = -forward;
			Yaw = yaw;
		}

	private:
		// CinematicShake as an offset along the view plane: a few sines of different speeds, so it reads as a
		// rumble rather than a wobble; the camera's rotation is left alone (the focus point stays where it is).
		static glm::vec3 GetShakeOffset()
		{
			if (CinematicShake <= 0.0f)
			{
				return glm::vec3(0.0f);
			}
			const float t = s_ShakeTime;
			const float x = 0.6f * std::sin(t * 47.0f) + 0.3f * std::sin(t * 89.0f + 1.3f) + 0.1f * std::sin(t * 151.0f + 0.4f);
			const float y = 0.6f * std::sin(t * 53.0f + 2.1f) + 0.3f * std::sin(t * 97.0f + 0.7f) + 0.1f * std::sin(t * 139.0f + 2.9f);
			return (Right * x + Up * y) * CinematicShake;
		}

		static bool KeyPressed(KeyCode key, bool& previous)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && previous == false;
			previous = down;
			return pressed;
		}

		static inline bool s_PreviousV = false;
		static inline bool s_PreviousO = false;
		static inline bool s_Tilted = false;
		static inline SceneCamera s_TopDown;
		static inline float s_TopDownZ = 0.0f;
		static inline float s_ShakeTime = 0.0f;
	};
}
