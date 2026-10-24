#include "ProjectWindow.h"
#include "Utilities/AssetDragDrop.h"
#include "../EditorPaths.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Asset/AssetMetaSerializer.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/SceneSerializer.h>
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

	void ProjectWindow::OnImguiRender(const Ref<Scene>& scene)
	{
		EnsureRoot();

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

		std::error_code ec;
		for (std::filesystem::directory_iterator it(m_CurrentDirectory, ec), end; ec.value() == 0 && it != end; it.increment(ec))
		{
			const std::filesystem::path& path = it->path();
			const std::string fileName = path.filename().u8string();
			if (fileName.empty() || fileName[0] == '.' || AssetMetaSerializer::IsSidecar(path))
			{
				continue;
			}

			AssetItem item;
			item.Path = path;
			item.Type = GetItemType(path);
			item.DisplayName = (item.Type == ItemType::Folder || item.Type == ItemType::Other) ? fileName : path.stem().u8string();
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

		// Files added, removed or renamed in this folder since the last look (by this window or outside
		// the editor): rescan the assets so they keep or get their handles. Not when only the folder changed.
		std::vector<std::filesystem::path> listed;
		listed.reserve(m_Items.size());
		for (const AssetItem& item : m_Items)
		{
			listed.push_back(item.Path);
		}

		const bool sameFolder = m_ListedDirectory == m_CurrentDirectory;
		if (sameFolder && listed != m_ListedPaths)
		{
			AssetManager::Rescan();
		}
		m_ListedDirectory = m_CurrentDirectory;
		m_ListedPaths = std::move(listed);

		// Only the shown folder's images keep a thumbnail texture (GPU memory).
		for (auto it = m_TextureThumbnails.begin(); it != m_TextureThumbnails.end();)
		{
			const bool shown = std::any_of(m_Items.begin(), m_Items.end(), [&](const AssetItem& item) { return item.Path.u8string() == it->first; });
			if (shown)
			{
				++it;
			}
			else
			{
				it = m_TextureThumbnails.erase(it);
			}
		}
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
			ImGui::TextDisabled("%s", search.empty() ? "This folder is empty. Save a prefab from the hierarchy or inspector." : "No assets match the search.");

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
			if (item.Type == ItemType::Other && OnAssetSelected)
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
		const bool isTexture = item.Type == ItemType::Other && Texture2D::IsSupportedFile(item.Path);
		const bool isAnimationSet = item.Type == ItemType::Other && item.Path.extension() == SpriteAnimationSerializer::FileExtension;
		if ((item.Type == ItemType::Prefab || item.Type == ItemType::Scene || isTexture || isAnimationSet) && ImGui::BeginDragDropSource())
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
				const Ref<Texture2D>& texture = GetTextureThumbnail(item.Path);
				const bool isSheet = texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple;
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
				else if (preview.HasSprite)
				{
					// The sprite as it will look: colour, aspect ratio from scale, Z rotation.
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
			case ItemType::Other:
			default:
			{
				if (Texture2D::IsSupportedFile(item.Path))
				{
					const Ref<Texture2D>& texture = GetTextureThumbnail(item.Path);
					if (texture != nullptr)
					{
						const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
						const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.90f);
						DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));

						// The image fitted into the tile, keeping its aspect ratio.
						const float box = innerMax.x - innerMin.x;
						const float width = (float)std::max(texture->GetWidth(), 1u);
						const float height = (float)std::max(texture->GetHeight(), 1u);
						const float scale = box / std::max(width, height);
						const ImVec2 imageMin = { innerMin.x + (box - width * scale) * 0.5f, innerMin.y + (box - height * scale) * 0.5f };
						drawList->AddImage((ImTextureID)Renderer2D::GetImGuiTexture(texture), imageMin, { imageMin.x + width * scale, imageMin.y + height * scale });

						// Sprite sheets get a strip, like scenes and prefabs.
						if (texture->GetSpriteMode() == SpriteMode::Multiple)
						{
							drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ImGui::GetColorU32(ImGuiCol_CheckMark), rounding, ImDrawFlags_RoundCornersBottom);
						}
						break;
					}
				}

				const ImU32 page = ToColor(0.62f, 0.64f, 0.68f);
				drawList->AddRectFilled(TilePoint(min, size, 0.28f, 0.18f), TilePoint(min, size, 0.72f, 0.82f), page, 2.0f);
				drawList->AddTriangleFilled(TilePoint(min, size, 0.58f, 0.18f), TilePoint(min, size, 0.72f, 0.18f), TilePoint(min, size, 0.72f, 0.32f), ToColor(0.17f, 0.17f, 0.19f));
				for (int line = 0; line < 3; line++)
				{
					const float v = 0.42f + 0.12f * line;
					drawList->AddLine(TilePoint(min, size, 0.36f, v), TilePoint(min, size, 0.64f, v), ToColor(0.40f, 0.42f, 0.46f), 2.0f);
				}
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
			default:
				break;
		}

		if (item.Type != ItemType::Folder && GetAssetTypeFromExtension(item.Path) != AssetType::None)
		{
			const AssetHandle handle = AssetManager::FindHandle(item.Path);
			if ((uint64_t)handle != 0)
			{
				ImGui::TextDisabled("Asset handle %llu", (unsigned long long)(uint64_t)handle);
			}
			else
			{
				ImGui::TextDisabled("No asset handle yet");
			}
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
			default:
			{
				const bool editable = Texture2D::IsSupportedFile(item.Path) || item.Path.extension() == SpriteAnimationSerializer::FileExtension;
				if (editable && OnOpenAsset)
				{
					OnOpenAsset(item.Path);
				}
				break;
			}
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
		return GetItemType(path) == ItemType::Folder || static_cast<uint64_t>(AssetManager::FindHandle(path)) != 0;
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
				preview.HasSprite = true;
				preview.Color = entity.GetComponent<SpriteRendererComponent>().Color;
			}
		}

		return m_PrefabPreviews[key] = preview;
	}

	ProjectWindow::ItemType ProjectWindow::GetItemType(const std::filesystem::path& path)
	{
		std::error_code ec;
		if (std::filesystem::is_directory(path, ec))
			return ItemType::Folder;

		const std::string extension = ToLower(path.extension().u8string());
		if (extension == PrefabSerializer::FileExtension)
			return ItemType::Prefab;
		if (extension == SceneSerializer::FileExtension)
			return ItemType::Scene;

		return ItemType::Other;
	}

	const Ref<Texture2D>& ProjectWindow::GetTextureThumbnail(const std::filesystem::path& path)
	{
		TextureThumbnail& thumbnail = m_TextureThumbnails[path.u8string()];

		// The .hymeta is checked once a second, like the folder; a re-import loads the image again.
		// A file that can't be loaded isn't tried again until its import changes (no log every frame).
		const double now = ImGui::GetTime();
		if (thumbnail.Loaded == false || now - thumbnail.LastCheckTime >= RescanInterval)
		{
			thumbnail.LastCheckTime = now;
			std::error_code error;
			std::filesystem::file_time_type importTime = std::filesystem::last_write_time(Texture2D::GetImportFilePath(path), error);
			if (error)
			{
				importTime = {};
			}

			if (thumbnail.Loaded == false || importTime != thumbnail.ImportTime)
			{
				thumbnail.Texture = Texture2D::LoadUnshared(path);
				thumbnail.ImportTime = importTime;
				thumbnail.Loaded = true;
			}
		}
		return thumbnail.Texture;
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
}
