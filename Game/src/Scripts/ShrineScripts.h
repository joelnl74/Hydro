#pragma once
#include "Hydro.h"
#include "DioramaCamera.h"
#include "VillageScripts.h"
#include "Data/SceneLightingDefinition.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// The Shrine Glade (Assets/Scenes/ShrineGlade.hyscene): the first scene that mixes meshes (terraces,
// retaining walls, stairs, the imported shrine and stone lanterns) with upright sprites (trees, props,
// characters), the Farm Village below it (Assets/Scenes/FarmVillage.hyscene), built the same way from the
// medieval village kit, and the village's interiors (the inn, Assets/Scenes/FarmVillageInn.hyscene). See
// docs/TestBed.md, "Shrine Glade", "Farm Village" and "Interiors".
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
			// Standing still she keeps facing the way she walked (Idle<Facing>); a sheet with one "Idle" still works.
			if (moving)
			{
				SpriteAnimator::Play(GetEntity(), "Walk" + m_Facing);
			}
			else if (SpriteAnimator::Play(GetEntity(), "Idle" + m_Facing) == false)
			{
				SpriteAnimator::Play(GetEntity(), "Idle");
			}

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

	// One time of day for a diorama scene is a LightingPreset (Data/SceneLightingDefinition.h): the sun's colour and
	// strength, how much of their strength the lanterns and the lit windows keep, and its render profile
	// (post-processing plan P5: the ambient, fog, exposure and bloom). The scenes' presets are data
	// (Assets/Data/Lighting/<Scene>.hydef); the scene template's director builds its own from its fields.

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
	//     light (the sun's DirectionalLightComponent), every point light whose tag starts "Light Lantern" or
	//     "Light Fire" (flickering amber pools, scaled by the preset's LanternFactor) and every one whose tag starts
	//     "Light Window" (lit windows, scaled by its WindowFactor: dark by day, lit at dusk and night; an interior
	//     also tints them with the sun, WindowsTakeSunColor). Keys 1 / 2 / 3 switch the presets, L the lanterns.
	//   - The last preset applied is shared (s_LastPreset), so an interior can start in the light of the scene the
	//     player came in from (FollowsOutsideLight).
	class DioramaDirectorScript : public ScriptableEntity
	{
	protected:
		virtual const char* GetSceneName() const = 0;
		virtual const char* GetHelp() const = 0;
		virtual DioramaSceneFraming GetFraming() const = 0;
		// index 0..2: keys 1, 2 and 3
		virtual const LightingPreset& GetPreset(int index) const = 0;
		virtual int GetStartPreset() const { return 1; }
		// An interior starts in the time of day of the scene it was entered from (when there is one).
		virtual bool FollowsOutsideLight() const { return false; }
		// The "Light Window *" lights take the preset's sun colour (an interior: the daylight through its windows)
		// instead of keeping their own (a house seen from outside: its lamps behind the glass).
		virtual bool WindowsTakeSunColor() const { return false; }
		// After a preset is applied.
		virtual void OnPresetApplied([[maybe_unused]] const LightingPreset& preset) {}

		// Preset `index` of a scene's lighting definition (Assets/Data/Lighting/<Scene>.hydef), looked up again on
		// every call (an edit shows at the next preset switch). A missing file or preset: plain white light, warned.
		const LightingPreset& GetLightingPreset(AssetHandle lighting, int index) const
		{
			m_Lighting = Data::Get<SceneLightingDefinition>(lighting);
			if (m_Lighting.IsValid() == false || m_Lighting->Presets.empty())
			{
				static const LightingPreset missing{ "(no lighting data)" };
				return missing;
			}
			return m_Lighting->Presets[(size_t)std::clamp(index, 0, (int)m_Lighting->Presets.size() - 1)];
		}

		void OnCreate() override
		{
			DioramaView::Enabled = true;
			DioramaView::Blend = 1.0f;
			ApplyFraming();

			// The first scene of a run starts from its own preset; one entered from another may follow its light.
			if (SceneTransition::GetDepth() == 0)
			{
				s_LastPreset = -1;
			}

			Scene& scene = *GetEntity().GetScene();
			auto lights = scene.m_Registry.view<TagComponent, Light2DComponent>();
			for (auto entity : lights)
			{
				const std::string& tag = lights.get<TagComponent>(entity).Tag;
				const Light2DComponent& light = lights.get<Light2DComponent>(entity);
				const bool flickers = tag.rfind("Light Lantern", 0) == 0 || tag.rfind("Light Fire", 0) == 0;
				if (light.Type == Light2DType::Point && flickers)
				{
					m_Lanterns.push_back({ entity, light.Intensity, light.Radius, (float)m_Lanterns.size() * 1.9f });
				}
				if (light.Type == Light2DType::Point && tag.rfind("Light Window", 0) == 0)
				{
					m_Windows.push_back({ entity, light.Intensity, light.Color });
				}
			}
			m_Preset = FollowsOutsideLight() && s_LastPreset >= 0 ? s_LastPreset : GetStartPreset();
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

		struct WindowLight
		{
			entt::entity Entity = entt::null;
			float BaseIntensity = 1.0f;
			glm::vec3 BaseColor{ 1.0f };
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
			const LightingPreset& preset = GetPreset(m_Preset);

			// The look: the preset's render profile for the environment and post-processing. A script loads a
			// handle before giving it to the scene (the scene never loads).
			const AssetHandle profile = preset.Profile;
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
				HY_WARN("{}: {}'s render profile is missing; setting the environment directly", GetSceneName(), preset.Name);
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
			// The lit windows: their strength as placed times the preset's WindowFactor.
			const bool sunColored = WindowsTakeSunColor();
			for (const WindowLight& window : m_Windows)
			{
				if (scene.m_Registry.valid(window.Entity) == false)
				{
					continue;
				}
				Light2DComponent& light = scene.m_Registry.get<Light2DComponent>(window.Entity);
				light.Color = sunColored ? preset.SunColor : window.BaseColor;
				light.Intensity = window.BaseIntensity * preset.WindowFactor;
			}

			s_LastPreset = m_Preset;
			OnPresetApplied(preset);
			HY_INFO("{}: {}", GetSceneName(), preset.Name);
		}

		static bool KeyPressed(KeyCode key, bool& previous)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && previous == false;
			previous = down;
			return pressed;
		}

		static inline int s_LastPreset = -1; // the preset of the director that applied one last (-1: none this run)
		// Keeps the lighting definition GetLightingPreset read from alive while its preset is used.
		mutable DataView<SceneLightingDefinition> m_Lighting;

		std::vector<Lantern> m_Lanterns;
		std::vector<WindowLight> m_Windows;
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
		HY_PROPERTY(Asset(Definition, SceneLightingDefinition), Tooltip("The day, dusk and night presets: sun, lanterns, windows, render profile"))
		AssetHandle Lighting = 6651133902390268617ull; // Assets/Data/Lighting/ShrineGlade.hydef

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
		const LightingPreset& GetPreset(int index) const override
		{
			return GetLightingPreset(Lighting, index);
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
		HY_PROPERTY(Asset(Definition, SceneLightingDefinition), Tooltip("The day, golden hour and night presets: sun, lanterns, windows, render profile"))
		AssetHandle Lighting = 1569287181202592509ull; // Assets/Data/Lighting/FarmVillage.hydef

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
		const LightingPreset& GetPreset(int index) const override
		{
			return GetLightingPreset(Lighting, index);
		}
	};
	// On the director entity of a field scene made from the project's scene template
	// (Assets/Scenes/Templates/Field.hyscene, see docs/TestBed.md "Scene template"). The same director as the Shrine
	// Glade's, but with everything as fields instead of code, so a new scene gets the shrine's framing and its day /
	// dusk / night light without writing a script: the three render profiles (RenderProfiles/Field/*.hyrender, copies
	// of the shrine's; point a scene at its own copies to give it its own mood), the sun and how strongly the lanterns
	// and lit windows burn in each. The defaults are the Shrine Glade's values; tune the framing while playing.
	HY_SCRIPT()
	class FieldSceneScript : public DioramaDirectorScript
	{
	public:
		HY_PROPERTY(Tooltip("The scene's name in the log"))
		std::string SceneName = "Field";
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
		glm::vec2 BoundsMin{ -10.0f, -10.0f };
		HY_PROPERTY(Tooltip("Highest x, y the camera's focus may reach"))
		glm::vec2 BoundsMax{ 10.0f, 10.0f };
		HY_PROPERTY(Range(0, 2), Tooltip("The light at the start: 0 day, 1 dusk, 2 night (keys 1 / 2 / 3)"))
		int StartPreset = 1;

		// The three times of day.
		HY_PROPERTY(Asset(RenderProfile), Tooltip("Day: ambient, fog, bloom and depth of field"))
		AssetHandle DayProfile = 0;
		HY_PROPERTY(Color, Tooltip("Day: the sun's colour"))
		glm::vec3 DaySunColor{ 1.00f, 0.95f, 0.85f };
		HY_PROPERTY(Range(0, 4), Tooltip("Day: the sun's strength"))
		float DaySunIntensity = 1.10f;
		HY_PROPERTY(Range(0, 3), Tooltip("Day: lantern and fire lights' strength (0 = off, 1 = as placed)"))
		float DayLanterns = 0.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("Day: lit windows' strength (0 = off, 1 = as placed)"))
		float DayWindows = 0.0f;

		HY_PROPERTY(Asset(RenderProfile), Tooltip("Dusk: ambient, fog, bloom and depth of field"))
		AssetHandle DuskProfile = 0;
		HY_PROPERTY(Color, Tooltip("Dusk: the sun's colour"))
		glm::vec3 DuskSunColor{ 1.00f, 0.72f, 0.46f };
		HY_PROPERTY(Range(0, 4), Tooltip("Dusk: the sun's strength"))
		float DuskSunIntensity = 0.75f;
		HY_PROPERTY(Range(0, 3), Tooltip("Dusk: lantern and fire lights' strength (0 = off, 1 = as placed)"))
		float DuskLanterns = 1.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("Dusk: lit windows' strength (0 = off, 1 = as placed)"))
		float DuskWindows = 1.0f;

		HY_PROPERTY(Asset(RenderProfile), Tooltip("Night: ambient, fog, bloom and depth of field"))
		AssetHandle NightProfile = 0;
		HY_PROPERTY(Color, Tooltip("Night: the moonlight's colour"))
		glm::vec3 NightSunColor{ 0.55f, 0.65f, 1.00f };
		HY_PROPERTY(Range(0, 4), Tooltip("Night: the moonlight's strength"))
		float NightSunIntensity = 0.18f;
		HY_PROPERTY(Range(0, 3), Tooltip("Night: lantern and fire lights' strength (0 = off, 1 = as placed)"))
		float NightLanterns = 1.4f;
		HY_PROPERTY(Range(0, 3), Tooltip("Night: lit windows' strength (0 = off, 1 = as placed)"))
		float NightWindows = 1.15f;

	protected:
		const char* GetSceneName() const override { return SceneName.c_str(); }
		const char* GetHelp() const override
		{
			return "WASD walk, E interact, 1/2/3 day-dusk-night, L lanterns, V top-down / tilted";
		}
		DioramaSceneFraming GetFraming() const override
		{
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax };
		}
		int GetStartPreset() const override { return StartPreset; }
		// Built from the fields on every call, so edits in the inspector apply at the next preset switch. The
		// ambient and fog colours are the Shrine Glade's, used only when a profile is missing.
		const LightingPreset& GetPreset(int index) const override
		{
			const int i = std::clamp(index, 0, 2);
			const AssetHandle profiles[3] = { DayProfile, DuskProfile, NightProfile };
			const AssetHandle profile = profiles[i];
			if (i == 0)
			{
				m_Presets[i] = { "Day", profile, DaySunColor, DaySunIntensity, { 0.55f, 0.6f, 0.68f }, { 0.2f, 0.19f, 0.16f }, { 0.7f, 0.74f, 0.78f }, DayLanterns, DayWindows };
			}
			else if (i == 1)
			{
				m_Presets[i] = { "Dusk", profile, DuskSunColor, DuskSunIntensity, { 0.198f, 0.154f, 0.176f }, { 0.066f, 0.0495f, 0.044f }, { 0.66f, 0.44f, 0.36f }, DuskLanterns, DuskWindows };
			}
			else
			{
				m_Presets[i] = { "Night", profile, NightSunColor, NightSunIntensity, { 0.036f, 0.045f, 0.078f }, { 0.012f, 0.012f, 0.018f }, { 0.08f, 0.1f, 0.18f }, NightLanterns, NightWindows };
			}
			return m_Presets[i];
		}

	private:
		mutable LightingPreset m_Presets[3] = {};
	};

	// On an interior's director entity (the Farm Village's inn, Assets/Scenes/FarmVillageInn.hyscene, built by
	// scripts/interiors/build_inn.py). Interiors are scenes of their own, pushed by a door ("Door Inn" in the village
	// opens FarmVillageInn) and popped by "Door Out", so the village waits exactly as it was left. The room is built
	// at the characters' scale with its camera-facing wall left out and black around it; the camera comes in closer
	// than the field's (the Camera's OrthographicSize, and a narrower view here). The light: no sun; the preset
	// lights the "Light Window *" point lights (daylight, evening, moonlight through the windows) and sets how
	// strongly the "Light Fire *" lights (the hearth, candles) burn. It starts in the time of day of the scene the
	// player came in from.
	HY_SCRIPT()
	class InteriorScript : public DioramaDirectorScript
	{
	public:
		HY_PROPERTY(Tooltip("The interior's name in the log"))
		std::string RoomName = "Inn";
		// The camera framing (DioramaView): applied every frame, so it can be tuned while playing.
		HY_PROPERTY(Range(0, 89), Tooltip("Camera tilt in degrees from straight down"))
		float TiltDegrees = 52.0f;
		HY_PROPERTY(Range(5, 120), Tooltip("Vertical field of view in degrees (perspective)"))
		float FieldOfView = 30.0f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres the camera looks ahead of the player (up the screen)"))
		float FocusLead = 0.6f;
		HY_PROPERTY(Range(-20, 20), Tooltip("Metres above the player's ground the camera aims at"))
		float FocusRise = 0.8f;
		HY_PROPERTY(Tooltip("Lowest x, y the camera's focus may reach"))
		glm::vec2 BoundsMin{ -1.5f, -1.0f };
		HY_PROPERTY(Tooltip("Highest x, y the camera's focus may reach"))
		glm::vec2 BoundsMax{ 1.5f, 1.5f };
		HY_PROPERTY(Range(0, 2), Tooltip("The light when the room is played on its own: 0 day, 1 evening, 2 night (keys 1 / 2 / 3)"))
		int StartPreset = 1;
		HY_PROPERTY(Asset(Definition, SceneLightingDefinition), Tooltip("The day, evening and night presets: window light, fire, render profile"))
		AssetHandle Lighting = 4733468918101467972ull; // Assets/Data/Lighting/Interiors.hydef

	protected:
		const char* GetSceneName() const override { return RoomName.c_str(); }
		const char* GetHelp() const override
		{
			return "WASD walk, E talk / rest / leave by the door, 1/2/3 day-evening-night, L the fire, V top-down / tilted";
		}
		DioramaSceneFraming GetFraming() const override
		{
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax };
		}
		int GetStartPreset() const override { return StartPreset; }
		bool FollowsOutsideLight() const override { return true; }
		// Presets line up with the field scenes' (day, golden hour / dusk, night). The "sun" is the light through
		// the windows; the profiles keep the room dim so the fire carries it.
		const LightingPreset& GetPreset(int index) const override
		{
			return GetLightingPreset(Lighting, index);
		}

		// The windows (DioramaDirectorScript's "Light Window *" lights) let the daylight in: the sun's colour, at the
		// preset's WindowFactor (its sun's strength).
		bool WindowsTakeSunColor() const override { return true; }
	};
}
