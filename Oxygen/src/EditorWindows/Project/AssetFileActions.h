#pragma once
#include "Hydro/Data/DataType.h"

#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// The Project window's changes on disk: new folders and assets, rename and delete with their popups. The window
	// follows them through the callbacks, which run while a popup draws.
	class AssetFileActions
	{
	public:
		// A folder was created.
		std::function<void(const std::filesystem::path&)> OnCreated;
		// A folder or asset was moved (from, to); assets keep their handle, so references follow.
		std::function<void(const std::filesystem::path&, const std::filesystem::path&)> OnRenamed;
		std::function<void(const std::filesystem::path&)> OnDeleted;

		// Opens the name popup to create a folder in `folder` or to rename `path`, or the delete popup for `path`.
		void BeginCreateFolder(const std::filesystem::path& folder);
		void BeginRename(const std::filesystem::path& path);
		void BeginDelete(const std::filesystem::path& path);
		void DrawPopups();

		// Folders and every registered asset (a rename keeps the asset handle that references use).
		static bool CanRename(const std::filesystem::path& path);

		// "<image name>.hyanim" next to a sprite sheet, one clip per sprite group. Like the other Create functions it
		// returns the new file, or an empty path when it couldn't be written.
		static std::filesystem::path CreateSpriteAnimation(const std::filesystem::path& imagePath);
		// "New Material.hymat" in `folder`.
		static std::filesystem::path CreateMaterial(const std::filesystem::path& folder);
		// "New Render Profile.hyrender" in `folder`.
		static std::filesystem::path CreateRenderProfile(const std::filesystem::path& folder);
		// "New <Type>.hydef" or "New <Type> Table.hytable" of the HY_DATA type `type`, registered with its handle.
		static std::filesystem::path CreateDataAsset(const std::filesystem::path& folder, const DataTypeInfo& type, bool table);

	private:
		// `base` + `extension` in `folder`, or "<base> 2", "<base> 3"... when that is taken.
		static std::filesystem::path GetFreePath(const std::filesystem::path& folder, const std::string& base,
			const std::string& extension);
		// Empty when `name` + `extension` is a valid new name in `parent`, else why it isn't. `ignore` is the item
		// being renamed.
		static std::string ValidateName(const std::string& name, const std::string& extension, const std::filesystem::path& parent,
			const std::filesystem::path& ignore);
		void DrawNamePopup();
		void DrawDeletePopup();

		// Name popup: create a folder in m_CreateFolder (m_RenamePath empty) or rename m_RenamePath; only the part
		// before the extension is edited.
		std::filesystem::path m_CreateFolder;
		std::filesystem::path m_RenamePath;
		std::string m_RenameExtension;
		char m_NameBuffer[256] = {};
		bool m_RequestNamePopup = false;

		std::filesystem::path m_PendingDeletePath;
		bool m_RequestDeletePopup = false;
	};
}
