#pragma once
#include "Hydro/Renderer/RenderProfile.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes .hyrender files (YAML): a Handle, then a RenderProfile map with the same Environment and
	// PostProcess maps a scene saves (SceneSerializer).
	class RenderProfileSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hyrender";

		// A new file gets a handle; an existing one keeps its own. False (logged) when it can't be written.
		static bool Serialize(const std::filesystem::path& filepath, const RenderProfileData& data);
		// Reads a .hyrender file. Returns false (logged) when it can't be read or isn't a render profile.
		static bool Deserialize(const std::filesystem::path& filepath, RenderProfileData& data);

		// One section as a YAML map, and back. Missing fields keep their defaults; a read environment map has Override on.
		static void SerializeEnvironment(YAML::Emitter& out, const SceneEnvironment& environment);
		static SceneEnvironment DeserializeEnvironment(const YAML::Node& node);
		static void SerializePostProcess(YAML::Emitter& out, const ScenePostProcess& postProcess);
		static ScenePostProcess DeserializePostProcess(const YAML::Node& node);

		// "Scene" / "Profile"; an unknown name reads as Scene.
		static const char* LookSourceToString(RenderLookSource source);
		static RenderLookSource LookSourceFromString(const std::string& name);
	};
}
