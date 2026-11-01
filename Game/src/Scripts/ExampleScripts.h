#pragma once
#include "Hydro.h"
#include "DioramaCamera.h"
#include "VillageScripts.h"
#include "BattleScripts.h"
#include "MvpScripts.h"
#include "ShrineScripts.h"

#include <algorithm>
#include <cmath>
#include <string>

// Small native scripts to try play mode with. HY_SCRIPT() registers them (the generated registration runs in
// OxygenEditorApplication::OnInit), so they show up under Add Component > Script and are saved with the scene;
// their HY_PROPERTY fields are edited in the inspector.
namespace Hydro
{
	// Turns the entity around Z (its own rotation, relative to its parent).
	HY_SCRIPT()
	class SpinScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			auto& transform = GetComponent<TransformComponent>();
			transform.Rotation.z += Speed * deltaTime;
		}

	public:
		HY_PROPERTY(Range(-50, 50), Tooltip("Radians per second around Z; negative turns the other way"))
		float Speed = 1.5f;
	};

	// Moves the entity up and down around where it started (relative to its parent: it bobs along with whatever it
	// is attached to).
	HY_SCRIPT()
	class BobScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			m_StartY = GetComponent<TransformComponent>().Translation.y;
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;
			auto& transform = GetComponent<TransformComponent>();
			transform.Translation.y = m_StartY + std::sin(m_Time * Frequency) * Height;
		}

	public:
		HY_PROPERTY(Range(0, 10), Tooltip("Metres up and down from where it started"))
		float Height = 0.5f;
		HY_PROPERTY(Range(0, 20), Tooltip("How fast it bobs (radians per second: 2 is about one bob every 3 seconds)"))
		float Frequency = 2.0f;

	private:
		float m_StartY = 0.0f;
		float m_Time = 0.0f;
	};

	// Walks a character with WASD or the arrow keys and picks its animation by direction:
	// "Walk<Facing>" while moving, "Idle<Facing>" when standing, Facing = Down/Right/Up/Left.
	// Needs a Sprite Animator whose set has those clips (Assets/Characters/*.hyanim in the Sandbox).
	HY_SCRIPT()
	class VillagerWalkScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			glm::vec2 move{ 0.0f };
			// A runtime UI canvas that captures input (UI::CapturesInput: a menu, a dialogue) owns the keys: the
			// character stands still (and faces the way it was).
			const bool inputAllowed = UI::CapturesInput(*GetEntity().GetScene()) == false;
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::A) || Input::IsKeyPressed(KeyCode::Left)))
			{
				move.x -= 1.0f;
			}
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::D) || Input::IsKeyPressed(KeyCode::Right)))
			{
				move.x += 1.0f;
			}
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::W) || Input::IsKeyPressed(KeyCode::Up)))
			{
				move.y += 1.0f;
			}
			if (inputAllowed && (Input::IsKeyPressed(KeyCode::S) || Input::IsKeyPressed(KeyCode::Down)))
			{
				move.y -= 1.0f;
			}

			// Sideways wins when moving diagonally.
			if (move.x < 0.0f)
			{
				m_Facing = "Left";
			}
			else if (move.x > 0.0f)
			{
				m_Facing = "Right";
			}
			else if (move.y > 0.0f)
			{
				m_Facing = "Up";
			}
			else if (move.y < 0.0f)
			{
				m_Facing = "Down";
			}

			const bool moving = move.x != 0.0f || move.y != 0.0f;
			// Safe every frame: a clip that is already playing carries on.
			SpriteAnimator::Play(GetEntity(), (moving ? "Walk" : "Idle") + m_Facing);

			// With a physics body the player moves by velocity, so walls stop it and it pushes crates;
			// without one (or while no world runs) it just slides the transform.

			uint32_t increasedSpeed = Input::IsKeyPressed(KeyCode::LeftShift) ? 2 : 1;
			const glm::vec2 velocity = moving ? glm::normalize(move) * (Speed + increasedSpeed * VillageState::PlayerSpeedScale()) : glm::vec2(0.0f);
			if (Physics2D::SetLinearVelocity(GetEntity(), velocity) == false && moving)
			{
				SetWorldPosition(GetWorldPosition() + glm::vec3(velocity * deltaTime, 0.0f));
			}
		}

	public:
		HY_PROPERTY(Range(0, 20), Tooltip("Units per second; Shift walks faster"))
		float Speed = 3.0f;

	private:
		std::string m_Facing = "Down";
	};

	// Sorts a sprite among the others by its height on screen: the lower it stands, the more in front it is
	// (SortOrder = round(10 - y), the same rule the LightingTest scene uses for its static sprites). For
	// characters that move; put it next to a script that walks them.
	HY_SCRIPT()
	class YSortScript : public ScriptableEntity
	{
	protected:
		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			Entity entity = GetEntity();
			if (entity.HasComponent<SpriteRendererComponent>() == false)
			{
				return;
			}

			const float y = GetWorldPosition().y;
			GetComponent<SpriteRendererComponent>().SortOrder = (int32_t)std::lround(10.0f - y);
		}
	};

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

				// The framing: the zone the target stands in, else the scene's. Entering another zone starts a blend
				// from the framing and pan area the camera had to the new ones over the zone's BlendSeconds, eased in
				// and out (smoothstep), so the camera starts and stops turning softly (the yaw turns the short way).
				DioramaFraming goalFraming = DioramaView::GetSceneFraming();
				const DioramaCameraZone* zone = DioramaView::FindZone(scene, { position.x, position.y });
				PanArea area;
				if (zone != nullptr)
				{
					goalFraming = DioramaView::ResolveZoneFraming(zone->Framing, goalFraming);
					area = { zone->HasPanBounds, zone->PanMin, zone->PanMax };
				}
				const entt::entity zoneEntity = zone != nullptr ? zone->Entity : entt::null;
				if (m_Snap)
				{
					m_Framing = goalFraming;
					m_FromFraming = goalFraming;
					m_ZoneEntity = zoneEntity;
					m_FromArea = area;
					m_ZoneBlend = 1.0f;
				}
				else if (zoneEntity != m_ZoneEntity)
				{
					m_ZoneEntity = zoneEntity;
					m_FromFraming = m_Framing;
					m_FromArea = m_Area;
					m_ZoneBlend = 0.0f;
					m_BlendSeconds = zone != nullptr ? zone->BlendSeconds : m_BlendSeconds;
				}
				m_Area = area;
				m_ZoneBlend = std::min(1.0f, m_ZoneBlend + deltaTime / std::max(0.05f, m_BlendSeconds));
				const float ease = m_ZoneBlend * m_ZoneBlend * (3.0f - 2.0f * m_ZoneBlend);
				m_Framing = DioramaFraming::Mix(m_FromFraming, goalFraming, ease);

				// Ahead of the target along the way the camera looks on the ground.
				const float yaw = glm::radians(m_Framing.YawDegrees) * DioramaView::Blend;
				const glm::vec2 ahead{ -std::sin(yaw), std::cos(yaw) };
				const float lead = LookAhead + m_Framing.FocusLead * DioramaView::Blend;
				const glm::vec2 wanted = glm::vec2(position.x, position.y) + ahead * lead;
				// Inside the old pan area and inside the new one, mixed by the same ease: the pan moves over smoothly.
				const glm::vec2 goal = glm::mix(Clamp(wanted, m_FromArea), Clamp(wanted, m_Area), ease);

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
		// Where the focus may go: a zone's pan area, or the scene's bounds when HasPan is false.
		struct PanArea
		{
			bool HasPan = false;
			glm::vec2 Min{ 0.0f };
			glm::vec2 Max{ 0.0f };
		};

		glm::vec2 Clamp(const glm::vec2& point, const PanArea& area) const
		{
			return area.HasPan ? glm::clamp(point, area.Min, area.Max) : DioramaView::ClampFocus(point, Bounds);
		}

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
		DioramaFraming m_Framing;      // the framing now
		DioramaFraming m_FromFraming;  // and where the blend into the current zone started
		PanArea m_Area;                // the current zone's pan area
		PanArea m_FromArea;            // the previous one, blended out
		entt::entity m_ZoneEntity = entt::null;
		float m_ZoneBlend = 1.0f;      // 0 .. 1 through the blend into the current zone
		float m_BlendSeconds = 0.8f;   // its length: the zone entered (leaving every zone keeps the last one's)
	};
}
