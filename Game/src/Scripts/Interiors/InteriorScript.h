// Game/src/Scripts/Interiors/InteriorScript.h
#pragma once

#include "Hydro.h"
#include "Scripts/Common/DioramaDirectorScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
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
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax, 0.0f, 0.0f };
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
