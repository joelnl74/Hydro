#pragma once
#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

// Native scripts of the StatueVillage scene (Assets/Scenes/StatueVillage.hyscene). They are registered by
// RegisterExampleScripts, so they also show up under Add Component > Script. They only use what the engine
// has: components, SpriteAnimator, Input and Renderer::SetSettings. No physics: the walker moves along its
// waypoints and nothing collides.
//
// The scene is 60 x 36 tiles of 32 px = 60 x 36 world units, centred on the origin (tile (30, 18) is
// world (0, 0); world x = tile x - 30, world y = 18 - tile y).
namespace Hydro
{
	struct VillageMap
	{
		static constexpr float HalfWidth = 30.0f;
		static constexpr float HalfHeight = 18.0f;
	};

	// Sorts a sprite among the others by its feet: the lower on screen, the more in front. The scene's static
	// sprites are written with the same rule (SortOrder = 4 * tile y of the feet), so a character walking past
	// a tree or house is drawn in front of it or behind it correctly. Needs a feet pivot (a sprite sheet whose
	// DefaultPivot is [0.5, 0]), so that the entity's y is the feet's y.
	class VillageYSortScript : public ScriptableEntity
	{
	protected:
		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			Entity entity = GetEntity();
			if (entity.HasComponent<SpriteRendererComponent>() == false)
			{
				return;
			}

			const float feetY = GetComponent<TransformComponent>().Translation.y;
			GetComponent<SpriteRendererComponent>().SortOrder = (int32_t)std::lround((VillageMap::HalfHeight - feetY) * 4.0f);
		}
	};

	// Walks the entity around a loop of waypoints, stopping at each. No physics, no path finding: a straight
	// line from one waypoint to the next, so lay the waypoints along paths. The entity is tagged
	// "Walker <Name>" and its waypoints are entities tagged "Waypoint <Name> <n> [Facing] [Seconds]", for
	// example "Waypoint Apron 3 Left 3.5": after reaching waypoint 3 the walker turns left and waits 3.5 s
	// (Facing is Up, Down, Left or Right; without it the walker keeps the direction it arrived in, and
	// without Seconds it waits 1 s). Only the waypoints' positions are used. After the last waypoint it walks
	// back to the first. Plays "Walk<Facing>" / "Idle<Facing>" from the entity's Sprite Animator. Every
	// walker gets a slightly different pace from its name, so a crowd does not move in step.
	class WaypointWalkScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Scene* scene = GetEntity().GetScene();
			const std::string& tag = GetComponent<TagComponent>().Tag;
			const std::string name = tag.size() > 7 ? tag.substr(7) : std::string();
			const std::string prefix = "Waypoint " + name + " ";

			std::vector<std::pair<int, Stop>> numbered;
			auto view = scene->m_Registry.view<TagComponent, TransformComponent>();
			for (auto entity : view)
			{
				const std::string& other = view.get<TagComponent>(entity).Tag;
				if (other.compare(0, prefix.size(), prefix) != 0)
				{
					continue;
				}

				const glm::vec3& position = view.get<TransformComponent>(entity).Translation;
				Stop stop;
				stop.Position = glm::vec2(position.x, position.y);
				const char* rest = other.c_str() + prefix.size();
				char* end = nullptr;
				const int number = (int)std::strtol(rest, &end, 10);
				std::string facing;
				for (const char* word : { "Up", "Down", "Left", "Right" })
				{
					if (std::string(end).find(std::string(" ") + word) != std::string::npos)
					{
						facing = word;
					}
				}
				stop.Facing = facing;
				const size_t lastSpace = other.find_last_of(' ');
				const float seconds = (float)std::atof(other.c_str() + lastSpace + 1);
				if (seconds > 0.0f && lastSpace + 1 > (size_t)(end - other.c_str()))
				{
					stop.Wait = seconds;
				}
				numbered.push_back({ number, stop });
			}

			std::sort(numbered.begin(), numbered.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			for (const auto& stop : numbered)
			{
				m_Stops.push_back(stop.second);
			}

			// A pace between 1.1 and 1.6 tiles per second, picked from the name.
			uint32_t hash = 2166136261u;
			for (char c : name)
			{
				hash = (hash ^ (uint8_t)c) * 16777619u;
			}
			m_Speed = 1.1f + (float)(hash % 100) / 200.0f;

			// Starts on the first waypoint, waits there, then walks to the second.
			m_Index = 0;
			if (m_Stops.empty() == false)
			{
				m_Pause = m_Stops[0].Wait * 0.5f + (float)(hash % 7) * 0.2f;
				if (m_Stops[0].Facing.empty() == false)
				{
					m_Facing = m_Stops[0].Facing;
				}
			}
		}

		void OnUpdate(float deltaTime) override
		{
			if (m_Stops.size() < 2)
			{
				return;
			}

			Entity self = GetEntity();
			if (m_Pause > 0.0f)
			{
				m_Pause -= deltaTime;
				SpriteAnimator::Play(self, "Idle" + m_Facing);
				if (m_Pause <= 0.0f)
				{
					m_Index = (m_Index + 1) % (int)m_Stops.size();
				}
				return;
			}

			auto& transform = GetComponent<TransformComponent>();
			const glm::vec2 position{ transform.Translation.x, transform.Translation.y };
			const Stop& target = m_Stops[m_Index];
			const glm::vec2 toTarget = target.Position - position;
			const float distance = glm::length(toTarget);
			const float step = m_Speed * deltaTime;

			if (distance <= step)
			{
				transform.Translation.x = target.Position.x;
				transform.Translation.y = target.Position.y;
				if (target.Facing.empty() == false)
				{
					m_Facing = target.Facing;
				}
				m_Pause = target.Wait;
				SpriteAnimator::Play(self, "Idle" + m_Facing);
				return;
			}

			const glm::vec2 direction = toTarget / distance;
			transform.Translation.x += direction.x * step;
			transform.Translation.y += direction.y * step;
			if (std::abs(direction.y) > std::abs(direction.x))
			{
				m_Facing = direction.y < 0.0f ? "Down" : "Up";
			}
			else
			{
				m_Facing = direction.x < 0.0f ? "Left" : "Right";
			}

			SpriteAnimator::Play(self, "Walk" + m_Facing);
		}

	private:
		struct Stop
		{
			glm::vec2 Position{ 0.0f };
			std::string Facing;  // empty: keep the direction the walker arrived in
			float Wait = 1.0f;   // seconds standing at the waypoint
		};

		std::vector<Stop> m_Stops;
		int m_Index = 0;
		float m_Pause = 0.0f;
		float m_Speed = 1.4f;   // units (tiles) per second: a stroll
		std::string m_Facing = "Down";
	};

	// Put it on the camera: it glides after the first entity tagged "Player", stays inside the 60 x 36 map (the
	// view never shows past the edge) and zooms with - and = (the camera's OrthographicSize, the world units
	// seen vertically). Scripts run in play mode only.
	class VillageCameraScript : public ScriptableEntity
	{
	protected:
		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();
			auto& transform = GetComponent<TransformComponent>();
			auto& camera = GetComponent<CameraComponent>();

			if (Input::IsKeyPressed(KeyCode::Minus))
			{
				camera.Camera.OrthographicSize += m_ZoomSpeed * deltaTime;
			}
			if (Input::IsKeyPressed(KeyCode::Equal))
			{
				camera.Camera.OrthographicSize -= m_ZoomSpeed * deltaTime;
			}
			camera.Camera.OrthographicSize = std::clamp(camera.Camera.OrthographicSize, m_MinSize, 2.0f * VillageMap::HalfHeight);

			if (m_Target == entt::null || scene->m_Registry.valid(m_Target) == false)
			{
				m_Target = FindTarget(*scene);
				m_Snap = true;
			}

			glm::vec2 goal{ transform.Translation.x, transform.Translation.y };
			if (m_Target != entt::null && scene->m_Registry.has<TransformComponent>(m_Target))
			{
				const glm::vec3& position = scene->m_Registry.get<TransformComponent>(m_Target).Translation;
				goal = { position.x, position.y + m_LookAhead };
			}

			// Keep the whole view inside the map; when it is larger than the map, centre on it.
			const float aspect = scene->GetViewportAspectRatio();
			const float halfHeight = 0.5f * camera.Camera.OrthographicSize;
			const float halfWidth = halfHeight * (aspect > 0.0f ? aspect : 1.0f);
			const float limitX = std::max(0.0f, VillageMap::HalfWidth - halfWidth);
			const float limitY = std::max(0.0f, VillageMap::HalfHeight - halfHeight);
			goal.x = std::clamp(goal.x, -limitX, limitX);
			goal.y = std::clamp(goal.y, -limitY, limitY);

			const float t = m_Snap ? 1.0f : 1.0f - std::exp(-m_Smoothing * deltaTime);
			transform.Translation.x += (goal.x - transform.Translation.x) * t;
			transform.Translation.y += (goal.y - transform.Translation.y) * t;
			m_Snap = false;
		}

	private:
		static entt::entity FindTarget(Scene& scene)
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
		float m_Smoothing = 8.0f;
		float m_LookAhead = 1.5f;   // aim at the player's chest, not the feet
		float m_MinSize = 8.0f;     // closest zoom, world units visible vertically
		float m_ZoomSpeed = 10.0f;  // units per second while - or = is held
	};

	// The scene's lighting test bench. Put it on one entity. It finds the scene's lights by their tags and
	// drives them from the keyboard (play mode):
	//   1 / 2 / 3   day / dusk / night: the global light and how strongly the lamps and windows burn
	//   L           all point lights on / off
	//   H           the blob shadows on / off (RendererSettings::Shadows)
	//   F           flame flicker on / off (a few percent, deterministic: no random jumps)
	//   B           the statue <-> a brazier A/B check (the statue is the default)
	//   [ and ]     point light radius smaller / larger (x0.9 / x1.1 per press, 0.5 to 2)
	// There is no in-game text, so the status line goes to the Console window; the Stats window shows the lit
	// views, light and shadow counts and their GPU time. Tags it looks for: "Ambient Light" (the global
	// light), "Light Lamp / Window / Market / Torch / Pond / Brazier ..." (point lights), and sprites whose tag
	// starts with "Statue" or "Brazier".
	class VillageDirectorScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Scene* scene = GetEntity().GetScene();

			auto lights = scene->m_Registry.view<TagComponent, Light2DComponent>();
			for (auto entity : lights)
			{
				const std::string& tag = lights.get<TagComponent>(entity).Tag;
				const Light2DComponent& light = lights.get<Light2DComponent>(entity);
				if (light.Type == Light2DType::Global)
				{
					if (tag == "Ambient Light")
					{
						m_Ambient = entity;
					}
					continue;
				}

				PointLight point;
				point.Entity = entity;
				point.BaseRadius = light.Radius;
				point.BaseIntensity = light.Intensity;
				point.Phase = (float)(m_Lights.size()) * 1.7f;
				if (tag.rfind("Light Lamp", 0) == 0)
				{
					point.Kind = LightKind::Lamp;
				}
				else if (tag.rfind("Light Window", 0) == 0)
				{
					point.Kind = LightKind::Window;
				}
				else if (tag.rfind("Light Market", 0) == 0)
				{
					point.Kind = LightKind::Market;
				}
				else if (tag.rfind("Light Torch", 0) == 0)
				{
					point.Kind = LightKind::Torch;
				}
				else if (tag.rfind("Light Pond", 0) == 0)
				{
					point.Kind = LightKind::Pond;
				}
				else if (tag.rfind("Light Brazier", 0) == 0)
				{
					point.Kind = LightKind::Brazier;
					point.BaseIntensity = 1.15f; // saved as 0 so the editor view stays brazier-free
				}
				m_Lights.push_back(point);
			}

			auto sprites = scene->m_Registry.view<TagComponent, SpriteRendererComponent>();
			for (auto entity : sprites)
			{
				const std::string& tag = sprites.get<TagComponent>(entity).Tag;
				if (tag.rfind("Statue", 0) == 0)
				{
					m_Statue.push_back({ entity, sprites.get<SpriteRendererComponent>(entity).Color.a });
				}
				else if (tag.rfind("Brazier", 0) == 0)
				{
					m_Brazier.push_back({ entity, 1.0f }); // saved invisible (alpha 0)
				}
			}

			HY_INFO("Village: 1/2/3 day-dusk-night, L lights, H shadows, F flicker, B statue/brazier, [ ] light radius, - = zoom, WASD walk");
			ApplyPreset();
			ReportStatus();
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;

			bool changed = false;
			if (KeyPressed(KeyCode::D1, m_Previous[0]))
			{
				m_Preset = 0;
				changed = true;
			}
			if (KeyPressed(KeyCode::D2, m_Previous[1]))
			{
				m_Preset = 1;
				changed = true;
			}
			if (KeyPressed(KeyCode::D3, m_Previous[2]))
			{
				m_Preset = 2;
				changed = true;
			}
			if (KeyPressed(KeyCode::L, m_Previous[3]))
			{
				m_LightsOn = m_LightsOn == false;
				changed = true;
			}
			if (KeyPressed(KeyCode::H, m_Previous[4]))
			{
				RendererSettings settings = Renderer::GetSettings();
				settings.Shadows = settings.Shadows == false;
				Renderer::SetSettings(settings);
				changed = true;
			}
			if (KeyPressed(KeyCode::F, m_Previous[5]))
			{
				m_Flicker = m_Flicker == false;
				changed = true;
			}
			if (KeyPressed(KeyCode::B, m_Previous[6]))
			{
				m_BrazierOn = m_BrazierOn == false;
				changed = true;
			}
			if (KeyPressed(KeyCode::LeftBracket, m_Previous[7]))
			{
				m_RadiusScale = std::max(0.5f, m_RadiusScale * 0.9f);
				changed = true;
			}
			if (KeyPressed(KeyCode::RightBracket, m_Previous[8]))
			{
				m_RadiusScale = std::min(2.0f, m_RadiusScale * 1.1f);
				changed = true;
			}

			if (changed)
			{
				ApplyPreset();
				ReportStatus();
			}

			UpdateLights();
		}

	private:
		enum class LightKind : uint8_t
		{
			Lamp,
			Window,
			Market,
			Torch,
			Pond,
			Brazier
		};

		struct PointLight
		{
			entt::entity Entity = entt::null;
			LightKind Kind = LightKind::Lamp;
			float BaseIntensity = 1.0f;
			float BaseRadius = 1.0f;
			float Phase = 0.0f;
		};

		struct Faded
		{
			entt::entity Entity = entt::null;
			float BaseAlpha = 1.0f;
		};

		// One global light colour and strength, and how much of their normal strength the lamps and windows keep.
		// The light buffer is multiplied twice over, so a global light of 0.5 white leaves the sprites as they are:
		// the night is darker than that, the day slightly brighter.
		struct Preset
		{
			const char* Name;
			glm::vec3 Color;
			float Intensity;
			float LampFactor;
			float WindowFactor;
		};

		static const Preset& GetPreset(int index)
		{
			static const Preset presets[3] = {
				{ "Day", { 1.00f, 1.00f, 1.00f }, 1.00f, 0.0f, 0.0f },
				{ "Dusk", { 0.86f, 0.72f, 0.70f }, 0.62f, 0.75f, 0.8f },
				{ "Night", { 0.50f, 0.60f, 0.85f }, 0.42f, 1.0f, 1.0f }
			};
			return presets[index];
		}

		static bool KeyPressed(KeyCode key, bool& previous)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && previous == false;
			previous = down;
			return pressed;
		}

		// Cheap smooth noise in -1..1 from a few sines, different for each light (phase): no random jumps.
		static float Noise(float time, float phase)
		{
			return 0.5f * std::sin(time * 7.1f + phase) + 0.3f * std::sin(time * 13.7f + phase * 1.7f) + 0.2f * std::sin(time * 3.3f + phase * 2.3f);
		}

		void ApplyPreset()
		{
			Scene* scene = GetEntity().GetScene();
			const Preset& preset = GetPreset(m_Preset);
			if (m_Ambient != entt::null && scene->m_Registry.valid(m_Ambient))
			{
				Light2DComponent& ambient = scene->m_Registry.get<Light2DComponent>(m_Ambient);
				ambient.Color = preset.Color;
				ambient.Intensity = preset.Intensity;
			}

			// Statue <-> brazier. Faded sprites are drawn with alpha 0.
			for (const Faded& faded : m_Statue)
			{
				SetAlpha(*scene, faded.Entity, m_BrazierOn ? 0.0f : faded.BaseAlpha);
			}
			for (const Faded& faded : m_Brazier)
			{
				SetAlpha(*scene, faded.Entity, m_BrazierOn ? faded.BaseAlpha : 0.0f);
			}
		}

		static void SetAlpha(Scene& scene, entt::entity entity, float alpha)
		{
			if (scene.m_Registry.valid(entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).Color.a = alpha;
			}
		}

		void UpdateLights()
		{
			Scene* scene = GetEntity().GetScene();
			const Preset& preset = GetPreset(m_Preset);
			for (const PointLight& point : m_Lights)
			{
				if (scene->m_Registry.valid(point.Entity) == false)
				{
					continue;
				}

				Light2DComponent& light = scene->m_Registry.get<Light2DComponent>(point.Entity);
				float factor = 0.0f;
				float flicker = 0.0f; // fraction of the intensity
				switch (point.Kind)
				{
					case LightKind::Lamp:
						factor = preset.LampFactor;
						flicker = 0.03f;
						break;
					case LightKind::Window:
						factor = preset.WindowFactor;
						break;
					case LightKind::Market:
						factor = preset.WindowFactor;
						flicker = 0.03f;
						break;
					case LightKind::Torch:
						factor = preset.LampFactor;
						flicker = 0.05f;
						break;
					case LightKind::Pond:
						factor = preset.LampFactor;
						flicker = 0.02f;
						break;
					case LightKind::Brazier:
						factor = m_BrazierOn ? 1.0f : 0.0f;
						flicker = 0.05f;
						break;
				}

				if (m_LightsOn == false)
				{
					factor = 0.0f;
				}
				if (m_Flicker == false)
				{
					flicker = 0.0f;
				}

				light.Intensity = point.BaseIntensity * factor * (1.0f + flicker * Noise(m_Time, point.Phase));
				light.Radius = point.BaseRadius * m_RadiusScale;
			}
		}

		void ReportStatus()
		{
			const Preset& preset = GetPreset(m_Preset);
			uint32_t active = 0;
			for (const PointLight& point : m_Lights)
			{
				float factor = point.Kind == LightKind::Lamp || point.Kind == LightKind::Torch || point.Kind == LightKind::Pond ? preset.LampFactor : preset.WindowFactor;
				if (point.Kind == LightKind::Brazier)
				{
					factor = m_BrazierOn ? 1.0f : 0.0f;
				}
				if (m_LightsOn && factor > 0.0f)
				{
					active++;
				}
			}

			HY_INFO("Village: {} | ambient {:.2f} | point lights {} of {} | shadows {} | flicker {} | centre {} | radius x{:.2f}",
				preset.Name, preset.Intensity, active, m_Lights.size(), Renderer::GetSettings().Shadows ? "on" : "off",
				m_Flicker ? "on" : "off", m_BrazierOn ? "brazier" : "statue", m_RadiusScale);
		}

		std::vector<PointLight> m_Lights;
		std::vector<Faded> m_Statue;
		std::vector<Faded> m_Brazier;
		entt::entity m_Ambient = entt::null;
		int m_Preset = 2; // night
		bool m_LightsOn = true;
		bool m_Flicker = true;
		bool m_BrazierOn = false;
		float m_RadiusScale = 1.0f;
		float m_Time = 0.0f;
		bool m_Previous[9] = {};
	};

	inline void RegisterVillageScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<VillageYSortScript>("VillageYSort");
		ScriptRegistry::Register<WaypointWalkScript>("WaypointWalk");
		ScriptRegistry::Register<VillageCameraScript>("VillageCamera");
		ScriptRegistry::Register<VillageDirectorScript>("VillageDirector");
	}
}
