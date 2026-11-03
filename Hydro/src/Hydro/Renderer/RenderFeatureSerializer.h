#pragma once
#include "Hydro/Renderer/RenderFeatures.h"

#include <yaml-cpp/yaml.h>

namespace Hydro
{
	// Writes and reads RenderFeatureSettings as a YAML map (enums by name), for project and scene files.
	class RenderFeatureSerializer
	{
	public:
		// Writes the map as the value of the current key.
		static void Serialize(YAML::Emitter& out, const RenderFeatureSettings& settings);
		// Missing keys and unknown names keep `settings`' current values.
		static void Deserialize(const YAML::Node& node, RenderFeatureSettings& settings);
	};
}
