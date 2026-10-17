#include "ProjectWindow.h"
#include "AssetDragDrop.h"
#include "../EditorPaths.h"

#include <imgui/imgui.h>

#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/SceneSerializer.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <system_error>

namespace Hydro
{
	namespace
	{
		constexpr double RescanInterval = 1.0; // seconds; picks up files saved from other windows / Explorer

		ImU32 ToColor(float r, float g, float b, float a = 1.0f)
		{
			return ImGui::ColorConvertFloat4ToU32({ r, g, b, a });
		}

		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			return text;
		}

		// Point inside a square tile, in 0..1 tile units.
		ImVec2 TilePoint(const glm::vec2& min, float size, float u, float v)
		{
			return { min.x + u * size, min.y + v * size };
		}

		void DrawCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float cell)
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
			if (fileName.empty() || fileName[0] == '.')
				continue;

			AssetItem item;
			item.Path = path;
			item.Type = GetAssetType(path);
			item.DisplayName = (item.Type == AssetType::Folder || item.Type == AssetType::Other) ? fileName : path.stem().u8string();
			m_Items.push_back(std::move(item));
		}

		// Folders first, then by name.
		std::sort(m_Items.begin(), m_Items.end(), [](const AssetItem& a, const AssetItem& b)
		{
			const bool aFolder = a.Type == AssetType::Folder;
			const bool bFolder = b.Type == AssetType::Folder;
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
			ImGui::TextDisabled("%s", search.empty() ? "This folder is empty. Save a prefab from the hierarchy or inspector." : "No assets match the search.");

		// Click on empty space clears the selection; right-click opens the folder menu.
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsAnyItemHovered() == false)
			m_SelectedPath.clear();

		if (ImGui::BeginPopupContextWindow("##ProjectContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("New Folder"))
				m_DeferredActions.push_back([this]() { CreateFolder(); });
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
			m_SelectedPath = item.Path;

		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			const AssetItem copy = item;
			Ref<Scene> sceneCopy = scene;
			m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
		}

		// Prefabs and scenes can be dragged onto the viewport / hierarchy.
		if ((item.Type == AssetType::Prefab || item.Type == AssetType::Scene) && ImGui::BeginDragDropSource())
		{
			AssetDragDrop::SetPayload(item.Path);
			ImGui::TextUnformatted(item.DisplayName.c_str());
			ImGui::TextDisabled("%s", item.Type == AssetType::Prefab ? "Drop to instantiate" : "Drop to open");
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginPopupContextItem("##ItemContext"))
		{
			const AssetItem copy = item;
			Ref<Scene> sceneCopy = scene;

			if (item.Type == AssetType::Prefab && ImGui::MenuItem("Instantiate"))
				m_DeferredActions.push_back([this, copy, sceneCopy]() { InstantiatePrefab(copy.Path, sceneCopy); });
			if (item.Type == AssetType::Scene && ImGui::MenuItem("Open Scene"))
				m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });
			if (item.Type == AssetType::Folder && ImGui::MenuItem("Open Folder"))
				m_DeferredActions.push_back([this, copy, sceneCopy]() { ActivateItem(copy, sceneCopy); });

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
			case AssetType::Folder:
			{
				const ImU32 folder = ToColor(0.86f, 0.66f, 0.28f);
				const ImU32 folderDark = ToColor(0.72f, 0.53f, 0.20f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.26f), TilePoint(min, size, 0.46f, 0.36f), folderDark, 3.0f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.32f), TilePoint(min, size, 0.84f, 0.78f), folder, 3.0f);
				break;
			}
			case AssetType::Scene:
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
			case AssetType::Prefab:
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
			case AssetType::Other:
			default:
			{
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
			case AssetType::Prefab:
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
			case AssetType::Scene:
				ImGui::TextDisabled("Scene - double-click or drag to the viewport to open");
				break;
			case AssetType::Folder:
				ImGui::TextDisabled("Folder");
				break;
			default:
				break;
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
				ImGui::TextDisabled("This cannot be undone. Entities already in a scene are not affected.");
			else
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Only empty folders can be deleted.");

			ImGui::Spacing();
			ImGui::BeginDisabled(canDelete == false);
			if (ImGui::Button("Delete", { 120.0f, 0.0f }))
			{
				if (std::filesystem::remove(m_PendingDeletePath, ec))
					HY_INFO("Deleted {}", m_PendingDeletePath.u8string());
				else
					HY_ERROR("Could not delete {}: {}", m_PendingDeletePath.u8string(), ec.message());

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
			case AssetType::Folder:
				m_CurrentDirectory = item.Path;
				m_SelectedPath.clear();
				m_NeedsRescan = true;
				break;
			case AssetType::Prefab:
				InstantiatePrefab(item.Path, scene);
				break;
			case AssetType::Scene:
				if (OnOpenScene)
					OnOpenScene(item.Path);
				break;
			default:
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

	void ProjectWindow::CreateFolder()
	{
		std::error_code ec;
		std::filesystem::path path = m_CurrentDirectory / "New Folder";
		for (int i = 2; std::filesystem::exists(path, ec); i++)
			path = m_CurrentDirectory / ("New Folder " + std::to_string(i));

		if (std::filesystem::create_directory(path, ec))
			m_SelectedPath = path;
		else
			HY_ERROR("Could not create folder {}: {}", path.u8string(), ec.message());

		m_NeedsRescan = true;
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

	ProjectWindow::AssetType ProjectWindow::GetAssetType(const std::filesystem::path& path)
	{
		std::error_code ec;
		if (std::filesystem::is_directory(path, ec))
			return AssetType::Folder;

		const std::string extension = ToLower(path.extension().u8string());
		if (extension == PrefabSerializer::FileExtension)
			return AssetType::Prefab;
		if (extension == SceneSerializer::FileExtension)
			return AssetType::Scene;

		return AssetType::Other;
	}
}
