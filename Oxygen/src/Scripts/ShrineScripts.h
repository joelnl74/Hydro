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
// characters), and the Farm Village below it (Assets/Scenes/FarmVillage.hyscene), built the same way from the
// medieval village kit. See docs/TestBed.md, "Shrine Glade" and "Farm Village".
namespace Hydro
{
	// Walks the player like VillagerWalk (WASD / arrows, Shift to hurry, Walk<Facing> / Idle<Facing> clips) over
	// the diorama's 3D collision: the entity's CharacterBodyComponent carries it up the terraces' stairs, over the
	// bridge and down slopes, and is blocked by walls, trees, posts and people (their 3D colliders). The script
	// only says where it wants to go (CharacterBody::Move); the physics stage moves it and writes the transform.
	// The scene's build script gives the player its Character Body (scripts/shrine_glade/collision.py).
	HY_SCRIPT()
	class DioramaWalkScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			if (GetEntity().HasComponent<CharacterBodyComponent>() == false)
			{
				HY_WARN("DioramaWalk: '{}' has no Character Body, so it can't walk; add one or rebuild the scene with its build script",
					GetComponent<TagComponent>().Tag);
			}
		}

		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			glm::vec2 move{ 0.0f };
			// A runtime UI canvas that captures input (a menu, a dialogue) owns the keys: she stands still.
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
			SpriteAnimator::Play(GetEntity(), (moving ? "Walk" + m_Facing : "Idle"));

			glm::vec3 velocity{ 0.0f };
			if (moving)
			{
				const float speed = Input::IsKeyPressed(KeyCode::LeftShift) ? m_Speed * RunMultiplier : m_Speed;
				velocity = glm::vec3(glm::normalize(move) * speed, 0.0f);
			}
			CharacterBody::Move(GetEntity(), velocity);
		}

	public:
		std::string m_Facing = "Up";
		HY_PROPERTY(Range(0, 20), Tooltip("Walking speed in metres per second"))
		float m_Speed = 3.5f; // the saved key stays m_Speed (renaming it needs #143's FormerNames)
		HY_PROPERTY(Range(1, 5), Tooltip("Shift multiplies the speed by this"))
		float RunMultiplier = 1.7f;
	};

	// One time of day for a diorama scene: the sun's colour and strength, how much of their strength the
	// lanterns keep, and its render profile (post-processing plan P5: the ambient, fog, exposure and bloom).
	// The ambient and fog colours are the fallback when the profile file is missing; keep them in step with the
	// profiles (written by the scene's build script).
	struct DioramaScenePreset
	{
		const char* Name;
		const char* Profile; // relative to the asset folder
		glm::vec3 SunColor;
		float SunIntensity;
		glm::vec3 SkyAmbient;
		glm::vec3 GroundAmbient;
		glm::vec3 FogColor;
		float LanternFactor;
	};

	// A scene's camera framing in the tilted view (DioramaView's Scene* settings).
	struct DioramaSceneFraming
	{
		float TiltDegrees = 0.0f;
		float FieldOfView = 0.0f;
		float FocusLead = 0.0f;
		float FocusRise = 0.0f;
		glm::vec2 BoundsMin{ 0.0f };
		glm::vec2 BoundsMax{ 0.0f };
	};

	// On a diorama scene's director entity: the scene's framing and its light. A scene's director derives from
	// it and gives its framing and three presets (ShrineGladeScript, FarmVillageScript).
	//   - Starts in the tilted diorama view with the scene's framing; reset when the scene ends.
	//   - Owns the light (forward lit, M6): the render profile (environment and post-processing), the main
	//     light (the sun's DirectionalLightComponent) and every point light whose tag starts "Light Lantern"
	//     (flickering amber pools). Keys 1 / 2 / 3 switch the presets, L the lanterns on/off.
	class DioramaDirectorScript : public ScriptableEntity
	{
	protected:
		virtual const char* GetSceneName() const = 0;
		virtual const char* GetHelp() const = 0;
		virtual DioramaSceneFraming GetFraming() const = 0;
		// index 0..2: keys 1, 2 and 3
		virtual const DioramaScenePreset& GetPreset(int index) const = 0;
		virtual int GetStartPreset() const { return 1; }

		void OnCreate() override
		{
			DioramaView::Enabled = true;
			DioramaView::Blend = 1.0f;
			ApplyFraming();

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
			m_Preset = GetStartPreset();
			ApplyPreset();
			HY_INFO("{}: {}", GetSceneName(), GetHelp());
		}

		void OnDestroy() override
		{
			DioramaView::ResetSceneFraming();
		}

		void OnUpdate(float deltaTime) override
		{
			// Every frame, so the director's framing fields can be tuned in the inspector while playing.
			ApplyFraming();
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

		void ApplyFraming()
		{
			const DioramaSceneFraming framing = GetFraming();
			DioramaView::SceneTiltDegrees = framing.TiltDegrees;
			DioramaView::SceneFieldOfView = framing.FieldOfView;
			DioramaView::SceneFocusLead = framing.FocusLead;
			DioramaView::SceneFocusRise = framing.FocusRise;
			DioramaView::HasSceneBounds = true;
			DioramaView::SceneBoundsMin = framing.BoundsMin;
			DioramaView::SceneBoundsMax = framing.BoundsMax;
		}

		void ApplyPreset()
		{
			Scene& scene = *GetEntity().GetScene();
			const DioramaScenePreset& preset = GetPreset(m_Preset);

			// The look: the preset's render profile for the environment and post-processing. A script loads a
			// handle before giving it to the scene (the scene never loads).
			const AssetHandle profile = AssetManager::IsProjectOpen()
				? AssetManager::FindHandle(AssetManager::GetAssetDirectory() / preset.Profile) : AssetHandle(0);
			if ((uint64_t)profile != 0 && AssetManager::GetAsset<RenderProfile>(profile) != nullptr)
			{
				scene.SetRenderProfile(profile);
				SceneLook look = scene.GetLook();
				look.Environment = RenderLookSource::Profile;
				look.PostProcess = RenderLookSource::Profile;
				scene.SetLook(look);
			}
			else
			{
				// No profile file: the preset's colours become the scene's own environment for this run.
				HY_WARN("{}: render profile '{}' not found; setting the environment directly", GetSceneName(), preset.Profile);
				SceneEnvironment environment = scene.GetActiveEnvironment();
				environment.Override = true;
				environment.SkyAmbient = preset.SkyAmbient;
				environment.GroundAmbient = preset.GroundAmbient;
				environment.FogColor = preset.FogColor;
				scene.SetEnvironment(environment);
				SceneLook look = scene.GetLook();
				look.Environment = RenderLookSource::Scene;
				scene.SetLook(look);
			}

			Entity sun = scene.GetMainLight();
			if ((bool)sun)
			{
				DirectionalLightComponent& light = sun.GetComponent<DirectionalLightComponent>();
				light.Color = preset.SunColor;
				light.Intensity = preset.SunIntensity;
			}
			HY_INFO("{}: {}", GetSceneName(), preset.Name);
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

	// On the Shrine Glade's director entity. Starts much lower than the other field scenes (70 degrees from
	// straight down, 20 below the horizon, with a wider 35 degree lens): the terrace walls, the steps and the
	// shrine face the camera and the sunset shows over the valley's pines, as in the concept. The follow camera
	// looks 3 m ahead up the stairs and 2.2 m above the player's ground. Keep in step with
	// scripts/shrine_glade/preview.py (TILT_DEGREES, FOV_DEGREES, FOCUS_LEAD, FOCUS_RISE, BOUNDS). Presets: day,
	// golden dusk (the default, the concept's), night.
	HY_SCRIPT()
	class ShrineGladeScript : public DioramaDirectorScript
	{
	public:
		// The camera framing (DioramaView): applied every frame, so it can be tuned while playing.
		HY_PROPERTY(Range(0, 89), Tooltip("Camera tilt in degrees from straight down"))
		float TiltDegrees = 70.0f;
		HY_PROPERTY(Range(5, 120), Tooltip("Vertical field of view in degrees (perspective)"))
		float FieldOfView = 35.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres the camera looks ahead of the player (up the screen)"))
		float FocusLead = 3.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres above the player's ground the camera aims at"))
		float FocusRise = 2.2f;
		HY_PROPERTY(Tooltip("Lowest x, y the camera's focus may reach"))
		glm::vec2 BoundsMin{ -2.0f, -8.0f };
		HY_PROPERTY(Tooltip("Highest x, y the camera's focus may reach"))
		glm::vec2 BoundsMax{ 2.0f, 6.0f };
		HY_PROPERTY(Range(0, 2), Tooltip("The light at the start: 0 day, 1 dusk, 2 night (keys 1 / 2 / 3)"))
		int StartPreset = 1;

	protected:
		const char* GetSceneName() const override { return "Shrine Glade"; }
		const char* GetHelp() const override
		{
			return "WASD walk up the stairs to the shrine, E interact, 1/2/3 day-dusk-night, L lanterns, V top-down / tilted, O perspective / orthographic";
		}
		DioramaSceneFraming GetFraming() const override
		{
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax };
		}
		int GetStartPreset() const override { return StartPreset; }
		const DioramaScenePreset& GetPreset(int index) const override
		{
			static const DioramaScenePreset presets[3] = {
				{ "Day", "ShrineGlade/RenderProfiles/Day.hyrender", { 1.00f, 0.95f, 0.85f }, 1.10f, { 0.55f, 0.6f, 0.68f }, { 0.2f, 0.19f, 0.16f }, { 0.7f, 0.74f, 0.78f }, 0.0f },
				{ "Dusk", "ShrineGlade/RenderProfiles/Dusk.hyrender", { 1.00f, 0.72f, 0.46f }, 0.75f, { 0.36f, 0.28f, 0.32f }, { 0.12f, 0.09f, 0.08f }, { 0.66f, 0.44f, 0.36f }, 1.0f },
				{ "Night", "ShrineGlade/RenderProfiles/Night.hyrender", { 0.55f, 0.65f, 1.00f }, 0.18f, { 0.12f, 0.15f, 0.26f }, { 0.04f, 0.04f, 0.06f }, { 0.08f, 0.1f, 0.18f }, 1.15f }
			};
			return presets[std::clamp(index, 0, 2)];
		}
	};

	// On the Farm Village's director entity (Assets/Scenes/FarmVillage.hyscene, scripts/farm_village/build.py).
	// A higher camera than the shrine's (55 degrees from straight down, a 30 degree lens), so the fields, the
	// square and the roofs read like the concept art; the focus stays on the village (the lane from the bridge to
	// the forest path). Keep in step with scripts/farm_village/build.py (BOUNDS_MIN, BOUNDS_MAX, CAMERA_ORTHO).
	// Presets: day, golden hour (the default), night.
	HY_SCRIPT()
	class FarmVillageScript : public DioramaDirectorScript
	{
	public:
		// The camera framing (DioramaView): applied every frame, so it can be tuned while playing.
		HY_PROPERTY(Range(0, 89), Tooltip("Camera tilt in degrees from straight down"))
		float TiltDegrees = 55.0f;
		HY_PROPERTY(Range(5, 120), Tooltip("Vertical field of view in degrees (perspective)"))
		float FieldOfView = 30.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres the camera looks ahead of the player (up the screen)"))
		float FocusLead = 2.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres above the player's ground the camera aims at"))
		float FocusRise = 1.2f;
		HY_PROPERTY(Tooltip("Lowest x, y the camera's focus may reach"))
		glm::vec2 BoundsMin{ -6.5f, -34.0f };
		HY_PROPERTY(Tooltip("Highest x, y the camera's focus may reach"))
		glm::vec2 BoundsMax{ 6.5f, 16.0f };
		HY_PROPERTY(Range(0, 2), Tooltip("The light at the start: 0 day, 1 golden hour, 2 night (keys 1 / 2 / 3)"))
		int StartPreset = 1;

	protected:
		const char* GetSceneName() const override { return "Farm Village"; }
		const char* GetHelp() const override
		{
			return "WASD walk, E talk / read / rest at the inn, the forest path north leads back to the shrine, 1/2/3 day-golden-night, L lanterns, V top-down / tilted";
		}
		DioramaSceneFraming GetFraming() const override
		{
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax };
		}
		int GetStartPreset() const override { return StartPreset; }
		const DioramaScenePreset& GetPreset(int index) const override
		{
			static const DioramaScenePreset presets[3] = {
				{ "Day", "FarmVillage/RenderProfiles/Day.hyrender", { 1.00f, 0.95f, 0.86f }, 1.15f, { 0.55f, 0.6f, 0.66f }, { 0.22f, 0.2f, 0.15f }, { 0.72f, 0.76f, 0.74f }, 0.0f },
				{ "Golden", "FarmVillage/RenderProfiles/Golden.hyrender", { 1.00f, 0.80f, 0.56f }, 1.0f, { 0.42f, 0.36f, 0.3f }, { 0.16f, 0.12f, 0.08f }, { 0.78f, 0.6f, 0.38f }, 0.8f },
				{ "Night", "FarmVillage/RenderProfiles/Night.hyrender", { 0.55f, 0.65f, 1.00f }, 0.18f, { 0.12f, 0.15f, 0.26f }, { 0.04f, 0.04f, 0.06f }, { 0.08f, 0.1f, 0.18f }, 1.15f }
			};
			return presets[std::clamp(index, 0, 2)];
		}
	};
}
