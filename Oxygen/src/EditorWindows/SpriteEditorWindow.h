#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Renderer/TextureImport.h>

#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// Slices a sprite sheet into named sprites (grid, Detect Grid, per-sprite name, rectangle and pivot).
	// Apply writes the image's .hymeta and asks the editor to re-import it.
	class SpriteEditorWindow
	{
	public:
		// Called after the image's .hymeta was saved.
		std::function<void(const std::filesystem::path&)> OnReimported;

		// Shows `imagePath`, asking first when the open one has unapplied changes.
		void Open(const std::filesystem::path& imagePath);
		void OnImguiRender();
		bool& IsOpen() { return m_Open; }

		// The image shown follows a rename or move in the Project window.
		void OnAssetMoved(const std::filesystem::path& from, const std::filesystem::path& to);
		// Closes the window when its file was deleted, so it isn't written back.
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
