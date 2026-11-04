#pragma once

#include "Hydro.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Common/DioramaCamera.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

// The battle's stage: the camera, its shake, and the mapping between layout units and the world.

namespace Hydro
{
	// Rules and HUD work in layout units (LayoutHeight tall, (0, 0) mid-screen), mapped to the world each frame: the HUD on
	// the view plane, actors where the camera ray meets the floor (scripts/game/battle/stage.py does the same for the builder).
	class BattleStage
	{
	public:
		static constexpr float LayoutHeight = 18.0f;
		static constexpr float HudLift = 1.0f;     // metres from the view plane towards the camera
		static constexpr float GroundHeight = 0.0f;
		static constexpr float ShakeTime = 0.22f;

		// The script's camera properties.
		struct Settings
		{
			float Tilt = 73.0f;          // degrees from straight down
			float FieldOfView = 30.0f;   // vertical, degrees
			glm::vec3 Focus{ 0.0f };     // the world point at the centre of the screen
			float ViewHeight = 10.0f;    // metres seen from the bottom of the screen to the top at the focus point
			float ShakeStrength = 1.0f;
		};

		// This frame's camera.
		struct View
		{
			glm::vec3 Focus{ 0.0f };
			glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
			glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
			glm::vec3 Forward{ 0.0f, 0.0f, -1.0f };
			glm::vec3 Back{ 0.0f, 0.0f, 1.0f };
			glm::vec3 Position{ 0.0f };
			float Distance = 1.0f;
			float Scale = 1.0f; // metres per layout unit at the focus point
		};

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			m_Camera = BattleEntities::Find(scene, "Camera");
		}

		const View& GetView() const { return m_View; }
		float GetScale() const { return m_View.Scale; }

		// Places the camera from the settings and the shake, and sets up this frame's mapping.
		void Update(const Settings& settings, float time, float deltaTime)
		{
			m_ShakeLeft = std::max(0.0f, m_ShakeLeft - deltaTime);
			m_ShakeStrength = settings.ShakeStrength;
			const float tilt = glm::radians(std::clamp(settings.Tilt, 0.0f, 89.0f));
			const float fieldOfView = std::clamp(settings.FieldOfView, 5.0f, 90.0f);
			const float viewHeight = std::max(settings.ViewHeight, 0.1f);
			View& view = m_View;
			view.Forward = { 0.0f, std::sin(tilt), -std::cos(tilt) };
			view.Up = { 0.0f, std::cos(tilt), std::sin(tilt) };
			view.Right = { 1.0f, 0.0f, 0.0f };
			view.Back = -view.Forward;
			view.Distance = viewHeight / (2.0f * std::tan(glm::radians(fieldOfView) * 0.5f));
			view.Scale = viewHeight / LayoutHeight;
			view.Focus = settings.Focus;
			if (m_ShakeLeft > 0.0f)
			{
				// The HUD moves with the camera, so the shake leaves it still on screen.
				const float amount = m_ShakeAmount * (m_ShakeLeft / ShakeTime);
				view.Focus += view.Right * (std::sin(time * 91.0f) * amount) + view.Up * (std::cos(time * 73.0f) * amount * 0.6f);
			}
			view.Position = view.Focus - view.Forward * view.Distance;

			Scene& scene = *m_Scene;
			if (BattleEntities::Alive(scene, m_Camera) && scene.m_Registry.has<CameraComponent>(m_Camera))
			{
				DioramaView::PlaceInWorld(Entity{ m_Camera, &scene }, view.Position, { tilt, 0.0f, 0.0f });
				SceneCamera& camera = scene.m_Registry.get<CameraComponent>(m_Camera).Camera;
				camera.Projection = ProjectionType::Perspective;
				camera.PerspectiveFOV = fieldOfView;
				camera.PerspectiveNear = 0.5f;
				camera.PerspectiveFar = view.Distance + 200.0f;
				camera.OrthographicSize = viewHeight;
			}
		}

		void Shake(float metres)
		{
			m_ShakeAmount = std::max(m_ShakeAmount * (m_ShakeLeft / ShakeTime), metres * m_ShakeStrength);
			m_ShakeLeft = ShakeTime;
		}

		glm::vec3 ScreenPoint(const glm::vec2& layout, float lift = HudLift) const
		{
			return m_View.Focus + (m_View.Right * layout.x + m_View.Up * layout.y) * m_View.Scale + m_View.Back * lift;
		}

		glm::vec3 GroundPoint(const glm::vec2& layout) const
		{
			const glm::vec3 onPlane = ScreenPoint(layout, 0.0f);
			const glm::vec3 direction = onPlane - m_View.Position;
			if (std::abs(direction.z) < 1.0e-5f)
			{
				return onPlane;
			}
			const float t = (GroundHeight - m_View.Position.z) / direction.z;
			return m_View.Position + direction * t;
		}

		// Where a world point shows on screen, in layout units.
		glm::vec2 LayoutOf(const glm::vec3& world) const
		{
			const glm::vec3 direction = world - m_View.Position;
			const float along = glm::dot(direction, m_View.Forward);
			if (std::abs(along) < 1.0e-5f)
			{
				return glm::vec2(0.0f);
			}
			const glm::vec3 offset = m_View.Position + direction * (m_View.Distance / along) - m_View.Focus;
			return glm::vec2(glm::dot(offset, m_View.Right), glm::dot(offset, m_View.Up)) / m_View.Scale;
		}

		glm::vec2 PlaneLayoutOf(const glm::vec3& world) const
		{
			const glm::vec3 offset = world - m_View.Focus;
			return glm::vec2(glm::dot(offset, m_View.Right), glm::dot(offset, m_View.Up)) / m_View.Scale;
		}

		// Upright sprites face the camera, so their up is the camera's up.
		glm::vec3 Above(const glm::vec2& ground, float metres) const
		{
			return GroundPoint(ground) + m_View.Up * metres;
		}

		glm::vec2 ScreenAbove(const glm::vec2& ground, float metres) const
		{
			return LayoutOf(Above(ground, metres));
		}

		// Back by the pivot, then down the leaning sprite by the feet's height in the cell. `size` in metres.
		glm::vec3 StandOffset(float foot, float pivot, float size) const
		{
			return glm::vec3(0.0f, pivot * size, 0.0f) - m_View.Up * (foot * size);
		}

		// Where an actor placed in the editor stands, in layout units.
		glm::vec2 ActorLayout(entt::entity entity, float foot, float pivot) const
		{
			Scene& scene = *m_Scene;
			if (BattleEntities::Alive(scene, entity) == false)
			{
				return glm::vec2(0.0f);
			}
			const TransformComponent& transform = scene.m_Registry.get<TransformComponent>(entity);
			const glm::vec3 feet = scene.GetWorldPosition(Entity{ entity, &scene }) - StandOffset(foot, pivot, transform.Scale.y);
			return LayoutOf(glm::vec3(feet.x, feet.y, GroundHeight));
		}

		// The contact shadow is an entity of its own so it stays on the floor while the sprite lifts (a floating wisp).
		void PlaceActor(entt::entity entity, entt::entity shadow, const glm::vec2& ground, float lift, float foot, float pivot) const
		{
			Scene& scene = *m_Scene;
			const glm::vec3 feet = GroundPoint(ground);
			if (BattleEntities::Alive(scene, entity))
			{
				const float size = scene.m_Registry.get<TransformComponent>(entity).Scale.y;
				scene.SetWorldPosition(Entity{ entity, &scene }, feet + StandOffset(foot, pivot, size) + glm::vec3(0.0f, 0.0f, lift));
			}
			if (BattleEntities::Alive(scene, shadow))
			{
				scene.SetWorldPosition(Entity{ shadow, &scene }, feet);
			}
		}

		// The overlay sprites (cursor, turn marker, popup digits) keep their place on screen in layout units, so the camera's
		// shake and moves leave them still. Captured once the camera is placed.
		void CaptureLayout()
		{
			Scene& scene = *m_Scene;
			m_Layout.clear();
			auto view = scene.m_Registry.view<TransformComponent, SpriteRendererComponent>();
			for (auto entity : view)
			{
				if (view.get<SpriteRendererComponent>(entity).Overlay == false)
				{
					continue;
				}
				const TransformComponent& transform = view.get<TransformComponent>(entity);
				LayoutItem item;
				item.Position = PlaneLayoutOf(scene.GetWorldPosition(Entity{ entity, &scene }));
				item.Size = glm::vec2(transform.Scale) / m_View.Scale;
				m_Layout[entity] = item;
			}
		}

		// An overlay sprite's place; ApplyLayout puts it in the world.
		void SetPosition(entt::entity entity, const glm::vec2& position)
		{
			const auto found = m_Layout.find(entity);
			if (found != m_Layout.end())
			{
				found->second.Position = position;
			}
		}

		void ApplyLayout()
		{
			Scene& scene = *m_Scene;
			for (const auto& [entity, item] : m_Layout)
			{
				if (BattleEntities::Alive(scene, entity) == false)
				{
					continue;
				}
				scene.SetWorldPosition(Entity{ entity, &scene }, ScreenPoint(item.Position));
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(entity);
				transform.Scale.x = item.Size.x * m_View.Scale;
				transform.Scale.y = item.Size.y * m_View.Scale;
			}
		}

	private:
		struct LayoutItem
		{
			glm::vec2 Position{ 0.0f };
			glm::vec2 Size{ 1.0f };
		};

		Scene* m_Scene = nullptr;
		entt::entity m_Camera = entt::null;
		View m_View;
		float m_ShakeLeft = 0.0f;
		float m_ShakeAmount = 0.0f;
		float m_ShakeStrength = 1.0f;
		std::unordered_map<entt::entity, LayoutItem> m_Layout;
	};
}
