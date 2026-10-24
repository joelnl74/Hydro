#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Renderer/TextureImport.h>

#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// Slices a sprite sheet (a texture in sprite mode Multiple) into named sprites: grid settings,
	// Detect Grid (SpriteSlicer::DetectGrid), Slice, and per-sprite name, rectangle and pivot, with
	// the sprites drawn over the image. Apply writes the image's .hymeta and asks the editor to
	// re-import it. Opened from the texture's inspector or by double-clicking an image.
	class SpriteEditorWindow
	{
	public:
		// Set by the editor layer: the .hymeta of this image was saved.
		std::function<void(const std::filesystem::path&)> OnReimported;

		// Shows `imagePath` (asks first when the open one has unapplied changes).
		void Open(const std::filesystem::path& imagePath);
		void OnImguiRender();
		bool& IsOpen() { return m_Open; }

		// The Project window renamed or moved a file or folder: the image shown follows it.
		void OnAssetMoved(const std::filesystem::path& from, const std::filesystem::path& to);
		// The Project window deleted a file: the window closes if it was showing it (it would write it back).
		void OnAssetDeleted(const std::filesystem::path& path);

	private:
		void Load(const std::filesystem::path& imagePath);
		void Apply();
		// Rebuilds the sprite list from the grid; sprites at the same rectangle keep their pivot.
		void Slice();
		void SyncRowNamesFromConfig();
		void SyncRowNamesToConfig();

		void DrawToolbar();
		void DrawSlicingSettings();
		void DrawSpriteList();
		void DrawSelectedSprite();
		void DrawCanvas();
		void DrawDiscardPopup();

	private:
		bool m_Open = false;
		std::filesystem::path m_Path;
		Ref<Texture2D> m_Texture;
		TextureImportConfig m_Config; // being edited
		bool m_Dirty = false;
		int32_t m_Selected = -1;
		float m_Zoom = 0.0f; // 0 = fit the image to the window
		char m_RowNames[8192] = {}; // RowNames, one per line, as typed

		std::filesystem::path m_PendingOpen; // waiting for "discard changes?"
		bool m_RequestDiscardPopup = false;
	};
}
