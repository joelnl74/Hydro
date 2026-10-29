#pragma once
#include "Hydro.h"

#include <algorithm>
#include <cmath>

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
namespace Hydro
{
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
		static void ResetSceneFraming()
		{
			SceneTiltDegrees = 0.0f;
			SceneFieldOfView = 0.0f;
			SceneFocusLead = 0.0f;
			SceneFocusRise = 0.0f;
			HasSceneBounds = false;
			SceneBoundsMin = glm::vec2(0.0f);
			SceneBoundsMax = glm::vec2(0.0f);
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

		// Where the camera looks this frame, for world-space UI (MvpScripts.h): the focus point on the ground
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
		}

		// Places the camera entity looking at focus (a point on the ground, `height` above z 0: a raised terrace the
		// player stands on) for the current blend. Works in the world, so the camera may sit under a parent.
		static void Apply(Entity cameraEntity, const glm::vec2& focus, float height = 0.0f)
		{
			Scene& scene = *cameraEntity.GetScene();
			TransformComponent placed = cameraEntity.GetComponent<TransformComponent>();
			placed.Translation = scene.GetWorldPosition(cameraEntity);
			Apply(placed, cameraEntity.GetComponent<CameraComponent>(), focus, height);
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
				return;
			}

			// Ease in and out, so the tilt starts and stops softly.
			const float eased = Blend * Blend * (3.0f - 2.0f * Blend);
			const float tilt = glm::radians(SceneTiltDegrees > 0.0f ? SceneTiltDegrees : TiltDegrees) * eased;
			const glm::vec3 forward{ 0.0f, std::sin(tilt), -std::cos(tilt) };
			const glm::vec3 up{ 0.0f, std::cos(tilt), std::sin(tilt) };

			// The distance at which the perspective camera sees OrthographicSize units vertically, so both
			// projections show the same amount of the scene at the focus point.
			const float fieldOfView = SceneFieldOfView > 0.0f ? SceneFieldOfView : FieldOfView;
			const float viewHeight = camera.OrthographicSize;
			const float distance = viewHeight / (2.0f * std::tan(glm::radians(fieldOfView) * 0.5f));

			camera.Projection = Perspective ? ProjectionType::Perspective : ProjectionType::Orthographic;
			camera.PerspectiveFOV = fieldOfView;
			camera.PerspectiveNear = 0.5f;
			camera.PerspectiveFar = distance + 200.0f;
			camera.OrthographicNear = 0.1f;
			camera.OrthographicFar = distance + 200.0f;

			const glm::vec3 focusPoint{ focus.x, focus.y, height };
			transform.Translation = focusPoint - forward * distance;
			// A camera turned by +tilt about X looks from -Z towards +Y (north) and down.
			transform.Rotation = { tilt, 0.0f, 0.0f };
			s_Tilted = true;

			Focus = focusPoint;
			Right = { 1.0f, 0.0f, 0.0f };
			Up = up;
			Back = -forward;
		}

	private:
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
	};
}
