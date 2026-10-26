#pragma once
#include "Hydro/Renderer/RenderFeatures.h"

#include <yaml-cpp/yaml.h>

namespace Hydro
{
	// Writes and reads RenderFeatureSettings as a YAML map (Depth, SpriteSorting, Lighting,
	// DefaultSpriteAlpha, Meshes; enums by name). Used by the project file (RendererSettings::Features)
	// and scene files (SceneRenderSettings).
	class RenderFeatureSerializer
	{
	public:
		// Writes the map as the value of the current key.
		static void Serialize(YAML::Emitter& out, const RenderFeatureSettings& settings);
		// Missing keys and unknown names keep `settings`' current values.
		static void Deserialize(const YAML::Node& node, RenderFeatureSettings& settings);
	};
}
