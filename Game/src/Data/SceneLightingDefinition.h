// Game/src/Data/SceneLightingDefinition.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

// A diorama scene's times of day (keys 1 / 2 / 3: day, dusk, night), one file per scene in Assets/Data/Lighting/,
// used by its director (DioramaDirectorScript, Scripts/Common/DioramaDirectorScript.h). The ambient and fog colours are the fallback when
// the render profile is missing; keep them in step with the profiles.
namespace Hydro
{
	HY_DATA()
	struct LightingPreset
	{
		HY_PROPERTY(Tooltip("Its name in the log"))
		std::string Name;
		HY_PROPERTY(Asset(RenderProfile), Tooltip("The ambient, fog, exposure and bloom"))
		AssetHandle Profile = 0;
		HY_PROPERTY(Color, Tooltip("The sun's (or moon's) colour; in an interior, the light through the windows"))
		glm::vec3 SunColor{ 1.0f };
		HY_PROPERTY(Range(0, 4))
		float SunIntensity = 1.0f;
		HY_PROPERTY(Color, Tooltip("Used only when the profile is missing"))
		glm::vec3 SkyAmbient{ 0.5f };
		HY_PROPERTY(Color, Tooltip("Used only when the profile is missing"))
		glm::vec3 GroundAmbient{ 0.2f };
		HY_PROPERTY(Color, Tooltip("Used only when the profile is missing"))
		glm::vec3 FogColor{ 0.7f };
		HY_PROPERTY(Range(0, 3), Tooltip("Lantern and fire lights' strength (0 = off, 1 = as placed)"))
		float LanternFactor = 1.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("Lit windows' strength (0 = off, 1 = as placed)"))
		float WindowFactor = 1.0f;
	};

	HY_DATA(Definition)
	struct SceneLightingDefinition
	{
		HY_PROPERTY(Tooltip("Day, dusk (golden hour, evening), night: keys 1 / 2 / 3"))
		std::vector<LightingPreset> Presets;
	};
}
