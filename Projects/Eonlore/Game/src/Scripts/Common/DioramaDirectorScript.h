#pragma once

#include "Hydro.h"
#include "Data/SceneLightingDefinition.h"
#include "Scripts/Common/DioramaCamera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// A scene's camera framing in the tilted view (DioramaView's Scene* settings).
	struct DioramaSceneFraming
	{
		float TiltDegrees = 0.0f;
		float FieldOfView = 0.0f;
		float FocusLead = 0.0f;
		float FocusRise = 0.0f;
		glm::vec2 BoundsMin{ 0.0f };
		glm::vec2 BoundsMax{ 0.0f };
		float YawDegrees = 0.0f; // counter-clockwise seen from above, 0 = looking north
		float ViewScale = 0.0f;  // times the camera's OrthographicSize, 0 = 1
	};

	// Base of a diorama scene's director: the tilted view with the scene's framing, and its light (three time-of-day presets
	// on keys 1 / 2 / 3, flickering lanterns, lit windows; L toggles the lanterns).
	class DioramaDirectorScript : public ScriptableEntity
	{
	protected:
		virtual const char* GetSceneName() const = 0;
		virtual const char* GetHelp() const = 0;
		virtual DioramaSceneFraming GetFraming() const = 0;
		// index 0..2: keys 1, 2 and 3
		virtual const LightingPreset& GetPreset(int index) const = 0;
		virtual int GetStartPreset() const { return 1; }
		// An interior starts in the time of day of the scene it was entered from.
		virtual bool FollowsOutsideLight() const { return false; }
		// "Light Window" lights take the sun's colour (an interior's daylight) instead of their own (lamps behind the glass).
		virtual bool WindowsTakeSunColor() const { return false; }
		virtual void OnPresetApplied([[maybe_unused]] const LightingPreset& preset) {}

		// Looked up again on every call, so an edit shows at the next preset switch. A missing file or preset: white light, warned.
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
			const int lastPreset = LastPreset();
			m_Preset = FollowsOutsideLight() && lastPreset >= 0 ? lastPreset : GetStartPreset();
			ApplyPreset();
			HY_INFO("{}: {}", GetSceneName(), GetHelp());
		}

		void OnDestroy() override
		{
			DioramaView::ResetSceneFraming();
		}

		void OnUpdate(float deltaTime) override
		{
			// Every frame, so the framing can be tuned in the inspector while playing.
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
			DioramaView::SceneYawDegrees = framing.YawDegrees;
			DioramaView::SceneViewScale = framing.ViewScale;
		}

		void ApplyPreset()
		{
			Scene& scene = *GetEntity().GetScene();
			const LightingPreset& preset = GetPreset(m_Preset);

			// A script loads a handle before giving it to the scene (the scene never loads).
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

			LastPreset() = m_Preset;
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

		// The preset of the director that applied one last (-1: none this run), on the game session.
		static int& LastPreset();
		// Keeps the lighting definition GetLightingPreset read from alive while its preset is used.
		mutable DataView<SceneLightingDefinition> m_Lighting;

		std::vector<Lantern> m_Lanterns;
		std::vector<WindowLight> m_Windows;
		int m_Preset = 1;
		bool m_LanternsOn = true;
		bool m_Previous[4] = { false, false, false, false };
		float m_Time = 0.0f;
	};

	// The director of a field scene made from the scene template (docs/TestBed.md, "Scene template"): the Shrine Glade's
	// framing and light as fields, so a new scene needs no script. The defaults are the Shrine Glade's values.
	HY_SCRIPT()
	class FieldSceneScript : public DioramaDirectorScript
	{
	public:
		HY_PROPERTY(Tooltip("The scene's name in the log"))
		std::string SceneName = "Field";
		// Applied every frame, so it can be tuned while playing.
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
		HY_PROPERTY(Range(-180, 180), Tooltip("Direction the camera looks, degrees counter-clockwise from north (+Y); camera zones may turn it"))
		float YawDegrees = 0.0f;
		HY_PROPERTY(Range(0, 4), Tooltip("How much the camera sees, times its Orthographic Size (below 1 = closer)"))
		float ViewScale = 1.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("The light at the start: 0 day, 1 dusk, 2 night (keys 1 / 2 / 3)"))
		int StartPreset = 1;

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
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax, YawDegrees, ViewScale };
		}
		int GetStartPreset() const override { return StartPreset; }
		// Built from the fields on every call, so inspector edits apply at the next preset switch. The ambient and fog colours
		// are the Shrine Glade's, used only when a profile is missing.
		const LightingPreset& GetPreset(int index) const override
		{
			const int i = std::clamp(index, 0, 2);
			const AssetHandle profiles[3] = { DayProfile, DuskProfile, NightProfile };
			const AssetHandle profile = profiles[i];
			if (i == 0)
			{
				m_Presets[i] = { "Day", profile, DaySunColor, DaySunIntensity,
					{ 0.55f, 0.6f, 0.68f }, { 0.2f, 0.19f, 0.16f }, { 0.7f, 0.74f, 0.78f }, DayLanterns, DayWindows };
			}
			else if (i == 1)
			{
				m_Presets[i] = { "Dusk", profile, DuskSunColor, DuskSunIntensity,
					{ 0.198f, 0.154f, 0.176f }, { 0.066f, 0.0495f, 0.044f }, { 0.66f, 0.44f, 0.36f }, DuskLanterns, DuskWindows };
			}
			else
			{
				m_Presets[i] = { "Night", profile, NightSunColor, NightSunIntensity,
					{ 0.036f, 0.045f, 0.078f }, { 0.012f, 0.012f, 0.018f }, { 0.08f, 0.1f, 0.18f }, NightLanterns, NightWindows };
			}
			return m_Presets[i];
		}

	private:
		mutable LightingPreset m_Presets[3] = {};
	};
}
