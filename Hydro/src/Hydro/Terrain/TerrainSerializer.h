#pragma once
#include "Hydro/Terrain/Terrain.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// Reads and writes .hyterrain files (Terrain plan, "Data"): readable settings under Terrain:, packed data under
	// Data:. The handle goes first, like in scenes and .hymat files (AssetMetaSerializer), so a rename keeps it.
	//
	//   Handle: 1234567890
	//   Terrain:
	//     Version: 1
	//     CellSize: 0.5
	//     StepHeight: 0.25
	//     Size: [65, 65]          # corners
	//     ...
	//     Layers:
	//       - Material: 987654321
	//         EdgeRoughness: 0.9
	//   Data:
	//     Levels: |-              # one signed byte per corner, run-length encoded, base64
	//       ...
	//     Ramps: |-               # one byte per cell (TerrainRamp), run-length encoded, base64
	//     WallMaterial: |-        # one byte per cell, run-length encoded, base64
	//     Weights: |-             # the RGBA8 weight map as a PNG (north row first, so it looks like a map), base64
	//
	// Writing is deterministic: the same terrain always gives the same bytes (fixed order, base64 wrapped at 76
	// characters, a PNG encoder with fixed settings), so an untouched block never shows in a diff.
	class TerrainSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hyterrain";
		static constexpr uint32_t CurrentVersion = 1;

		// Writes `terrain` to `filepath` (a new file gets a handle; an existing one keeps its own). Returns false
		// (logged) when the file can't be written.
		static bool Serialize(const std::filesystem::path& filepath, const Terrain& terrain);
		// Reads a .hyterrain file. nullptr (logged) when it can't be read or its data doesn't fit its size.
		static Ref<Terrain> Deserialize(const std::filesystem::path& filepath);

		// The packed blocks, exposed for tests and tools.
		static std::string EncodeBase64(const std::vector<uint8_t>& bytes, size_t lineLength = 76);
		static bool DecodeBase64(const std::string& text, std::vector<uint8_t>& bytes);
		static std::vector<uint8_t> EncodeRunLength(const std::vector<uint8_t>& values);
		static bool DecodeRunLength(const std::vector<uint8_t>& runs, size_t count, std::vector<uint8_t>& values);
	};
}
