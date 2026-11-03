#pragma once
#include "Hydro/Terrain/Terrain.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// Reads and writes .hyterrain files: the handle first, settings under Terrain:, packed blocks under Data:
	// (run-length + base64, the weight map as a PNG). Deterministic output, so untouched blocks never show in a diff.
	class TerrainSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hyterrain";
		static constexpr uint32_t CurrentVersion = 1;

		// A new file gets a handle; an existing one keeps its own. False (logged) when it can't be written.
		static bool Serialize(const std::filesystem::path& filepath, const Terrain& terrain);
		// nullptr (logged) when it can't be read or its data doesn't fit its size.
		static Ref<Terrain> Deserialize(const std::filesystem::path& filepath);

		// The packed blocks, exposed for tests and tools.
		static std::string EncodeBase64(const std::vector<uint8_t>& bytes, size_t lineLength = 76);
		static bool DecodeBase64(const std::string& text, std::vector<uint8_t>& bytes);
		static std::vector<uint8_t> EncodeRunLength(const std::vector<uint8_t>& values);
		static bool DecodeRunLength(const std::vector<uint8_t>& runs, size_t count, std::vector<uint8_t>& values);
	};
}
