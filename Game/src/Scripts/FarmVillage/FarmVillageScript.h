// Game/src/Scripts/FarmVillage/FarmVillageScript.h
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
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax, 0.0f, 0.0f };
		}
		int GetStartPreset() const override { return StartPreset; }
		const LightingPreset& GetPreset(int index) const override
		{
			return GetLightingPreset(Lighting, index);
		}
	};
}
