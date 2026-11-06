#pragma once
#include "Project/AssetFileActions.h"
#include "Project/AssetItem.h"
#include "Project/AssetPreview.h"

#include "Hydro/Asset/Asset.h"
#include "Hydro/Data/DataType.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Hydro
{
	// Content browser for <project>/Assets: the registered assets plus the folders on disk, with thumbnails.
	// Double-click opens a folder or scene and instantiates a prefab; dragging onto the viewport does the same.
	class ProjectWindow
	{
	public:
		ProjectWindow();

		// Called after the window has finished drawing.
		std::function<void(const std::filesystem::path&)> OnOpenScene;
		// A prefab to instantiate in the scene (double-click, Instantiate): the editor layer does it as an undo step.
		std::function<void(const std::filesystem::path&)> OnInstantiatePrefab;
		// A folder or asset was renamed (from, to); the editor layer moves the paths it holds that point into it.
		std::function<void(const std::filesystem::path&, const std::filesystem::path&)> OnPathRenamed;
		// A file was deleted; windows editing it let go of it.
		std::function<void(const std::filesystem::path&)> OnPathDeleted;
		// A file was clicked (the inspector shows it), or opened (images in the Sprite Editor, .hyanim files in the
		// Animation window).
		std::function<void(const std::filesystem::path&)> OnAssetSelected;
		std::function<void(const std::filesystem::path&)> OnOpenAsset;

		void OnImguiRender(const Ref<Scene>& scene);

		// Rescans the folder on the next frame.
		void Refresh() { m_NeedsRescan = true; }

	private:
		using ItemType = AssetItemType;

		void EnsureRoot();
		void Rescan();
		void DrawToolbar();
		void DrawGrid(const Ref<Scene>& scene);
		void DrawItem(const AssetItem& item, const Ref<Scene>& scene, float thumbnailSize);

		void ActivateItem(const AssetItem& item, const Ref<Scene>& scene);
		void InstantiatePrefab(const std::filesystem::path& path, const Ref<Scene>& scene);
		// Where New Material and the other new assets go: the current folder.
		std::filesystem::path GetNewAssetFolder() const;
		// Selects a file just created and shows it in the inspector, or opens it (`open`). Nothing when `path` is empty.
		void ShowCreated(const std::filesystem::path& path, bool open);
		void CreateDataAsset(const DataTypeInfo& type, bool table);
		// False for asset types the window doesn't show.
		static bool ToItemType(AssetType type, ItemType& itemType);

	private:
		std::filesystem::path m_RootDirectory;
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedPath;

		std::vector<AssetItem> m_Items;
		// What the last scan of m_ListedDirectory found, to notice files added, removed or renamed.
		std::filesystem::path m_ListedDirectory;
		std::vector<std::filesystem::path> m_ListedPaths;

		AssetPreview m_Preview;
		AssetFileActions m_FileActions;

		bool m_NeedsRescan = true;
		double m_LastScanTime = 0.0;
		float m_ThumbnailSize = 88.0f;
		char m_SearchBuffer[128] = {};

		// Actions that change the scene or the folder run after drawing.
		std::vector<std::function<void()>> m_DeferredActions;
	};
}
