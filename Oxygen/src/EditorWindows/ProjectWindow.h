#pragma once
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Texture2D.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	// Content browser for <project>/Assets: folders, scenes and prefabs.
	// Prefabs get a live thumbnail drawn from their saved components. .hymeta sidecars (asset handles,
	// see AssetMetaSerializer) are hidden; deleting an image deletes its sidecar too. When the folder's
	// contents change (here or in Explorer) it asks the AssetManager to rescan, so new files get handles.
	//
	// Double-click: folder -> open, prefab -> instantiate into the scene, scene -> open.
	// Drag a prefab or scene onto the viewport or the hierarchy to do the same.
	class ProjectWindow
	{
	public:
		// Set by the editor layer. Called after the window has finished drawing.
		std::function<void(const std::filesystem::path&)> OnOpenScene;
		std::function<void(Entity)> OnEntityCreated;
		// A folder or scene was renamed on disk: from -> to. The editor layer moves the paths it
		// holds (open scene, start scene, last session) that point at or into it.
		std::function<void(const std::filesystem::path&, const std::filesystem::path&)> OnPathRenamed;
		// A file was clicked (the inspector shows it), or double-clicked / opened from its menu
		// (images open in the Sprite Editor, .hyanim files in the Animation window).
		std::function<void(const std::filesystem::path&)> OnAssetSelected;
		std::function<void(const std::filesystem::path&)> OnOpenAsset;

		void OnImguiRender(const Ref<Scene>& scene);

		// Rescan the folder on the next frame (e.g. after saving a prefab from elsewhere).
		void Refresh() { m_NeedsRescan = true; }

	private:
		// What the window shows a file as (not Hydro::AssetType: folders are items, images are "Other").
		enum class ItemType { Folder, Scene, Prefab, Other };

		struct AssetItem
		{
			std::filesystem::path Path;
			std::string DisplayName;
			ItemType Type = ItemType::Other;
		};

		// What a prefab looks like, read from its file. Rebuilt when the file changes.
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
		};

		void EnsureRoot();
		void Rescan();
		void DrawToolbar();
		void DrawGrid(const Ref<Scene>& scene);
		void DrawItem(const AssetItem& item, const Ref<Scene>& scene, float thumbnailSize);
		void DrawThumbnail(const AssetItem& item, const glm::vec2& min, float size, bool hovered, bool selected);
		void DrawItemTooltip(const AssetItem& item);
		void DrawDeletePopup();
		// "New Folder" and "Rename" both ask for a name in this popup.
		void DrawNamePopup();

		void ActivateItem(const AssetItem& item, const Ref<Scene>& scene);
		void InstantiatePrefab(const std::filesystem::path& path, const Ref<Scene>& scene);
		// Opens the name popup to create a folder in the current folder, or to rename `path`.
		void BeginCreateFolder();
		void BeginRename(const std::filesystem::path& path);
		// Folders and scenes. TODO(assets): prefabs, textures and other files once references go
		// through asset IDs; today a texture rename would break every sprite that uses it by path.
		static bool CanRename(ItemType type) { return type == ItemType::Folder || type == ItemType::Scene; }
		// Empty when `name` + `extension` is a valid new file / folder name in `parent`, otherwise why
		// it isn't. `ignore` is the item being renamed (keeping its own name is allowed).
		static std::string ValidateName(const std::string& name, const std::string& extension, const std::filesystem::path& parent, const std::filesystem::path& ignore);

		const PrefabPreview& GetPrefabPreview(const std::filesystem::path& path);
		// The image shown on an image's tile, loaded the first time it is drawn.
		// TODO(assets): small cached thumbnails instead of the whole texture.
		const Ref<Texture2D>& GetTextureThumbnail(const std::filesystem::path& path);
		// Writes "<image name>.hyanim" next to a sprite sheet, one clip per sprite group, and opens it.
		void CreateSpriteAnimation(const std::filesystem::path& imagePath);
		static ItemType GetItemType(const std::filesystem::path& path);

	private:
		std::filesystem::path m_RootDirectory;
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedPath;
		std::filesystem::path m_PendingDeletePath;

		std::vector<AssetItem> m_Items;
		// What the last scan of m_ListedDirectory found, to notice files added, removed or renamed.
		std::filesystem::path m_ListedDirectory;
		std::vector<std::filesystem::path> m_ListedPaths;
		std::unordered_map<std::string, PrefabPreview> m_PrefabPreviews;
		struct TextureThumbnail
		{
			Ref<Texture2D> Texture;                       // null when the file can't be loaded
			bool Loaded = false;
			std::filesystem::file_time_type ImportTime{}; // of its .hymeta: loaded again when it changes
			double LastCheckTime = 0.0;                   // ImGui time of the last .hymeta check
		};
		std::unordered_map<std::string, TextureThumbnail> m_TextureThumbnails;

		bool m_RequestDeletePopup = false;

		// Name popup: create a folder (m_RenamePath empty) or rename m_RenamePath. Files keep their
		// extension (m_RenameExtension); only the name in front of it is edited.
		std::filesystem::path m_RenamePath;
		std::string m_RenameExtension;
		char m_NameBuffer[256] = {};
		bool m_RequestNamePopup = false;
		bool m_NeedsRescan = true;
		double m_LastScanTime = 0.0;
		float m_ThumbnailSize = 88.0f;
		char m_SearchBuffer[128] = {};

		// Actions that change the scene or the folder run after drawing, never mid-iteration.
		std::vector<std::function<void()>> m_DeferredActions;
	};
}
