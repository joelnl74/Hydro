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
	// The Shrine Glade's director: a much lower camera than the other field scenes, so the terrace walls, steps and shrine
	// face it and the sunset shows over the valley. Keep in step with scripts/shrine_glade/preview.py.
	HY_SCRIPT()
	class ShrineGladeScript : public DioramaDirectorScript
	{
	public:
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
			return { TiltDegrees, FieldOfView, FocusLead, FocusRise, BoundsMin, BoundsMax, 0.0f, 0.0f };
		}
		int GetStartPreset() const override { return StartPreset; }
		const LightingPreset& GetPreset(int index) const override
		{
			return GetLightingPreset(Lighting, index);
		}
	};
}
