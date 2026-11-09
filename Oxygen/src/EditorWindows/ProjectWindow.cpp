#include "ProjectWindow.h"
#include "Utilities/AssetDragDrop.h"
#include "Utilities/ThumbnailManager.h"
#include "../EditorPaths.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Asset/AssetMetaSerializer.h>
#include <Hydro/Data/DataTypeRegistry.h>
#include <Hydro/Scene/PrefabSerializer.h>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace Hydro
{
	constexpr double RescanInterval = 1.0; // seconds; picks up files saved from other windows / Explorer


	static std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return text;
	}

	ProjectWindow::ProjectWindow()
	{
		// The file actions change the disk; the window follows: it selects, rescans and tells the editor after drawing.
		m_FileActions.OnCreated = [this](const std::filesystem::path& path)
		{
			m_SelectedPath = path;
			m_NeedsRescan = true;
		};
		m_FileActions.OnRenamed = [this](const std::filesystem::path& from, const std::filesystem::path& to)
		{
			// Keep the browser where it was: follow the folder if we were inside it.
			const std::filesystem::path relative = m_CurrentDirectory.lexically_relative(from);
			if (relative.empty() == false && *relative.begin() != "..")
			{
				m_CurrentDirectory = (to / relative).lexically_normal();
			}
			m_SelectedPath = to;
			m_NeedsRescan = true;
			if (OnPathRenamed)
			{
				m_DeferredActions.push_back([this, from, to]() { OnPathRenamed(from, to); });
			}
		};
		m_FileActions.OnDeleted = [this](const std::filesystem::path& path)
		{
			m_Preview.Forget(path);
			if (m_SelectedPath == path)
			{
				m_SelectedPath.clear();
			}
			m_NeedsRescan = true;
			if (OnPathDeleted)
			{
				m_DeferredActions.push_back([this, path]() { OnPathDeleted(path); });
			}
		};
	}

	void ProjectWindow::OnImguiRender(const Ref<Scene>& scene)
	{
		EnsureRoot();
		m_Preview.BeginFrame();

		if (m_NeedsRescan || ImGui::GetTime() - m_LastScanTime > RescanInterval)
		{
			Rescan();
		}

		// Collapsed or a hidden tab: no toolbar or grid. Its open popups (rename, delete) still draw.
		if (ImGui::Begin("Project"))
		{
			DrawToolbar();
			ImGui::Separator();
			DrawGrid(scene);
		}
		m_FileActions.DrawPopups();

		ImGui::End();

		// Run scene / file changes now that nothing is iterating m_Items or the registry.
		std::vector<std::function<void()>> actions;
		actions.swap(m_DeferredActions);
		for (auto& action : actions)
		{
			action();
		}
	}

	void ProjectWindow::EnsureRoot()
	{
		const std::filesystem::path root = EditorPaths::GetAssetsDirectory();

		if (root != m_RootDirectory)
		{
			// New project (or first frame): start at Assets and make sure the standard folders exist.
			m_RootDirectory = root;
			m_CurrentDirectory = root;
			m_SelectedPath.clear();
			// Thumbnails and previews belong to the project that was open.
			ThumbnailManager::Clear();
			m_Preview.Clear();

			std::error_code ec;
			std::filesystem::create_directories(EditorPaths::GetScenesDirectory(), ec);
			std::filesystem::create_directories(EditorPaths::GetPrefabsDirectory(), ec);
			m_NeedsRescan = true;
		}

		std::error_code ec;
		if (std::filesystem::is_directory(m_CurrentDirectory, ec) == false)
		{
			m_CurrentDirectory = m_RootDirectory;
			m_NeedsRescan = true;
		}
	}

	void ProjectWindow::Rescan()
	{
		m_NeedsRescan = false;
		m_LastScanTime = ImGui::GetTime();
		m_Items.clear();

		// The folder on disk: its sub-folders are items (folders aren't assets), and its asset files tell whether
		// anything was added, removed or renamed since the last look.
		std::vector<std::filesystem::path> folders;
		std::vector<std::filesystem::path> listed;
		std::error_code ec;
		for (std::filesystem::directory_iterator it(m_CurrentDirectory, ec), end; ec.value() == 0 && it != end; it.increment(ec))
		{
			const std::filesystem::path& path = it->path();
			const std::string fileName = path.filename().u8string();
			if (fileName.empty() || fileName[0] == '.' || AssetMetaSerializer::IsSidecar(path))
			{
				continue;
			}

			std::error_code typeError;
			if (std::filesystem::is_directory(path, typeError))
			{
				folders.push_back(path);
			}
			else if (GetAssetTypeFromExtension(path) != AssetType::None)
			{
				listed.push_back(path);
			}
		}
		std::sort(listed.begin(), listed.end());

		// Files added, removed or renamed in this folder: rescan the assets so they keep or get their handles. Not
		// when only the folder changed.
		const bool sameFolder = m_ListedDirectory == m_CurrentDirectory;
		if (sameFolder && listed != m_ListedPaths)
		{
			AssetManager::Rescan();
		}
		m_ListedDirectory = m_CurrentDirectory;
		m_ListedPaths = std::move(listed);

		for (const std::filesystem::path& folder : folders)
		{
			AssetItem item;
			item.Path = folder;
			item.Type = ItemType::Folder;
			item.DisplayName = folder.filename().u8string();
			m_Items.push_back(std::move(item));
		}

		// The files are the registry's assets in this folder (paths relative to the asset folder, forward slashes).
		// Files that are gone but still registered are left out.
		std::string folderKey = m_CurrentDirectory.lexically_relative(m_RootDirectory).generic_u8string();
		if (folderKey == ".")
		{
			folderKey.clear();
		}

		for (const auto& entry : AssetManager::GetRegistry().GetAll())
		{
			const AssetMetadata& metadata = entry.second;
			ItemType type = ItemType::Folder;
			if (metadata.IsMissing || ToItemType(metadata.Type, type) == false
				|| metadata.Path.parent_path().generic_u8string() != folderKey)
			{
				continue;
			}

			AssetItem item;
			item.Path = m_CurrentDirectory / metadata.Path.filename();
			item.Type = type;
			item.Handle = metadata.Handle;
			item.DisplayName = (type == ItemType::Scene || type == ItemType::Prefab) ? item.Path.stem().u8string()
				: item.Path.filename().u8string();
			m_Items.push_back(std::move(item));
		}

		// Folders first, then by name.
		std::sort(m_Items.begin(), m_Items.end(), [](const AssetItem& a, const AssetItem& b)
		{
			const bool aFolder = a.Type == ItemType::Folder;
			const bool bFolder = b.Type == ItemType::Folder;
			if (aFolder != bFolder)
			{
				return aFolder;
			}

			return ToLower(a.DisplayName) < ToLower(b.DisplayName);
		});
	}

	void ProjectWindow::DrawToolbar()
	{
		// Back
		const bool atRoot = m_CurrentDirectory == m_RootDirectory;
		ImGui::BeginDisabled(atRoot);
		if (ImGui::Button("<"))
		{
			m_CurrentDirectory = m_CurrentDirectory.parent_path();
			m_SelectedPath.clear();
			m_NeedsRescan = true;
		}
		ImGui::EndDisabled();

		// Breadcrumbs: Assets / Prefabs / ...
		ImGui::SameLine();
		std::filesystem::path crumb = m_RootDirectory;
		if (ImGui::SmallButton("Assets"))
		{
			m_CurrentDirectory = m_RootDirectory;
			m_NeedsRescan = true;
		}

		std::error_code ec;
		const std::filesystem::path relative = std::filesystem::relative(m_CurrentDirectory, m_RootDirectory, ec);
		if (ec.value() == 0 && relative != ".")
		{
			int index = 0;
			for (const auto& part : relative)
			{
				crumb /= part;
				ImGui::SameLine(0.0f, 2.0f);
				ImGui::TextDisabled("/");
				ImGui::SameLine(0.0f, 2.0f);
				ImGui::PushID(index++);
				if (ImGui::SmallButton(part.u8string().c_str()))
				{
					m_CurrentDirectory = crumb;
					m_NeedsRescan = true;
				}
				ImGui::PopID();
			}
		}

		// Search + thumbnail size, right aligned.
		const float searchWidth = 160.0f;
		const float sliderWidth = 90.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float rightEdge = ImGui::GetWindowContentRegionMax().x;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX(), rightEdge - searchWidth - sliderWidth - spacing));

		ImGui::SetNextItemWidth(searchWidth);
		ImGui::InputTextWithHint("##Search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));

		ImGui::SameLine();
		ImGui::SetNextItemWidth(sliderWidth);
		ImGui::SliderFloat("##Size", &m_ThumbnailSize, 48.0f, 160.0f, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
	}

	void ProjectWindow::DrawGrid(const Ref<Scene>& scene)
	{
		ImGui::BeginChild("##ProjectGrid");

		const std::string search = ToLower(m_SearchBuffer);
		const float padding = 12.0f;
		const float cellSize = m_ThumbnailSize + padding;
		const int columns = std::max(1, std::min(64, (int)(ImGui::GetContentRegionAvail().x / cellSize)));

		int visibleCount = 0;
		if (ImGui::BeginTable("##Assets", columns, ImGuiTableFlags_SizingFixedSame))
		{
			for (const AssetItem& item : m_Items)
			{
				if (search.empty() == false && ToLower(item.DisplayName).find(search) == std::string::npos)
				{
					continue;
				}

				ImGui::TableNextColumn();
				DrawItem(item, scene, m_ThumbnailSize);
				visibleCount++;
			}

			ImGui::EndTable();
		}

		if (visibleCount == 0)
		{
			ImGui::TextDisabled("%s", search.empty()
				? (AssetManager::IsProjectOpen() ? "This folder is empty. Save a prefab from the hierarchy or inspector." : "No project is open.")
				: "No assets match the search.");
		}

		// Click on empty space clears the selection; right-click opens the folder menu.
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsAnyItemHovered() == false)
		{
			m_SelectedPath.clear();
		}

		// F2 renames the selected folder or asset.
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false) &&
			m_SelectedPath.empty() == false && AssetFileActions::CanRename(m_SelectedPath))
		{
			m_FileActions.BeginRename(m_SelectedPath);
		}

		if (ImGui::BeginPopupContextWindow("##ProjectContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("New Folder..."))
			{
				m_FileActions.BeginCreateFolder(m_CurrentDirectory);
			}
			if (ImGui::MenuItem("New Material"))
			{
				m_DeferredActions.push_back([this]() { ShowCreated(AssetFileActions::CreateMaterial(GetNewAssetFolder()), false); });
			}
			if (ImGui::MenuItem("New Render Profile"))
			{
				m_DeferredActions.push_back([this]() { ShowCreated(AssetFileActions::CreateRenderProfile(GetNewAssetFolder()), false); });
			}
			// Game data: one entry per HY_DATA(Definition) / HY_DATA(Table) type, by display name.
			for (int shape = 0; shape < 2; shape++)
			{
				const bool table = shape == 1;
				if (ImGui::BeginMenu(table ? "New Data Table" : "New Definition"))
				{
					bool any = false;
					for (const DataTypeInfo* type : DataTypeRegistry::GetTypes())
					{
						if ((table ? type->CanBeTable : type->CanBeDefinition) == false)
						{
							continue;
						}
						any = true;
						if (ImGui::MenuItem(type->DisplayName.c_str()))
						{
							m_DeferredActions.push_back([this, type, table]() { CreateDataAsset(*type, table); });
						}
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", type->Name.c_str());
						}
					}
					if (any == false)
					{
						ImGui::TextDisabled(table ? "No HY_DATA(Table) types in the game's code"
							: "No HY_DATA(Definition) types in the game's code");
					}
					ImGui::EndMenu();
				}
			}
			if (ImGui::MenuItem("Refresh"))
			{
				m_NeedsRescan = true;
			}

			ImGui::EndPopup();
		}

		ImGui::EndChild();
	}

	void ProjectWindow::DrawItem(const AssetItem& item, const Ref<Scene>& scene, float thumbnailSize)
	{
		ImGui::PushID(item.Path.u8string().c_str());

		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##Tile", { thumbnailSize, thumbnailSize },
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

		const bool hovered = ImGui::IsItemHovered();
		const bool selected = m_SelectedPath == item.Path;

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_SelectedPath = item.Path;
			if ((item.Type == ItemType::Texture || item.Type == ItemType::Animation || item.Type == ItemType::Material
				|| item.Type == ItemType::Model || item.Type == ItemType::RenderProfile || item.Type == ItemType::Definition
				|| item.Type == ItemType::DataTable) && OnAssetSelected)
			{
				OnAssetSelected(item.Path);
			}
		}

		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			const AssetItem copy = item;
			Ref<Scene> sceneCopy = scene;
			m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
		}

		// Assets are dragged by handle: prefabs and scenes onto the viewport or hierarchy, images, animations and the
		// rest onto matching inspector fields.
		const bool isTexture = item.Type == ItemType::Texture;
		const bool isAnimationSet = item.Type == ItemType::Animation;
		const bool isMaterial = item.Type == ItemType::Material;
		const bool isModel = item.Type == ItemType::Model;
		const bool isRenderProfile = item.Type == ItemType::RenderProfile;
		const bool isTerrain = item.Type == ItemType::Terrain;
		const bool isData = item.Type == ItemType::Definition || item.Type == ItemType::DataTable;
		if ((item.Type == ItemType::Prefab || item.Type == ItemType::Scene || isTexture || isAnimationSet || isMaterial || isModel
			|| isRenderProfile || isTerrain || isData) && ImGui::BeginDragDropSource())
		{
			AssetDragDrop::SetPayload(item.Path);
			ImGui::TextUnformatted(item.DisplayName.c_str());
			if (isTexture)
			{
				ImGui::TextDisabled("Drop on a sprite's Texture");
			}
			else if (isAnimationSet)
			{
				ImGui::TextDisabled("Drop on a Sprite Animator's Animations");
			}
			else if (isMaterial)
			{
				ImGui::TextDisabled("Drop on a mesh's material Asset field");
			}
			else if (isRenderProfile)
			{
				ImGui::TextDisabled("Drop on Render Settings > Look (scene) > Render profile");
			}
			else if (isTerrain)
			{
				ImGui::TextDisabled("Drop on a Terrain component's Terrain field");
			}
			else if (isModel)
			{
				ImGui::TextDisabled("Drop into the viewport to place it, or on a mesh's Model field");
			}
			else
			{
				ImGui::TextDisabled("%s", item.Type == ItemType::Prefab ? "Drop to instantiate" : "Drop to open");
			}
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginPopupContextItem("##ItemContext"))
		{
			const AssetItem copy = item;
			Ref<Scene> sceneCopy = scene;

			if (item.Type == ItemType::Prefab && ImGui::MenuItem("Instantiate"))
			{
				m_DeferredActions.push_back([this, copy, sceneCopy]() { InstantiatePrefab(copy.Path, sceneCopy); });
			}
			if (item.Type == ItemType::Scene && ImGui::MenuItem("Open Scene"))
			{
				m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
			}
			if (item.Type == ItemType::Folder && ImGui::MenuItem("Open Folder"))
			{
				m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
			}
			if (isTexture)
			{
				if (ImGui::MenuItem("Import Settings") && OnAssetSelected)
				{
					OnAssetSelected(item.Path);
				}
				if (ImGui::MenuItem("Sprite Editor") && OnOpenAsset)
				{
					OnOpenAsset(item.Path);
				}
				const bool isSheet = ThumbnailManager::Get(item.Handle).IsSheet;
				if (ImGui::MenuItem("Create Sprite Animation", nullptr, false, isSheet))
				{
					m_DeferredActions.push_back([this, copy]() { ShowCreated(AssetFileActions::CreateSpriteAnimation(copy.Path), true); });
				}
				if (isSheet == false && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("Set the image's Sprite Mode to Multiple and slice it first.");
				}
			}
			if (isAnimationSet && ImGui::MenuItem("Open in Animation Window") && OnOpenAsset)
			{
				OnOpenAsset(item.Path);
			}
			if (AssetFileActions::CanRename(item.Path) && ImGui::MenuItem("Rename...", "F2"))
			{
				m_FileActions.BeginRename(item.Path);
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Delete..."))
			{
				m_FileActions.BeginDelete(item.Path);
			}

			ImGui::EndPopup();
		}
		else if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left) == false)
		{
			m_Preview.DrawTooltip(item);
		}

		m_Preview.DrawThumbnail(item, { cursor.x, cursor.y }, thumbnailSize, hovered, selected);

		// Name under the tile, wrapped to the tile width.
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);
		ImGui::TextWrapped("%s", item.DisplayName.c_str());
		ImGui::PopTextWrapPos();

		ImGui::PopID();
	}

	void ProjectWindow::ActivateItem(const AssetItem& item, const Ref<Scene>& scene)
	{
		switch (item.Type)
		{
			case ItemType::Folder:
				m_CurrentDirectory = item.Path;
				m_SelectedPath.clear();
				m_NeedsRescan = true;
				break;
			case ItemType::Prefab:
				InstantiatePrefab(item.Path, scene);
				break;
			case ItemType::Scene:
				if (OnOpenScene)
				{
					OnOpenScene(item.Path);
				}
				break;
			case ItemType::Texture:
			case ItemType::Animation:
				if (OnOpenAsset)
				{
					OnOpenAsset(item.Path);
				}
				break;
			case ItemType::Font:
			case ItemType::Terrain:
			case ItemType::Shader:
				break;
			case ItemType::Material:
			case ItemType::Model:
			case ItemType::RenderProfile:
			case ItemType::Definition:
			case ItemType::DataTable:
				// Edited in the inspector.
				if (OnAssetSelected)
				{
					OnAssetSelected(item.Path);
				}
				break;
		}
	}

	void ProjectWindow::InstantiatePrefab(const std::filesystem::path& path, const Ref<Scene>& scene)
	{
		if (scene != nullptr && OnInstantiatePrefab)
		{
			OnInstantiatePrefab(path);
		}
	}

	bool ProjectWindow::ToItemType(AssetType type, ItemType& itemType)
	{
		switch (type)
		{
			case AssetType::Texture2D:
				itemType = ItemType::Texture;
				return true;
			case AssetType::Scene:
				itemType = ItemType::Scene;
				return true;
			case AssetType::Prefab:
				itemType = ItemType::Prefab;
				return true;
			case AssetType::SpriteAnimation:
				itemType = ItemType::Animation;
				return true;
			case AssetType::Material:
				itemType = ItemType::Material;
				return true;
			case AssetType::Mesh:
				itemType = ItemType::Model;
				return true;
			case AssetType::RenderProfile:
				itemType = ItemType::RenderProfile;
				return true;
			case AssetType::Font:
				itemType = ItemType::Font;
				return true;
			case AssetType::Terrain:
				itemType = ItemType::Terrain;
				return true;
			case AssetType::Definition:
				itemType = ItemType::Definition;
				return true;
			case AssetType::DataTable:
				itemType = ItemType::DataTable;
				return true;
			case AssetType::Shader:
				itemType = ItemType::Shader;
				return true;
			default:
				return false;
		}
	}

	std::filesystem::path ProjectWindow::GetNewAssetFolder() const
	{
		return m_CurrentDirectory.empty() ? m_RootDirectory : m_CurrentDirectory;
	}

	void ProjectWindow::ShowCreated(const std::filesystem::path& path, bool open)
	{
		if (path.empty())
		{
			return;
		}

		m_SelectedPath = path;
		m_NeedsRescan = true;
		const std::function<void(const std::filesystem::path&)>& callback = open ? OnOpenAsset : OnAssetSelected;
		if (callback)
		{
			callback(path);
		}
	}

	void ProjectWindow::CreateDataAsset(const DataTypeInfo& type, bool table)
	{
		// Selected and named right away: a new data asset has no useful default name.
		const std::filesystem::path created = AssetFileActions::CreateDataAsset(GetNewAssetFolder(), type, table);
		ShowCreated(created, false);
		if (created.empty() == false)
		{
			m_FileActions.BeginRename(created);
		}
	}
}
