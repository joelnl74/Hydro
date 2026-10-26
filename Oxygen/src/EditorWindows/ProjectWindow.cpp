#include "ProjectWindow.h"
#include "Utilities/AssetDragDrop.h"
#include "Utilities/ThumbnailManager.h"
#include "../EditorPaths.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Asset/AssetMetaSerializer.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/MaterialSerializer.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/SceneSerializer.h>
#include <Hydro/Scene/SpriteAnimation.h>
#include <Hydro/Scene/SpriteAnimationSerializer.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Renderer/TextureImportSerializer.h>
#include "SpriteAnimationWindow.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <system_error>

namespace Hydro
{
	constexpr double RescanInterval = 1.0; // seconds; picks up files saved from other windows / Explorer

	static ImU32 ToColor(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32({ r, g, b, a });
	}

	static std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return text;
	}

	// Point inside a square tile, in 0..1 tile units.
	static ImVec2 TilePoint(const glm::vec2& min, float size, float u, float v)
	{
		return { min.x + u * size, min.y + v * size };
	}

	static void DrawCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float cell)
	{
		drawList->AddRectFilled(min, max, ToColor(0.30f, 0.30f, 0.32f));
		drawList->PushClipRect(min, max, true);

		const ImU32 dark = ToColor(0.22f, 0.22f, 0.24f);
		int row = 0;
		for (float y = min.y; y < max.y; y += cell, row++)
		{
			int column = 0;
			for (float x = min.x; x < max.x; x += cell, column++)
			{
				if ((row + column) % 2 == 0)
					drawList->AddRectFilled({ x, y }, { x + cell, y + cell }, dark);
			}
		}

		drawList->PopClipRect();
	}


	// The loaded asset for a tile. A full image is only loaded while `loadsLeft` allows it (one is used per
	// load), so a folder of many prefabs fills in over a few frames. Never retries an asset that failed.
	template<typename T>
	static Ref<T> FindAsset(AssetHandle handle, int& loadsLeft)
	{
		if (static_cast<uint64_t>(handle) == 0)
		{
			return nullptr;
		}

		Ref<T> loaded = AssetManager::GetLoadedAsset<T>(handle);
		if (loaded != nullptr)
		{
			return loaded;
		}

		if (loadsLeft > 0 && AssetManager::GetLoadState(handle) == AssetLoadState::NotLoaded)
		{
			loadsLeft--;
			return AssetManager::GetAsset<T>(handle);
		}
		return nullptr;
	}

	// The first frame of the clip an animator starts with.
	static bool GetStartFrame(const SpriteAnimationSet& set, const std::string& startClip, Ref<Texture2D>& texture, uint32_t& spriteIndex, bool& flipX)
	{
		const int32_t clipIndex = set.FindStartClip(startClip);
		if (clipIndex < 0 || set.GetTexture() == nullptr)
		{
			return false;
		}

		const SpriteAnimationClip& clip = set.GetClip((uint32_t)clipIndex);
		if (clip.Frames.empty())
		{
			return false;
		}

		texture = set.GetTexture();
		spriteIndex = clip.Frames[0].Sprite;
		flipX = clip.FlipX;
		return true;
	}

	// What a sprite entity looks like at the start: its animation's first frame (the inspector shows the
	// same one), else its own sprite. False when there is nothing to draw yet (no image, or not loaded).
	static bool ResolveSprite(AssetHandle textureHandle, uint32_t spriteIndex, bool flipX, AssetHandle animations, const std::string& startClip,
		int& loadsLeft, Ref<Texture2D>& texture, uint32_t& outIndex, bool& outFlipX)
	{
		if (static_cast<uint64_t>(animations) != 0)
		{
			const Ref<SpriteAnimationSet> set = FindAsset<SpriteAnimationSet>(animations, loadsLeft);
			if (set != nullptr && GetStartFrame(*set, startClip, texture, outIndex, outFlipX))
			{
				return true;
			}
		}

		texture = FindAsset<Texture2D>(textureHandle, loadsLeft);
		outIndex = spriteIndex;
		outFlipX = flipX;
		return texture != nullptr;
	}

	// Sprite `spriteIndex` of `texture` fitted into the box, keeping its aspect ratio, tinted.
	static void DrawTileSprite(ImDrawList* drawList, const Ref<Texture2D>& texture, uint32_t spriteIndex, bool flipX, ImU32 tint, const ImVec2& boxMin, const ImVec2& boxMax)
	{
		const SpriteRegion& sprite = texture->GetSprite(spriteIndex);
		const float width = (float)std::max(sprite.Size.x, 1u);
		const float height = (float)std::max(sprite.Size.y, 1u);
		const float boxWidth = boxMax.x - boxMin.x;
		const float boxHeight = boxMax.y - boxMin.y;
		const float scale = std::min(boxWidth / width, boxHeight / height);
		const ImVec2 imageMin = { boxMin.x + (boxWidth - width * scale) * 0.5f, boxMin.y + (boxHeight - height * scale) * 0.5f };
		const ImVec2 imageMax = { imageMin.x + width * scale, imageMin.y + height * scale };

		TextureRect rect = texture->GetSpriteUVRect(spriteIndex);
		if (flipX)
		{
			std::swap(rect.Min.x, rect.Max.x);
		}
		drawList->AddImage((ImTextureID)Renderer2D::GetImGuiTexture(texture), imageMin, imageMax, { rect.Min.x, rect.Min.y }, { rect.Max.x, rect.Max.y }, tint);
	}

	// A sheet of paper: for files whose picture isn't ready or can't be made.
	static void DrawPageIcon(ImDrawList* drawList, const glm::vec2& min, float size)
	{
		const ImU32 page = ToColor(0.62f, 0.64f, 0.68f);
		drawList->AddRectFilled(TilePoint(min, size, 0.28f, 0.18f), TilePoint(min, size, 0.72f, 0.82f), page, 2.0f);
		drawList->AddTriangleFilled(TilePoint(min, size, 0.58f, 0.18f), TilePoint(min, size, 0.72f, 0.18f), TilePoint(min, size, 0.72f, 0.32f), ToColor(0.17f, 0.17f, 0.19f));
		for (int line = 0; line < 3; line++)
		{
			const float v = 0.42f + 0.12f * line;
			drawList->AddLine(TilePoint(min, size, 0.36f, v), TilePoint(min, size, 0.64f, v), ToColor(0.40f, 0.42f, 0.46f), 2.0f);
		}
	}

	void ProjectWindow::OnImguiRender(const Ref<Scene>& scene)
	{
		EnsureRoot();
		m_PreviewLoadsLeft = 2;

		if (m_NeedsRescan || ImGui::GetTime() - m_LastScanTime > RescanInterval)
			Rescan();

		ImGui::Begin("Project");

		DrawToolbar();
		ImGui::Separator();
		DrawGrid(scene);
		DrawDeletePopup();
		DrawNamePopup();

		ImGui::End();

		// Run scene / file changes now that nothing is iterating m_Items or the registry.
		std::vector<std::function<void()>> actions;
		actions.swap(m_DeferredActions);
		for (auto& action : actions)
			action();
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
			m_PrefabPreviews.clear();

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

		// The folder on disk: its sub-folders are items (folders aren't assets), and its asset files tell
		// whether anything was added, removed or renamed since the last look.
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

		// Files added, removed or renamed in this folder (by this window or outside the editor): rescan the
		// assets so they keep or get their handles. Not when only the folder changed.
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

		// The files are the registry's assets that live in this folder (the registry keeps paths relative to the
		// asset folder, with forward slashes). Files that are gone but still registered are left out.
		std::string folderKey = m_CurrentDirectory.lexically_relative(m_RootDirectory).generic_u8string();
		if (folderKey == ".")
		{
			folderKey.clear();
		}

		for (const auto& entry : AssetManager::GetRegistry().GetAll())
		{
			const AssetMetadata& metadata = entry.second;
			ItemType type = ItemType::Folder;
			if (metadata.IsMissing || ToItemType(metadata.Type, type) == false || metadata.Path.parent_path().generic_u8string() != folderKey)
			{
				continue;
			}

			AssetItem item;
			item.Path = m_CurrentDirectory / metadata.Path.filename();
			item.Type = type;
			item.Handle = metadata.Handle;
			item.DisplayName = (type == ItemType::Scene || type == ItemType::Prefab) ? item.Path.stem().u8string() : item.Path.filename().u8string();
			m_Items.push_back(std::move(item));
		}

		// Folders first, then by name.
		std::sort(m_Items.begin(), m_Items.end(), [](const AssetItem& a, const AssetItem& b)
		{
			const bool aFolder = a.Type == ItemType::Folder;
			const bool bFolder = b.Type == ItemType::Folder;
			if (aFolder != bFolder)
				return aFolder;

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
					continue;

				ImGui::TableNextColumn();
				DrawItem(item, scene, m_ThumbnailSize);
				visibleCount++;
			}

			ImGui::EndTable();
		}

		if (visibleCount == 0)
			ImGui::TextDisabled("%s", search.empty() ? (AssetManager::IsProjectOpen() ? "This folder is empty. Save a prefab from the hierarchy or inspector." : "No project is open.") : "No assets match the search.");

		// Click on empty space clears the selection; right-click opens the folder menu.
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsAnyItemHovered() == false)
			m_SelectedPath.clear();

		// F2 renames the selected folder or asset.
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false) &&
			m_SelectedPath.empty() == false && CanRename(m_SelectedPath))
		{
			BeginRename(m_SelectedPath);
		}

		if (ImGui::BeginPopupContextWindow("##ProjectContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("New Folder..."))
			{
				BeginCreateFolder();
			}
			if (ImGui::MenuItem("New Material"))
			{
				m_DeferredActions.push_back([this]() { CreateMaterial(); });
			}
			if (ImGui::MenuItem("Refresh"))
				m_NeedsRescan = true;

			ImGui::EndPopup();
		}

		ImGui::EndChild();
	}

	void ProjectWindow::DrawItem(const AssetItem& item, const Ref<Scene>& scene, float thumbnailSize)
	{
		ImGui::PushID(item.Path.u8string().c_str());

		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##Tile", { thumbnailSize, thumbnailSize }, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

		const bool hovered = ImGui::IsItemHovered();
		const bool selected = m_SelectedPath == item.Path;

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_SelectedPath = item.Path;
			if ((item.Type == ItemType::Texture || item.Type == ItemType::Animation || item.Type == ItemType::Material || item.Type == ItemType::Model) && OnAssetSelected)
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

		// Prefabs and scenes can be dragged onto the viewport / hierarchy, images onto a sprite's
		// Texture field in the inspector, sprite animations onto a Sprite Animator's Animations field.
		// What is dragged is the asset's handle (AssetDragDrop).
		const bool isTexture = item.Type == ItemType::Texture;
		const bool isAnimationSet = item.Type == ItemType::Animation;
		const bool isMaterial = item.Type == ItemType::Material;
		const bool isModel = item.Type == ItemType::Model;
		if ((item.Type == ItemType::Prefab || item.Type == ItemType::Scene || isTexture || isAnimationSet || isMaterial || isModel) && ImGui::BeginDragDropSource())
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
				m_DeferredActions.push_back([this, copy, sceneCopy]() { InstantiatePrefab(copy.Path, sceneCopy); });
			if (item.Type == ItemType::Scene && ImGui::MenuItem("Open Scene"))
				m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
			if (item.Type == ItemType::Folder && ImGui::MenuItem("Open Folder"))
				m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
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
					m_DeferredActions.push_back([this, copy]() { CreateSpriteAnimation(copy.Path); });
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
			if (CanRename(item.Path) && ImGui::MenuItem("Rename...", "F2"))
			{
				BeginRename(item.Path);
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Delete..."))
			{
				m_PendingDeletePath = item.Path;
				m_RequestDeletePopup = true;
			}

			ImGui::EndPopup();
		}
		else if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left) == false)
		{
			DrawItemTooltip(item);
		}

		DrawThumbnail(item, { cursor.x, cursor.y }, thumbnailSize, hovered, selected);

		// Name under the tile, wrapped to the tile width.
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);
		ImGui::TextWrapped("%s", item.DisplayName.c_str());
		ImGui::PopTextWrapPos();

		ImGui::PopID();
	}

	void ProjectWindow::DrawThumbnail(const AssetItem& item, const glm::vec2& min, float size, bool hovered, bool selected)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 tileMin = { min.x, min.y };
		const ImVec2 tileMax = { min.x + size, min.y + size };
		const float rounding = 4.0f;

		// Tile background + selection.
		drawList->AddRectFilled(tileMin, tileMax, hovered ? ToColor(0.24f, 0.24f, 0.27f) : ToColor(0.17f, 0.17f, 0.19f), rounding);
		if (selected)
			drawList->AddRect(tileMin, tileMax, ToColor(0.26f, 0.59f, 0.98f), rounding, 0, 2.0f);

		switch (item.Type)
		{
			case ItemType::Folder:
			{
				const ImU32 folder = ToColor(0.86f, 0.66f, 0.28f);
				const ImU32 folderDark = ToColor(0.72f, 0.53f, 0.20f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.26f), TilePoint(min, size, 0.46f, 0.36f), folderDark, 3.0f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.32f), TilePoint(min, size, 0.84f, 0.78f), folder, 3.0f);
				break;
			}
			case ItemType::Scene:
			{
				const ImVec2 frameMin = TilePoint(min, size, 0.16f, 0.22f);
				const ImVec2 frameMax = TilePoint(min, size, 0.84f, 0.78f);
				drawList->AddRectFilled(frameMin, frameMax, ToColor(0.16f, 0.30f, 0.50f), 3.0f);
				drawList->AddCircleFilled(TilePoint(min, size, 0.68f, 0.36f), size * 0.06f, ToColor(0.98f, 0.85f, 0.35f));
				drawList->AddTriangleFilled(TilePoint(min, size, 0.20f, 0.76f), TilePoint(min, size, 0.42f, 0.44f), TilePoint(min, size, 0.62f, 0.76f), ToColor(0.30f, 0.62f, 0.35f));
				drawList->AddTriangleFilled(TilePoint(min, size, 0.46f, 0.76f), TilePoint(min, size, 0.62f, 0.54f), TilePoint(min, size, 0.80f, 0.76f), ToColor(0.22f, 0.50f, 0.28f));
				drawList->AddRect(frameMin, frameMax, ToColor(0.60f, 0.75f, 0.95f), 3.0f, 0, 1.5f);
				// Green strip = scene.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.30f, 0.70f, 0.40f), rounding, ImDrawFlags_RoundCornersBottom);
				break;
			}
			case ItemType::Prefab:
			{
				const PrefabPreview& preview = GetPrefabPreview(item.Path);

				// The first frame of its animation, or its sprite's image; none when it has no image.
				Ref<Texture2D> spriteTexture;
				uint32_t spriteIndex = 0;
				bool spriteFlipX = false;
				const bool hasImage = preview.Valid && preview.HasSprite &&
					ResolveSprite(preview.TextureHandle, preview.SpriteIndex, preview.FlipX, preview.AnimationsHandle, preview.StartClip, m_PreviewLoadsLeft, spriteTexture, spriteIndex, spriteFlipX);

				const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
				const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.88f);
				DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));

				const ImVec2 center = { (innerMin.x + innerMax.x) * 0.5f, (innerMin.y + innerMax.y) * 0.5f };
				const float extent = (innerMax.x - innerMin.x) * 0.36f; // half size of the biggest side

				if (preview.Valid == false)
				{
					// Couldn't read the file.
					drawList->AddLine({ center.x - extent, center.y - extent }, { center.x + extent, center.y + extent }, ToColor(0.90f, 0.30f, 0.30f), 3.0f);
					drawList->AddLine({ center.x - extent, center.y + extent }, { center.x + extent, center.y - extent }, ToColor(0.90f, 0.30f, 0.30f), 3.0f);
				}
				else if (hasImage)
				{
					DrawTileSprite(drawList, spriteTexture, spriteIndex, spriteFlipX,
						ImGui::ColorConvertFloat4ToU32({ preview.Color.r, preview.Color.g, preview.Color.b, preview.Color.a }), innerMin, innerMax);
				}
				else if (preview.HasSprite)
				{
					// No image (none set, or not loaded yet): the sprite as a coloured quad: colour, aspect ratio from scale, Z rotation.
					const float sx = std::max(std::abs(preview.Scale.x), 0.0001f);
					const float sy = std::max(std::abs(preview.Scale.y), 0.0001f);
					const float largest = std::max(sx, sy);
					const float halfW = extent * sx / largest;
					const float halfH = extent * sy / largest;

					const float c = std::cos(preview.RotationZ);
					const float s = std::sin(preview.RotationZ);
					auto corner = [&](float x, float y) -> ImVec2 { return { center.x + x * c - y * s, center.y + x * s + y * c }; };

					const ImVec2 p0 = corner(-halfW, -halfH);
					const ImVec2 p1 = corner(halfW, -halfH);
					const ImVec2 p2 = corner(halfW, halfH);
					const ImVec2 p3 = corner(-halfW, halfH);

					drawList->AddQuadFilled(p0, p1, p2, p3, ImGui::ColorConvertFloat4ToU32({ preview.Color.r, preview.Color.g, preview.Color.b, preview.Color.a }));
					drawList->AddQuad(p0, p1, p2, p3, ToColor(0.0f, 0.0f, 0.0f, 0.45f), 1.0f);
				}
				else
				{
					// Entity without a sprite: show its origin.
					const ImU32 gizmo = ToColor(0.85f, 0.85f, 0.90f);
					drawList->AddCircle(center, extent * 0.55f, gizmo, 24, 1.5f);
					drawList->AddLine({ center.x, center.y }, { center.x + extent * 0.8f, center.y }, ToColor(0.85f, 0.25f, 0.25f), 2.0f);
					drawList->AddLine({ center.x, center.y }, { center.x, center.y - extent * 0.8f }, ToColor(0.30f, 0.80f, 0.30f), 2.0f);
				}

				// Blue strip = prefab.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.26f, 0.59f, 0.98f), rounding, ImDrawFlags_RoundCornersBottom);
				break;
			}
			case ItemType::Texture:
			{
				const Thumbnail& thumbnail = ThumbnailManager::Get(item.Handle);
				if (thumbnail.Texture != nullptr)
				{
					const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
					const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.90f);
					DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));
					DrawTileSprite(drawList, thumbnail.Texture, 0, false, ToColor(1.0f, 1.0f, 1.0f), innerMin, innerMax);

					// Sprite sheets get a strip, like scenes and prefabs.
					if (thumbnail.IsSheet)
					{
						drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ImGui::GetColorU32(ImGuiCol_CheckMark), rounding, ImDrawFlags_RoundCornersBottom);
					}
				}
				else
				{
					DrawPageIcon(drawList, min, size);
				}
				break;
			}
			case ItemType::Animation:
			{
				Ref<Texture2D> frameTexture;
				uint32_t frameSprite = 0;
				bool frameFlipX = false;
				const Ref<SpriteAnimationSet> set = FindAsset<SpriteAnimationSet>(item.Handle, m_PreviewLoadsLeft);
				if (set != nullptr && GetStartFrame(*set, std::string(), frameTexture, frameSprite, frameFlipX))
				{
					const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
					const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.88f);
					DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));
					DrawTileSprite(drawList, frameTexture, frameSprite, frameFlipX, ToColor(1.0f, 1.0f, 1.0f), innerMin, innerMax);
				}
				else
				{
					DrawPageIcon(drawList, min, size);
				}

				// Purple strip = animation set.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.62f, 0.40f, 0.90f), rounding, ImDrawFlags_RoundCornersBottom);
				break;
			}
			case ItemType::Material:
			{
				// A round swatch: its albedo image (or white) times its tint.
				const Ref<MaterialAsset> material = FindAsset<MaterialAsset>(item.Handle, m_PreviewLoadsLeft);
				const ImVec2 swatchMin = TilePoint(min, size, 0.18f, 0.14f);
				const ImVec2 swatchMax = TilePoint(min, size, 0.82f, 0.78f);
				const float radius = (swatchMax.x - swatchMin.x) * 0.5f;
				const ImVec2 center = { swatchMin.x + radius, swatchMin.y + radius };
				if (material != nullptr)
				{
					const MaterialData& data = material->GetData();
					const ImU32 tint = ImGui::ColorConvertFloat4ToU32({ data.Tint.r, data.Tint.g, data.Tint.b, 1.0f });
					const Ref<Texture2D> albedo = (uint64_t)data.Albedo != 0 ? ThumbnailManager::Get(data.Albedo).Texture : nullptr;
					if (albedo != nullptr)
					{
						drawList->AddImageRounded((ImTextureID)Renderer2D::GetImGuiTexture(albedo), swatchMin, swatchMax, { 0.0f, 0.0f }, { 1.0f, 1.0f }, tint, radius);
					}
					else
					{
						drawList->AddCircleFilled(center, radius, tint, 32);
					}
					// A highlight, so it reads as a ball.
					drawList->AddCircleFilled({ center.x - radius * 0.35f, center.y - radius * 0.35f }, radius * 0.18f, ToColor(1.0f, 1.0f, 1.0f, 0.35f), 16);
				}
				else
				{
					drawList->AddCircleFilled(center, radius, ToColor(0.45f, 0.45f, 0.48f), 32);
				}
				drawList->AddCircle(center, radius, ToColor(0.0f, 0.0f, 0.0f, 0.5f), 32, 1.5f);

				// Orange strip = material.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.95f, 0.55f, 0.20f), rounding, ImDrawFlags_RoundCornersBottom);
				break;
			}
			case ItemType::Model:
			{
				// A cube seen from above the front-right: three shaded faces.
				const ImVec2 top[4] = { TilePoint(min, size, 0.50f, 0.18f), TilePoint(min, size, 0.80f, 0.32f), TilePoint(min, size, 0.50f, 0.46f), TilePoint(min, size, 0.20f, 0.32f) };
				const ImVec2 left[4] = { TilePoint(min, size, 0.20f, 0.32f), TilePoint(min, size, 0.50f, 0.46f), TilePoint(min, size, 0.50f, 0.82f), TilePoint(min, size, 0.20f, 0.68f) };
				const ImVec2 right[4] = { TilePoint(min, size, 0.50f, 0.46f), TilePoint(min, size, 0.80f, 0.32f), TilePoint(min, size, 0.80f, 0.68f), TilePoint(min, size, 0.50f, 0.82f) };
				drawList->AddConvexPolyFilled(top, 4, ToColor(0.78f, 0.80f, 0.86f));
				drawList->AddConvexPolyFilled(left, 4, ToColor(0.52f, 0.55f, 0.62f));
				drawList->AddConvexPolyFilled(right, 4, ToColor(0.38f, 0.41f, 0.48f));

				// Teal strip = model.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.20f, 0.70f, 0.70f), rounding, ImDrawFlags_RoundCornersBottom);
				break;
			}
		}
	}

	void ProjectWindow::DrawItemTooltip(const AssetItem& item)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(item.DisplayName.c_str());

		switch (item.Type)
		{
			case ItemType::Prefab:
			{
				const PrefabPreview& preview = GetPrefabPreview(item.Path);
				if (preview.Valid == false)
				{
					ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read prefab file");
					break;
				}

				ImGui::TextDisabled("Prefab");
				ImGui::Separator();
				ImGui::BulletText("Tag");
				if (preview.HasTransform)
					ImGui::BulletText("Transform  (scale %.1f x %.1f)", preview.Scale.x, preview.Scale.y);
				if (preview.HasSprite)
				{
					ImGui::BulletText("Sprite Renderer");
					ImGui::SameLine();
					ImGui::ColorButton("##color", { preview.Color.r, preview.Color.g, preview.Color.b, preview.Color.a }, ImGuiColorEditFlags_NoTooltip, { 12.0f, 12.0f });
				}
				ImGui::Spacing();
				ImGui::TextDisabled("Double-click or drag into the scene");
				break;
			}
			case ItemType::Scene:
				ImGui::TextDisabled("Scene - double-click or drag to the viewport to open");
				break;
			case ItemType::Folder:
				ImGui::TextDisabled("Folder");
				break;
			case ItemType::Texture:
				ImGui::TextDisabled(ThumbnailManager::Get(item.Handle).IsSheet ? "Image - sprite sheet" : "Image");
				break;
			case ItemType::Animation:
				ImGui::TextDisabled("Sprite animation");
				break;
			case ItemType::Material:
				ImGui::TextDisabled("Material (Standard) - drag onto a mesh's material Asset field");
				break;
			case ItemType::Model:
				ImGui::TextDisabled("Model (glTF) - drag into the viewport to place it");
				break;
		}

		if (item.Type != ItemType::Folder)
		{
			ImGui::TextDisabled("Asset handle %llu", (unsigned long long)(uint64_t)item.Handle);
		}

		ImGui::EndTooltip();
	}

	void ProjectWindow::DrawDeletePopup()
	{
		if (m_RequestDeletePopup)
		{
			ImGui::OpenPopup("Delete Asset?");
			m_RequestDeletePopup = false;
		}

		if (ImGui::BeginPopupModal("Delete Asset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			std::error_code ec;
			const bool isFolder = std::filesystem::is_directory(m_PendingDeletePath, ec);
			const bool canDelete = isFolder == false || std::filesystem::is_empty(m_PendingDeletePath, ec);

			ImGui::Text("Delete '%s'?", m_PendingDeletePath.filename().u8string().c_str());
			if (canDelete)
				ImGui::TextDisabled("This cannot be undone. Scenes that use it will report it as missing.");
			else
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Only empty folders can be deleted.");

			ImGui::Spacing();
			ImGui::BeginDisabled(canDelete == false);
			if (ImGui::Button("Delete", { 120.0f, 0.0f }))
			{
				// The manager deletes the file and its .hymeta, keeps the handle (marked missing, so scenes that
				// use it report it) and logs why when it can't.
				if (AssetManager::DeleteAsset(m_PendingDeletePath))
				{
					HY_INFO("Deleted {}", m_PendingDeletePath.u8string());
					if (OnPathDeleted)
					{
						const std::filesystem::path deleted = m_PendingDeletePath;
						m_DeferredActions.push_back([this, deleted]() { OnPathDeleted(deleted); });
					}
				}

				m_PrefabPreviews.erase(m_PendingDeletePath.u8string());
				if (m_SelectedPath == m_PendingDeletePath)
					m_SelectedPath.clear();
				m_PendingDeletePath.clear();
				m_NeedsRescan = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 120.0f, 0.0f }))
			{
				m_PendingDeletePath.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
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
					OnOpenScene(item.Path);
				break;
			case ItemType::Texture:
			case ItemType::Animation:
				if (OnOpenAsset)
				{
					OnOpenAsset(item.Path);
				}
				break;
			case ItemType::Material:
			case ItemType::Model:
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
		if (scene == nullptr)
			return;

		Entity entity = PrefabSerializer::Instantiate(*scene, path);
		if (entity && OnEntityCreated)
			OnEntityCreated(entity);
	}

	void ProjectWindow::BeginCreateFolder()
	{
		// Suggest a name that doesn't exist yet; the user can type over it.
		std::error_code ec;
		std::string name = "New Folder";
		for (int i = 2; std::filesystem::exists(m_CurrentDirectory / std::filesystem::u8path(name), ec); i++)
		{
			name = "New Folder " + std::to_string(i);
		}

		m_RenamePath.clear();
		m_RenameExtension.clear();
		std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", name.c_str());
		m_RequestNamePopup = true;
	}

	bool ProjectWindow::CanRename(const std::filesystem::path& path)
	{
		std::error_code ec;
		return std::filesystem::is_directory(path, ec) || static_cast<uint64_t>(AssetManager::FindHandle(path)) != 0;
	}

	void ProjectWindow::BeginRename(const std::filesystem::path& path)
	{
		// Folders: the whole name. Files: the name without the extension, which stays as it is.
		std::error_code ec;
		const bool isFolder = std::filesystem::is_directory(path, ec);
		m_RenamePath = path;
		m_RenameExtension = isFolder ? std::string() : path.extension().u8string();
		const std::string name = isFolder ? path.filename().u8string() : path.stem().u8string();
		std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", name.c_str());
		m_RequestNamePopup = true;
	}

	std::string ProjectWindow::ValidateName(const std::string& name, const std::string& extension, const std::filesystem::path& parent, const std::filesystem::path& ignore)
	{
		if (name.empty())
		{
			return "Enter a name.";
		}
		if (name == "." || name == "..")
		{
			return "That name is reserved.";
		}
		if (name.front() == ' ' || name.back() == ' ' || name.back() == '.')
		{
			return "Names can't start or end with a space, or end with a dot.";
		}
		for (char c : name)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' || (unsigned char)c < 32)
			{
				return "Names can't contain < > : \" / \\ | ? *";
			}
		}

		// Windows file names aren't case sensitive, so "textures" clashes with "Textures".
		const std::filesystem::path target = parent / std::filesystem::u8path(name + extension);
		std::error_code ec;
		if (std::filesystem::exists(target, ec) && (ignore.empty() || std::filesystem::equivalent(target, ignore, ec) == false))
		{
			return "Something with that name already exists here.";
		}

		return {};
	}

	void ProjectWindow::DrawNamePopup()
	{
		const char* popupId = "Name";
		if (m_RequestNamePopup)
		{
			ImGui::OpenPopup(popupId);
			m_RequestNamePopup = false;
		}

		if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const bool renaming = m_RenamePath.empty() == false;
			const bool renamingFile = renaming && m_RenameExtension.empty() == false;
			const std::filesystem::path parent = renaming ? m_RenamePath.parent_path() : m_CurrentDirectory;

			ImGui::TextUnformatted(renaming == false ? "New folder" : (renamingFile ? "Rename asset" : "Rename folder"));

			// Focus the field (with its text selected) when the popup opens.
			if (ImGui::IsWindowAppearing())
			{
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(300.0f);
			const bool entered = ImGui::InputText("##Name", m_NameBuffer, sizeof(m_NameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (renamingFile)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", m_RenameExtension.c_str());
			}

			const std::string name = m_NameBuffer;
			const std::string problem = ValidateName(name, m_RenameExtension, parent, m_RenamePath);
			if (problem.empty() == false)
			{
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "%s", problem.c_str());
			}
			else if (renaming && renamingFile == false)
			{
				// Sprites and animators reference textures and .hyanim files by asset handle, so they follow a
				// rename. Scenes saved before handles existed are given one the next time they are saved.
				ImGui::TextDisabled("Sprites in saved scenes follow by asset handle. Scenes from before handles follow once re-saved.");
			}

			ImGui::Spacing();
			ImGui::BeginDisabled(problem.empty() == false);
			const bool confirm = ImGui::Button(renaming ? "Rename" : "Create", { 120.0f, 0.0f }) || (entered && problem.empty());
			ImGui::EndDisabled();

			ImGui::SameLine();
			const bool cancel = ImGui::Button("Cancel", { 120.0f, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

			if (confirm && problem.empty())
			{
				const std::filesystem::path target = parent / std::filesystem::u8path(name + m_RenameExtension);
				bool succeeded = false;
				if (renaming)
				{
					// The manager moves the file with its .hymeta and keeps the handle, so everything that refers
					// to it by handle follows; it logs why when it can't.
					succeeded = AssetManager::MoveAsset(m_RenamePath, target);
				}
				else
				{
					std::error_code ec;
					succeeded = std::filesystem::create_directory(target, ec);
					if (succeeded == false)
					{
						HY_ERROR("Could not create {}: {}", target.u8string(), ec.message());
					}
				}

				if (succeeded)
				{
					// Keep the browser where it was: follow the folder if we were inside it.
					const std::filesystem::path relative = m_CurrentDirectory.lexically_relative(m_RenamePath);
					if (renaming && relative.empty() == false && *relative.begin() != "..")
					{
						m_CurrentDirectory = (target / relative).lexically_normal();
					}
					m_SelectedPath = target;

					if (renaming && OnPathRenamed)
					{
						const std::filesystem::path from = m_RenamePath;
						m_DeferredActions.push_back([this, from, target]() { OnPathRenamed(from, target); });
					}
				}

				m_RenamePath.clear();
				m_RenameExtension.clear();
				m_NeedsRescan = true;
				ImGui::CloseCurrentPopup();
			}
			else if (cancel)
			{
				m_RenamePath.clear();
				m_RenameExtension.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	const ProjectWindow::PrefabPreview& ProjectWindow::GetPrefabPreview(const std::filesystem::path& path)
	{
		std::error_code ec;
		const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, ec);

		const std::string key = path.u8string();
		auto it = m_PrefabPreviews.find(key);
		if (it != m_PrefabPreviews.end() && it->second.WriteTime == writeTime)
			return it->second;

		// (Re)load: deserialize into a throwaway scene and copy out what the thumbnail needs.
		// Uses the same loader as instantiating, so previews always match what you'd get.
		PrefabPreview preview;
		preview.WriteTime = writeTime;

		Scene previewScene("PrefabPreview");
		Entity entity = PrefabSerializer::Load(previewScene, path);
		if (entity)
		{
			preview.Valid = true;
			preview.Name = entity.GetName();

			if (entity.HasComponent<TransformComponent>())
			{
				const TransformComponent& transform = entity.GetComponent<TransformComponent>();
				preview.HasTransform = true;
				preview.Scale = transform.Scale;
				preview.RotationZ = transform.Rotation.z;
			}

			if (entity.HasComponent<SpriteRendererComponent>())
			{
				const SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();
				preview.HasSprite = true;
				preview.Color = sprite.Color;
				preview.TextureHandle = sprite.Texture;
				preview.SpriteIndex = sprite.SpriteIndex;
				preview.FlipX = sprite.FlipX;
			}

			if (entity.HasComponent<SpriteAnimatorComponent>())
			{
				const SpriteAnimatorComponent& animator = entity.GetComponent<SpriteAnimatorComponent>();
				preview.AnimationsHandle = animator.Animations;
				preview.StartClip = animator.StartClip;
			}
		}

		return m_PrefabPreviews[key] = preview;
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
			default:
				return false;
		}
	}

	void ProjectWindow::CreateSpriteAnimation(const std::filesystem::path& imagePath)
	{
		const Ref<Texture2D> texture = Texture2D::LoadFromFile(imagePath);
		if (texture == nullptr)
		{
			return;
		}

		// "<image>.hyanim", or "<image> 2.hyanim"... when taken.
		const std::filesystem::path folder = imagePath.parent_path();
		const std::string stem = imagePath.stem().u8string();
		std::filesystem::path target = folder / std::filesystem::u8path(stem + SpriteAnimationSerializer::FileExtension);
		std::error_code error;
		for (int number = 2; std::filesystem::exists(target, error); number++)
		{
			target = folder / std::filesystem::u8path(stem + " " + std::to_string(number) + SpriteAnimationSerializer::FileExtension);
		}

		SpriteAnimationSetSpecification specification;
		specification.Texture = texture;
		specification.Clips = SpriteAnimationWindow::MakeClipsFromSpriteNames(*texture, 10.0f);
		const SpriteAnimationSet set(specification);
		if (SpriteAnimationSerializer::Serialize(target, set) == false)
		{
			return;
		}

		HY_INFO("Created {} ({} clips)", target.filename().u8string(), set.GetClipCount());
		m_SelectedPath = target;
		m_NeedsRescan = true;
		if (OnOpenAsset)
		{
			OnOpenAsset(target);
		}
	}
	void ProjectWindow::CreateMaterial()
	{
		std::filesystem::path folder = m_CurrentDirectory.empty() ? m_RootDirectory : m_CurrentDirectory;
		std::filesystem::path target = folder / std::filesystem::u8path(std::string("New Material") + MaterialSerializer::FileExtension);
		std::error_code error;
		for (int number = 2; std::filesystem::exists(target, error); number++)
		{
			target = folder / std::filesystem::u8path("New Material " + std::to_string(number) + MaterialSerializer::FileExtension);
		}

		if (MaterialSerializer::Serialize(target, MaterialData{}) == false)
		{
			return;
		}

		HY_INFO("Created {}", target.filename().u8string());
		m_SelectedPath = target;
		m_NeedsRescan = true;
		if (OnAssetSelected)
		{
			OnAssetSelected(target);
		}
	}
}
