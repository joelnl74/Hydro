#pragma once
#include "Hydro/Renderer/RenderProfile.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes .hyrender files: one render profile (YAML, post-processing plan P4). The handle goes
	// first, like in scenes and .hymat files (AssetMetaSerializer), so a rename or move keeps it.
	//
	//   Handle: 1234567890
	//   RenderProfile:
	//     Environment:            # left out: no environment of its own (Global lights, no fog)
	//       SkyAmbient: [0.36, 0.28, 0.32]
	//       GroundAmbient: [0.12, 0.09, 0.08]
	//       Fog: true
	//       FogColor: [0.66, 0.44, 0.36]
	//       FogStart: 13
	//       FogEnd: 38
	//     PostProcess:
	//       Exposure: 0
	//       Bloom: true
	//       BloomThreshold: 1
	//       BloomKnee: 0.5
	//       BloomIntensity: 0.6
	//       BloomRadius: 1
	//
	// The Environment and PostProcess maps are the same ones a scene saves for its own values (SceneSerializer).
	class RenderProfileSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hyrender";

		// Writes `data` to `filepath` (a new file gets a handle; an existing one keeps its own). Returns false
		// (logged) when the file can't be written.
		static bool Serialize(const std::filesystem::path& filepath, const RenderProfileData& data);
		// Reads a .hyrender file. Returns false (logged) when it can't be read or isn't a render profile.
		static bool Deserialize(const std::filesystem::path& filepath, RenderProfileData& data);

		// One section as a YAML map, and back. Missing fields keep their defaults. Reading an environment map
		// gives Override = true (the map is only written for an environment of its own).
		static void SerializeEnvironment(YAML::Emitter& out, const SceneEnvironment& environment);
		static SceneEnvironment DeserializeEnvironment(const YAML::Node& node);
		static void SerializePostProcess(YAML::Emitter& out, const ScenePostProcess& postProcess);
		static ScenePostProcess DeserializePostProcess(const YAML::Node& node);

		// "Scene" / "Profile"; an unknown name reads as Scene.
		static const char* LookSourceToString(RenderLookSource source);
		static RenderLookSource LookSourceFromString(const std::string& name);
	};
}
