#pragma once
#include "Hydro/Scene/SpriteAnimation.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes .hyanim files (YAML sprite animation sets). Frames name the texture's sprites, so re-slicing
	// keeps clips; unknown names are logged and left out. Format: docs/SceneAndECS.md.
	class SpriteAnimationSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hyanim";

		static bool Serialize(const std::filesystem::path& filepath, const SpriteAnimationSet& set);
		// Loads the texture too. False (logged) when the file or its texture can't be read.
		static bool Deserialize(const std::filesystem::path& filepath, SpriteAnimationSetSpecification& specification);
	};
}
