#pragma once
#include "AssetItem.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Hydro
{
	// The Project window's tile pictures and hover tooltips. A prefab's preview is read from its file and kept until
	// the file changes; full images load a few per frame, so a folder of many prefabs fills in over a few frames.
	class AssetPreview
	{
	public:
		// Once per frame, before the tiles are drawn.
		void BeginFrame() { m_LoadsLeft = 2; }
		// Forgets every prefab preview (another project was opened), or the one of `path` (it was deleted).
		void Clear() { m_PrefabPreviews.clear(); }
		void Forget(const std::filesystem::path& path) { m_PrefabPreviews.erase(path.u8string()); }

		// The tile's background, selection frame and picture in the square at `min` (screen pixels).
		void DrawThumbnail(const AssetItem& item, const glm::vec2& min, float size, bool hovered, bool selected);
		void DrawTooltip(const AssetItem& item);

	private:
		// What a prefab looks like, read from its file and rebuilt when the file changes.
		struct PrefabPreview
		{
			std::filesystem::file_time_type WriteTime{};
			bool Valid = false;
			std::string Name;
			bool HasTransform = false;
			bool HasSprite = false;
			glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
			float RotationZ = 0.0f;
			glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
			// Its animation's first frame when it has one, else its sprite.
			AssetHandle TextureHandle = 0;
			uint32_t SpriteIndex = 0;
			bool FlipX = false;
			AssetHandle AnimationsHandle = 0;
			std::string StartClip;
		};

		const PrefabPreview& GetPrefabPreview(const std::filesystem::path& path);

		std::unordered_map<std::string, PrefabPreview> m_PrefabPreviews;
		// Full images the tiles may still load this frame.
		int m_LoadsLeft = 0;
	};
}
