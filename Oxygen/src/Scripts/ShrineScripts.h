#pragma once
#include "Hydro.h"
#include "DioramaCamera.h"
#include "VillageScripts.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// The Shrine Glade (Assets/Scenes/ShrineGlade.hyscene): the first scene that mixes meshes (terraces,
// retaining walls, stairs, the imported shrine and stone lanterns) with upright sprites (trees, props,
// characters). See docs/TestBed.md, "Shrine Glade".
namespace Hydro
{
	// The walkable ground of a scene, read from its MeshComponents every time it is asked: the top of the
	// highest built-in shape (or model, as its bounding box) under a point. Shapes may be turned about Z only
	// (their Rotation.z), as blockout is. A model whose geometry starts well above its entity's base (a tree's
	// crown) is overhead, not ground: things walk under it. Game code for now; a collision system replaces it.
	struct DioramaGround
	{
		// No ground at all here (outside the map).
		static constexpr float None = -1.0e9f;
		// A model whose lowest point is this far above its entity's base is overhead (metres).
		static constexpr float OverheadClearance = 2.0f;

		static float HeightAt(Scene& scene, const glm::vec2& point)
		{
			float height = None;
			auto meshes = scene.m_Registry.view<TransformComponent, MeshComponent>();
			for (auto entity : meshes)
			{
				const auto [transform, mesh] = meshes.get<TransformComponent, MeshComponent>(entity);
				height = std::max(height, SurfaceAt(transform, mesh, point));
			}
			return height;
		}

	private:
		// The top of one mesh at `point`, or None when the point isn't over it.
		static float SurfaceAt(const TransformComponent& transform, const MeshComponent& mesh, const glm::vec2& point)
		{
			// The point in the shape's own frame (before its scale).
			const glm::vec2 offset = point - glm::vec2(transform.Translation);
			const float c = std::cos(-transform.Rotation.z);
			const float s = std::sin(-transform.Rotation.z);
			const glm::vec3 scale = glm::max(glm::abs(transform.Scale), glm::vec3(1.0e-4f));
			const glm::vec2 local{ (offset.x * c - offset.y * s) / scale.x, (offset.x * s + offset.y * c) / scale.y };
			const float base = transform.Translation.z;

			if ((uint64_t)mesh.Model != 0)
			{
				const Ref<Mesh> model = AssetManager::GetLoadedAsset<Mesh>(mesh.Model);
				if (model == nullptr)
				{
					return None;
				}
				const glm::vec3& min = model->GetBoundsMin();
				const glm::vec3& max = model->GetBoundsMax();
				if (min.z * scale.z > OverheadClearance)
				{
					return None;
				}
				if (local.x < min.x || local.x > max.x || local.y < min.y || local.y > max.y)
				{
					return None;
				}
				return base + max.z * scale.z;
			}

			const glm::vec3& size = mesh.Size;
			const glm::vec2 half{ size.x * 0.5f, size.y * 0.5f };
			if (mesh.Shape == MeshShape::Cylinder)
			{
				const float u = half.x > 0.0f ? local.x / half.x : 2.0f;
				const float v = half.y > 0.0f ? local.y / half.y : 2.0f;
				return u * u + v * v <= 1.0f ? base + size.z * scale.z : None;
			}
			if (std::abs(local.x) > half.x || std::abs(local.y) > half.y)
			{
				return None;
			}

			// 0 at the front (-Y) edge, 1 at the back.
			const float along = size.y > 0.0f ? std::clamp((local.y + half.y) / size.y, 0.0f, 1.0f) : 0.0f;
			switch (mesh.Shape)
			{
				case MeshShape::Plane:
					return base;
				case MeshShape::Ramp:
					return base + along * size.z * scale.z;
				case MeshShape::Stairs:
				{
					const uint32_t steps = std::max(mesh.Steps, 1u);
					const float step = std::min(std::floor(along * (float)steps), (float)steps - 1.0f);
					return base + (step + 1.0f) / (float)steps * size.z * scale.z;
				}
				default:
					return base + size.z * scale.z;
			}
		}
	};

	// Walks the player like VillagerWalk (WASD / arrows, Shift to hurry, Walk<Facing> / Idle<Facing> clips),
	// but over the diorama's ground: its feet follow the height of the terraces, stairs and bridge under them
	// (DioramaGround), it can't climb or drop more than a step, and it doesn't walk into the solid box
	// colliders of other entities (trees, lanterns, people). No physics body: put it on an entity without a
	// Rigidbody 2D.
	class DioramaWalkScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			auto& transform = GetComponent<TransformComponent>();
			const float ground = DioramaGround::HeightAt(scene, glm::vec2(transform.Translation));
			if (ground > DioramaGround::None)
			{
				transform.Translation.z = ground;
			}
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			auto& transform = GetComponent<TransformComponent>();

			glm::vec2 move{ 0.0f };
			const bool inputAllowed = UIState::MenuOpen == false;
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
			SpriteAnimator::Play(GetEntity(), (moving ? "Walk" : "Idle") + m_Facing);

			const glm::vec2 position{ transform.Translation };
			if (moving)
			{
				const float speed = Input::IsKeyPressed(KeyCode::LeftShift) ? m_Speed * 1.7f : m_Speed;
				const glm::vec2 step = glm::normalize(move) * speed * std::min(deltaTime, 0.05f);
				// The whole step, else slide along a wall on one axis.
				const glm::vec2 tries[3] = { position + step, position + glm::vec2(step.x, 0.0f), position + glm::vec2(0.0f, step.y) };
				for (const glm::vec2& next : tries)
				{
					if (CanStand(scene, next, transform.Translation.z))
					{
						transform.Translation.x = next.x;
						transform.Translation.y = next.y;
						break;
					}
				}
			}

			// Feet on the ground: eased a little, so a stair step is a quick lift instead of a jump.
			const float ground = DioramaGround::HeightAt(scene, glm::vec2(transform.Translation));
			if (ground > DioramaGround::None)
			{
				const float t = 1.0f - std::exp(-m_HeightEasing * deltaTime);
				transform.Translation.z += (ground - transform.Translation.z) * t;
				if (std::abs(ground - transform.Translation.z) < 0.005f)
				{
					transform.Translation.z = ground;
				}
			}
		}

	private:
		bool CanStand(Scene& scene, const glm::vec2& point, float currentHeight) const
		{
			const float ground = DioramaGround::HeightAt(scene, point);
			if (ground <= DioramaGround::None || std::abs(ground - currentHeight) > m_MaxStep)
			{
				return false;
			}

			// Solid box colliders of other entities, as boxes on the ground (their scale applied, rotation not).
			const entt::entity self = GetEntity();
			auto colliders = scene.m_Registry.view<TransformComponent, BoxCollider2DComponent>();
			for (auto entity : colliders)
			{
				if (entity == (entt::entity)self)
				{
					continue;
				}
				const auto [transform, collider] = colliders.get<TransformComponent, BoxCollider2DComponent>(entity);
				if (collider.IsSensor)
				{
					continue;
				}
				// Only things standing at about the player's height block it (a tree on the terrace above doesn't).
				if (std::abs(transform.Translation.z - currentHeight) > 1.0f)
				{
					continue;
				}
				const glm::vec2 scale = glm::abs(glm::vec2(transform.Scale));
				const glm::vec2 centre = glm::vec2(transform.Translation) + collider.Offset * scale;
				const glm::vec2 half = collider.Size * scale * 0.5f + glm::vec2(m_FootRadius);
				if (std::abs(point.x - centre.x) < half.x && std::abs(point.y - centre.y) < half.y)
				{
					return false;
				}
			}
			return true;
		}

		std::string m_Facing = "Up";
		float m_Speed = 3.5f;        // metres per second
		float m_MaxStep = 0.35f;     // the highest step up or down (stairs are 0.2)
		float m_FootRadius = 0.15f;
		float m_HeightEasing = 18.0f;
	};

	// On the Shrine Glade's director entity: the scene's framing and its light.
	//   - Starts in the tilted diorama view, much lower than the other field scenes (70 degrees from straight
	//     down, 20 below the horizon, with a wider 35 degree lens): the terrace walls, the steps and the
	//     shrine face the camera and the sunset shows over the valley's pines, as in the concept. The follow
	//     camera looks 3 m ahead up the stairs and 2.2 m above the player's ground, and its focus stays on the
	//     dressed stage (SceneBounds). All of it is reset when the scene ends. Keep in step with
	//     scripts/shrine_glade/preview.py (TILT_DEGREES, FOV_DEGREES, FOCUS_LEAD, FOCUS_RISE, BOUNDS).
	//   - Owns the light (the scene is forward lit, M6): the scene's Environment (sky and ground ambient, fog),
	//     the main light (the sun's DirectionalLightComponent) and every point light whose tag starts
	//     "Light Lantern" (flickering amber pools). Keys 1 / 2 / 3: day, golden dusk (the default, the
	//     concept's), night; L lanterns on/off. The scene file's Environment and sun are the dusk preset.
	class ShrineGladeScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			DioramaView::Enabled = true;
			DioramaView::Blend = 1.0f;
			DioramaView::SceneTiltDegrees = 70.0f;
			DioramaView::SceneFieldOfView = 35.0f;
			DioramaView::SceneFocusLead = 3.0f;
			DioramaView::SceneFocusRise = 2.2f;
			DioramaView::HasSceneBounds = true;
			DioramaView::SceneBoundsMin = { -2.0f, -8.0f };
			DioramaView::SceneBoundsMax = { 2.0f, 6.0f };

			Scene& scene = *GetEntity().GetScene();
			auto lights = scene.m_Registry.view<TagComponent, Light2DComponent>();
			for (auto entity : lights)
			{
				const std::string& tag = lights.get<TagComponent>(entity).Tag;
				const Light2DComponent& light = lights.get<Light2DComponent>(entity);
				if (light.Type == Light2DType::Point && tag.rfind("Light Lantern", 0) == 0)
				{
					m_Lanterns.push_back({ entity, light.Intensity, light.Radius, (float)m_Lanterns.size() * 1.9f });
				}
			}
			ApplyPreset();
			HY_INFO("Shrine Glade: WASD walk up the stairs to the shrine, E interact, 1/2/3 day-dusk-night, L lanterns, V top-down / tilted, O perspective / orthographic");
		}

		void OnDestroy() override
		{
			DioramaView::ResetSceneFraming();
		}

		void OnUpdate(float deltaTime) override
		{
			m_Time += deltaTime;
			const KeyCode keys[3] = { KeyCode::D1, KeyCode::D2, KeyCode::D3 };
			for (int i = 0; i < 3; i++)
			{
				if (KeyPressed(keys[i], m_Previous[i]))
				{
					m_Preset = i;
					ApplyPreset();
				}
			}
			if (KeyPressed(KeyCode::L, m_Previous[3]))
			{
				m_LanternsOn = m_LanternsOn == false;
			}

			// Lanterns: a slow breathing flicker from a few sines, different per lantern (no random jumps).
			Scene& scene = *GetEntity().GetScene();
			const float factor = m_LanternsOn ? GetPreset(m_Preset).LanternFactor : 0.0f;
			for (const Lantern& lantern : m_Lanterns)
			{
				if (scene.m_Registry.valid(lantern.Entity) == false)
				{
					continue;
				}
				const float t = m_Time + lantern.Phase;
				const float flicker = 0.5f * std::sin(t * 6.3f) + 0.3f * std::sin(t * 11.9f + 1.3f) + 0.2f * std::sin(t * 2.7f + 0.4f);
				Light2DComponent& light = scene.m_Registry.get<Light2DComponent>(lantern.Entity);
				light.Intensity = lantern.BaseIntensity * factor * (1.0f + 0.05f * flicker);
				light.Radius = lantern.BaseRadius * (1.0f + 0.02f * flicker);
			}
		}

	private:
		struct Lantern
		{
			entt::entity Entity = entt::null;
			float BaseIntensity = 1.0f;
			float BaseRadius = 1.0f;
			float Phase = 0.0f;
		};

		// One time of day: the sun's colour and strength, the hemisphere ambient, the fog, and how much of their
		// strength the lanterns keep. Dusk is what build.py writes into the scene; keep them in step (and
		// scripts/shrine_glade/preview.py's PRESETS, which approximates them).
		struct Preset
		{
			const char* Name;
			glm::vec3 SunColor;
			float SunIntensity;
			glm::vec3 SkyAmbient;
			glm::vec3 GroundAmbient;
			glm::vec3 FogColor;
			float LanternFactor;
		};
		static const Preset& GetPreset(int index)
		{
			static const Preset presets[3] = {
				{ "Day", { 1.00f, 0.95f, 0.85f }, 1.10f, { 0.55f, 0.6f, 0.68f }, { 0.2f, 0.19f, 0.16f }, { 0.7f, 0.74f, 0.78f }, 0.0f },
				{ "Dusk", { 1.00f, 0.72f, 0.46f }, 0.75f, { 0.36f, 0.28f, 0.32f }, { 0.12f, 0.09f, 0.08f }, { 0.66f, 0.44f, 0.36f }, 1.0f },
				{ "Night", { 0.55f, 0.65f, 1.00f }, 0.18f, { 0.12f, 0.15f, 0.26f }, { 0.04f, 0.04f, 0.06f }, { 0.08f, 0.1f, 0.18f }, 1.15f }
			};
			return presets[index];
		}

		void ApplyPreset()
		{
			Scene& scene = *GetEntity().GetScene();
			const Preset& preset = GetPreset(m_Preset);
			// Starts from what is drawn (the scene's own environment or its render profile's) and makes it the
			// scene's own for this run, so the preset shows whichever the scene uses. Plan P5 turns the presets
			// into Day, Dusk and Night render profiles switched with Scene::SetRenderProfile.
			SceneEnvironment environment = scene.GetActiveEnvironment();
			environment.Override = true;
			environment.SkyAmbient = preset.SkyAmbient;
			environment.GroundAmbient = preset.GroundAmbient;
			environment.FogColor = preset.FogColor;
			scene.SetEnvironment(environment);
			SceneLook look = scene.GetLook();
			look.Environment = RenderLookSource::Scene;
			scene.SetLook(look);

			Entity sun = scene.GetMainLight();
			if ((bool)sun)
			{
				DirectionalLightComponent& light = sun.GetComponent<DirectionalLightComponent>();
				light.Color = preset.SunColor;
				light.Intensity = preset.SunIntensity;
			}
			HY_INFO("Shrine Glade: {}", preset.Name);
		}

		static bool KeyPressed(KeyCode key, bool& previous)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && previous == false;
			previous = down;
			return pressed;
		}

		std::vector<Lantern> m_Lanterns;
		int m_Preset = 1;
		bool m_LanternsOn = true;
		bool m_Previous[4] = { false, false, false, false };
		float m_Time = 0.0f;
	};

	inline void RegisterShrineScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<DioramaWalkScript>("DioramaWalk");
		ScriptRegistry::Register<ShrineGladeScript>("ShrineGlade");
	}
}
