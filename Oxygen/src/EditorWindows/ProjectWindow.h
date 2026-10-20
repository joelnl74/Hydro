#pragma once
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	// Content browser for <project>/Assets: folders, scenes and prefabs.
	// Prefabs get a live thumbnail drawn from their saved components.
	//
	// Double-click: folder -> open, prefab -> instantiate into the scene, scene -> open.
	// Drag a prefab or scene onto the viewport or the hierarchy to do the same.
	class ProjectWindow
	{
	public:
		// Set by the editor layer. Called after the window has finished drawing.
		std::function<void(const std::filesystem::path&)> OnOpenScene;
		std::function<void(Entity)> OnEntityCreated;

		void OnImguiRender(const Ref<Scene>& scene);

		// Rescan the folder on the next frame (e.g. after saving a prefab from elsewhere).
		void Refresh() { m_NeedsRescan = true; }

	private:
		enum class AssetType { Folder, Scene, Prefab, Other };

		struct AssetItem
		{
			std::filesystem::path Path;
			std::string DisplayName;
			AssetType Type = AssetType::Other;
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
		void DrawFolderNamePopup();

		void ActivateItem(const AssetItem& item, const Ref<Scene>& scene);
		void InstantiatePrefab(const std::filesystem::path& path, const Ref<Scene>& scene);
		// Opens the name popup to create a folder in the current folder, or to rename `folder`.
		void BeginCreateFolder();
		void BeginRenameFolder(const std::filesystem::path& folder);
		// Empty when `name` is a valid new folder name in `parent`, otherwise why it isn't.
		// `ignore` is the folder being renamed (keeping its own name is allowed).
		static std::string ValidateFolderName(const std::string& name, const std::filesystem::path& parent, const std::filesystem::path& ignore);

		const PrefabPreview& GetPrefabPreview(const std::filesystem::path& path);
		static AssetType GetAssetType(const std::filesystem::path& path);

	private:
		std::filesystem::path m_RootDirectory;
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedPath;
		std::filesystem::path m_PendingDeletePath;

		std::vector<AssetItem> m_Items;
		std::unordered_map<std::string, PrefabPreview> m_PrefabPreviews;

		bool m_RequestDeletePopup = false;

		// Folder name popup: create a folder (m_RenamePath empty) or rename m_RenamePath.
		std::filesystem::path m_RenamePath;
		char m_FolderNameBuffer[256] = {};
		bool m_RequestFolderNamePopup = false;
		bool m_NeedsRescan = true;
		double m_LastScanTime = 0.0;
		float m_ThumbnailSize = 88.0f;
		char m_SearchBuffer[128] = {};

		// Actions that change the scene or the folder run after drawing, never mid-iteration.
		std::vector<std::function<void()>> m_DeferredActions;
	};
}
