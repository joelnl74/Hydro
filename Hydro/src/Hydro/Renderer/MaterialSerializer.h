#pragma once
#include "Hydro/Renderer/Material.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes .hymat files (YAML): a Handle, then a Material map. A mesh slot saves the same fields inline
	// (without Type).
	class MaterialSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymat";

		// A new file gets a handle; an existing one keeps its own. False (logged) when it can't be written.
		static bool Serialize(const std::filesystem::path& filepath, const MaterialData& data);
		// Reads a .hymat and loads its textures. False (logged) when it can't be read or isn't a material.
		static bool Deserialize(const std::filesystem::path& filepath, MaterialData& data);

		// One material as a YAML map, and back (loading its textures). Missing fields keep their defaults.
		static void SerializeData(YAML::Emitter& out, const MaterialData& data);
		static MaterialData DeserializeData(const YAML::Node& node);
	};
}
