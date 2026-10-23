#pragma once
#include "Hydro/Scene/SpriteAnimation.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes .hyanim files: a sprite animation set (YAML).
	//
	//   SpriteAnimationSet:
	//     Texture: Villagers.png          # relative to the .hyanim's folder
	//     DefaultClip: IdleDown
	//     Clips:
	//       - Name: IdleDown              # short form: every frame lasts 1 / FrameRate seconds
	//         FrameRate: 6
	//         Loop: Loop                  # Once | Loop | PingPong
	//         Frames: [Farmer_Down_0, Farmer_Down_1]
	//       - Name: WalkLeft              # long form: a duration (seconds) per frame
	//         FlipX: true
	//         Frames:
	//           - { Sprite: Farmer_Right_0, Duration: 0.1 }
	//         Events:
	//           - { Frame: 0, Name: Footstep }
	//
	// Frames name the texture's sprites (TextureImportConfig), so slicing the sheet again doesn't break
	// clips; a plain number is a sprite index. Unknown names are logged and the frame left out.
	class SpriteAnimationSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hyanim";

		static bool Serialize(const std::filesystem::path& filepath, const SpriteAnimationSet& set);
		// Loads the texture too (Texture2D::LoadFromFile). Returns false (logged) when the file or its
		// texture can't be read.
		static bool Deserialize(const std::filesystem::path& filepath, SpriteAnimationSetSpecification& specification);
	};
}
