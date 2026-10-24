#include "SpriteEditorWindow.h"
#include "PropertyUI.h"
#include "Utilities/PathFollower.h"

#include <imgui/imgui.h>

#include <Hydro/Core/Log.h>

#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Renderer/SpriteSlicer.h>
#include <Hydro/Renderer/TextureImportSerializer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace Hydro
{
	struct SpritePivotPreset
	{
		const char* Name;
		glm::vec2 Pivot;
	};

	// Pivot y runs up: 0 is the sprite's bottom edge (where SceneRenderer places the entity).
	static const SpritePivotPreset s_PivotPresets[] =
	{
		{ "Center", { 0.5f, 0.5f } },
		{ "Bottom (feet)", { 0.5f, 0.0f } },
		{ "Top", { 0.5f, 1.0f } },
		{ "Left", { 0.0f, 0.5f } },
		{ "Right", { 1.0f, 0.5f } },
		{ "Bottom Left", { 0.0f, 0.0f } },
		{ "Bottom Right", { 1.0f, 0.0f } },
		{ "Top Left", { 0.0f, 1.0f } },
		{ "Top Right", { 1.0f, 1.0f } },
	};

	// A pivot preset combo plus the two numbers. True when the pivot changed.
	static bool DrawPivotField(const char* id, glm::vec2& pivot)
	{
		ImGui::PushID(id);
		bool changed = false;

		const char* current = "Custom";
		for (const SpritePivotPreset& preset : s_PivotPresets)
		{
			if (preset.Pivot == pivot)
			{
				current = preset.Name;
			}
		}

		const float width = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(width * 0.45f);
		if (ImGui::BeginCombo("##Preset", current))
		{
			for (const SpritePivotPreset& preset : s_PivotPresets)
			{
				if (ImGui::Selectable(preset.Name, preset.Pivot == pivot))
				{
					pivot = preset.Pivot;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputFloat2("##Pivot", &pivot.x, "%.2f"))
		{
			changed = true;
		}

		ImGui::PopID();
		return changed;
	}

	void SpriteEditorWindow::Open(const std::filesystem::path& imagePath)
	{
		m_Open = true;
		ImGui::SetWindowFocus("Sprite Editor");
		if (imagePath == m_Path)
		{
			return;
		}

		if (m_Dirty)
		{
			m_PendingOpen = imagePath;
			m_RequestDiscardPopup = true;
			return;
		}
		Load(imagePath);
	}

	void SpriteEditorWindow::OnAssetMoved(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		PathFollower::Follow(m_Path, from, to);
		PathFollower::Follow(m_PendingOpen, from, to);
	}

	void SpriteEditorWindow::OnAssetDeleted(const std::filesystem::path& path)
	{
		if (PathFollower::Follow(m_Path, path, path) == false)
		{
			return;
		}

		m_Open = false;
		m_Path.clear();
		m_Texture = nullptr;
		m_Config = TextureImportConfig{};
		m_Selected = -1;
		m_Dirty = false;
		m_PendingOpen.clear();
	}

	void SpriteEditorWindow::Load(const std::filesystem::path& imagePath)
	{
		m_Path = imagePath;
		m_Texture = Texture2D::LoadFromFile(imagePath);
		m_Config = m_Texture != nullptr ? m_Texture->GetImportConfig() : TextureImportConfig{};
		// Editing sprites makes it a sheet; a Single texture's one sprite is the whole image.
		if (m_Config.Mode == SpriteMode::Single)
		{
			m_Config.Sprites.clear();
		}
		m_Selected = m_Config.Sprites.empty() ? -1 : 0;
		m_Dirty = false;
		m_Zoom = 0.0f;
		SyncRowNamesFromConfig();
	}

	void SpriteEditorWindow::SyncRowNamesFromConfig()
	{
		std::string text;
		for (const std::string& name : m_Config.Slicing.RowNames)
		{
			text += name;
			text += '\n';
		}
		std::snprintf(m_RowNames, sizeof(m_RowNames), "%s", text.c_str());
	}

	void SpriteEditorWindow::SyncRowNamesToConfig()
	{
		m_Config.Slicing.RowNames.clear();
		std::istringstream lines(m_RowNames);
		std::string line;
		while (std::getline(lines, line))
		{
			// Trim spaces and a trailing '\r'; keep empty lines, they stand for unnamed rows.
			const size_t first = line.find_first_not_of(" \t\r");
			const size_t last = line.find_last_not_of(" \t\r");
			m_Config.Slicing.RowNames.push_back(first == std::string::npos ? std::string() : line.substr(first, last - first + 1));
		}
		// Unnamed rows at the end mean nothing.
		while (m_Config.Slicing.RowNames.empty() == false && m_Config.Slicing.RowNames.back().empty())
		{
			m_Config.Slicing.RowNames.pop_back();
		}
	}

	void SpriteEditorWindow::Slice()
	{
		if (m_Texture == nullptr)
		{
			return;
		}

		SyncRowNamesToConfig();
		std::vector<SpriteRegion> sliced = SpriteSlicer::SliceGrid(m_Texture->GetWidth(), m_Texture->GetHeight(), m_Config.Slicing, m_Path.stem().u8string());

		// A sprite whose rectangle didn't move keeps the pivot it was given.
		for (SpriteRegion& sprite : sliced)
		{
			for (const SpriteRegion& old : m_Config.Sprites)
			{
				if (old.Position == sprite.Position && old.Size == sprite.Size)
				{
					sprite.Pivot = old.Pivot;
					break;
				}
			}
		}

		m_Config.Sprites = std::move(sliced);
		m_Selected = m_Config.Sprites.empty() ? -1 : std::min(std::max(m_Selected, 0), (int32_t)m_Config.Sprites.size() - 1);
		m_Dirty = true;
	}

	void SpriteEditorWindow::Apply()
	{
		SyncRowNamesToConfig();
		m_Config.Mode = SpriteMode::Multiple;
		if (TextureImportSerializer::Serialize(Texture2D::GetImportFilePath(m_Path), m_Config) == false)
		{
			return;
		}

		// Told first, so the texture is reloaded before this window reads it again.
		const std::filesystem::path path = m_Path;
		const int32_t selected = m_Selected;
		const float zoom = m_Zoom;
		if (OnReimported)
		{
			OnReimported(path);
		}
		Load(path);
		m_Selected = std::min(selected, (int32_t)m_Config.Sprites.size() - 1);
		m_Zoom = zoom;
	}

	void SpriteEditorWindow::OnImguiRender()
	{
		DrawDiscardPopup();
		if (m_Open == false)
		{
			return;
		}

		ImGui::SetNextWindowSize({ 960.0f, 640.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Sprite Editor", &m_Open, m_Dirty ? ImGuiWindowFlags_UnsavedDocument : 0) == false)
		{
			ImGui::End();
			return;
		}

		if (m_Path.empty() || m_Texture == nullptr)
		{
			ImGui::TextDisabled(m_Path.empty()
				? "Select an image in the Project window and press Open Sprite Editor, or double-click it."
				: "This image can't be loaded (see the Console).");
			ImGui::End();
			return;
		}

		DrawToolbar();
		ImGui::Separator();

		if (ImGui::BeginTable("##SpriteEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Settings", ImGuiTableColumnFlags_WidthFixed, 320.0f);
			ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (ImGui::BeginChild("##Settings", { 0.0f, 0.0f }, false))
			{
				DrawSlicingSettings();
				ImGui::Spacing();
				DrawSpriteList();
				ImGui::Spacing();
				DrawSelectedSprite();
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(1);
			DrawCanvas();

			ImGui::EndTable();
		}

		ImGui::End();
	}

	void SpriteEditorWindow::DrawToolbar()
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(m_Path.filename().u8string().c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%u x %u px, %zu sprites", m_Texture->GetWidth(), m_Texture->GetHeight(), m_Config.Sprites.size());

		const float buttonWidth = 90.0f;
		const ImGuiStyle& style = ImGui::GetStyle();
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 2.0f * buttonWidth - 3.0f * style.ItemSpacing.x - 150.0f);

		// Zoom: 0 = fit; otherwise screen pixels per image pixel.
		ImGui::SetNextItemWidth(150.0f);
		float zoomPercent = m_Zoom * 100.0f;
		if (ImGui::SliderFloat("##Zoom", &zoomPercent, 0.0f, 800.0f, m_Zoom == 0.0f ? "Zoom: fit" : "Zoom: %.0f%%"))
		{
			m_Zoom = zoomPercent < 5.0f ? 0.0f : zoomPercent / 100.0f;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Drag to the left end to fit the image to the window.");
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(m_Dirty == false);
		if (ImGui::Button("Revert", { buttonWidth, 0.0f }))
		{
			Load(m_Path);
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply", { buttonWidth, 0.0f }))
		{
			Apply();
		}
		ImGui::EndDisabled();
	}

	void SpriteEditorWindow::DrawSlicingSettings()
	{
		ImGui::TextDisabled("Slicing");
		SpriteSlicingConfig& slicing = m_Config.Slicing;

		PropertyUI::Row("Mode", [&]()
		{
			const char* modes[] = { "Manual", "Grid", "Auto (detect grid)" };
			int mode = (int)slicing.Mode;
			if (ImGui::Combo("##Mode", &mode, modes, 3))
			{
				slicing.Mode = (SpriteSlicingMode)mode;
				m_Dirty = true;
			}
		});

		if (slicing.Mode == SpriteSlicingMode::Manual)
		{
			if (ImGui::Button("Add Sprite", { ImGui::GetContentRegionAvail().x, 0.0f }))
			{
				SpriteRegion sprite;
				sprite.Name = m_Path.stem().u8string() + "_" + std::to_string(m_Config.Sprites.size());
				sprite.Size = glm::min(slicing.CellSize, glm::uvec2(m_Texture->GetWidth(), m_Texture->GetHeight()));
				sprite.Pivot = slicing.DefaultPivot;
				m_Config.Sprites.push_back(sprite);
				m_Selected = (int32_t)m_Config.Sprites.size() - 1;
				m_Dirty = true;
			}
			return;
		}

		PropertyUI::Row("Cell Size", [&]() { m_Dirty |= PropertyUI::InputUVec2("##CellSize", slicing.CellSize, 1); });
		PropertyUI::Row("Offset", [&]() { m_Dirty |= PropertyUI::InputUVec2("##Offset", slicing.Offset); });
		PropertyUI::Row("Spacing", [&]() { m_Dirty |= PropertyUI::InputUVec2("##Spacing", slicing.Spacing); });
		PropertyUI::Row("Columns", [&]()
		{
			m_Dirty |= PropertyUI::InputUInt("##Columns", slicing.Columns);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("0 = as many as fit");
			}
		});
		PropertyUI::Row("Rows", [&]()
		{
			m_Dirty |= PropertyUI::InputUInt("##Rows", slicing.Rows);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("0 = as many as fit");
			}
		});
		PropertyUI::Row("Default Pivot", [&]() { m_Dirty |= DrawPivotField("##DefaultPivot", slicing.DefaultPivot); });

		ImGui::TextUnformatted("Row names (one per line)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Sprites are named <row name>_<column>, e.g. Farmer_Left_0.\nRows without a name use <image name>_<index>.\nNamed rows make clips easy to build (Clips from Rows).");
		}
		if (ImGui::InputTextMultiline("##RowNames", m_RowNames, sizeof(m_RowNames), { -1.0f, ImGui::GetTextLineHeight() * 7.0f }))
		{
			m_Dirty = true;
		}

		const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		if (ImGui::Button("Detect Grid", { half, 0.0f }))
		{
			if (SpriteSlicer::DetectGrid(m_Path, m_Config, slicing))
			{
				Slice();
			}
			else
			{
				HY_WARN("Sprite Editor: no grid found in {}", m_Path.filename().u8string());
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Find evenly spaced sprites (by transparency, or the colour key) and slice.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Slice", { half, 0.0f }))
		{
			Slice();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Replace the sprites with the grid's cells. Cells that didn't move keep their pivot.");
		}
	}

	void SpriteEditorWindow::DrawSpriteList()
	{
		ImGui::TextDisabled("Sprites");
		const float height = ImGui::GetTextLineHeightWithSpacing() * 8.0f;
		if (ImGui::BeginListBox("##Sprites", { -1.0f, height }))
		{
			ImGuiListClipper clipper;
			clipper.Begin((int)m_Config.Sprites.size());
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				{
					char label[160];
					std::snprintf(label, sizeof(label), "%d: %s##sprite%d", i, m_Config.Sprites[i].Name.c_str(), i);
					if (ImGui::Selectable(label, m_Selected == i))
					{
						m_Selected = i;
					}
				}
			}
			ImGui::EndListBox();
		}
	}

	void SpriteEditorWindow::DrawSelectedSprite()
	{
		if (m_Selected < 0 || m_Selected >= (int32_t)m_Config.Sprites.size())
		{
			ImGui::TextDisabled("Click a sprite in the image or the list.");
			return;
		}

		SpriteRegion& sprite = m_Config.Sprites[m_Selected];
		ImGui::TextDisabled("Selected sprite");

		PropertyUI::Row("Name", [&]()
		{
			char name[128];
			std::snprintf(name, sizeof(name), "%s", sprite.Name.c_str());
			if (ImGui::InputText("##Name", name, sizeof(name)))
			{
				sprite.Name = name;
				m_Dirty = true;
			}
		});
		PropertyUI::Row("Position", [&]() { m_Dirty |= PropertyUI::InputUVec2("##Position", sprite.Position, 0, m_Texture->GetWidth()); });
		PropertyUI::Row("Size", [&]() { m_Dirty |= PropertyUI::InputUVec2("##Size", sprite.Size, 1, std::max(m_Texture->GetWidth(), m_Texture->GetHeight())); });
		PropertyUI::Row("Pivot", [&]() { m_Dirty |= DrawPivotField("##Pivot", sprite.Pivot); });

		const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		if (ImGui::Button("Pivot to All", { half, 0.0f }))
		{
			const glm::vec2 pivot = sprite.Pivot;
			for (SpriteRegion& other : m_Config.Sprites)
			{
				other.Pivot = pivot;
			}
			m_Dirty = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Give every sprite this pivot.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete Sprite", { half, 0.0f }))
		{
			m_Config.Sprites.erase(m_Config.Sprites.begin() + m_Selected);
			m_Selected = std::min(m_Selected, (int32_t)m_Config.Sprites.size() - 1);
			m_Dirty = true;
		}
	}

	void SpriteEditorWindow::DrawCanvas()
	{
		ImGui::BeginChild("##Canvas", { 0.0f, 0.0f }, false, ImGuiWindowFlags_HorizontalScrollbar);

		const glm::vec2 imageSize = { (float)m_Texture->GetWidth(), (float)m_Texture->GetHeight() };
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float fit = std::max(0.01f, std::min(available.x / imageSize.x, available.y / imageSize.y));
		const float zoom = m_Zoom == 0.0f ? fit : m_Zoom;

		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 drawnSize = { imageSize.x * zoom, imageSize.y * zoom };
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// A frame behind the image so transparent parts are visible against the window.
		drawList->AddRectFilled(origin, { origin.x + drawnSize.x, origin.y + drawnSize.y }, ImGui::GetColorU32(ImGuiCol_FrameBg));
		ImGui::Image((ImTextureID)Renderer2D::GetImGuiTexture(m_Texture), drawnSize);
		const bool imageHovered = ImGui::IsItemHovered();

		const ImU32 outline = ImGui::GetColorU32(ImGuiCol_Text, 0.35f);
		const ImU32 highlight = ImGui::GetColorU32(ImGuiCol_CheckMark);
		const auto toScreen = [&](float x, float y) { return ImVec2{ origin.x + x * zoom, origin.y + y * zoom }; };

		int32_t hoveredSprite = -1;
		const ImVec2 mouse = ImGui::GetMousePos();
		for (int32_t i = 0; i < (int32_t)m_Config.Sprites.size(); i++)
		{
			const SpriteRegion& sprite = m_Config.Sprites[i];
			const ImVec2 min = toScreen((float)sprite.Position.x, (float)sprite.Position.y);
			const ImVec2 max = toScreen((float)(sprite.Position.x + sprite.Size.x), (float)(sprite.Position.y + sprite.Size.y));
			if (i != m_Selected)
			{
				drawList->AddRect(min, max, outline);
			}
			if (imageHovered && mouse.x >= min.x && mouse.x < max.x && mouse.y >= min.y && mouse.y < max.y)
			{
				hoveredSprite = i;
			}
		}

		// The selected sprite on top, with its pivot.
		if (m_Selected >= 0 && m_Selected < (int32_t)m_Config.Sprites.size())
		{
			const SpriteRegion& sprite = m_Config.Sprites[m_Selected];
			const ImVec2 min = toScreen((float)sprite.Position.x, (float)sprite.Position.y);
			const ImVec2 max = toScreen((float)(sprite.Position.x + sprite.Size.x), (float)(sprite.Position.y + sprite.Size.y));
			drawList->AddRect(min, max, highlight, 0.0f, 0, 2.0f);
			const ImVec2 pivot = { min.x + (max.x - min.x) * sprite.Pivot.x, max.y - (max.y - min.y) * sprite.Pivot.y };
			drawList->AddCircleFilled(pivot, 4.0f, highlight);
			drawList->AddCircle(pivot, 6.0f, ImGui::GetColorU32(ImGuiCol_WindowBg), 0, 1.5f);
		}

		if (hoveredSprite >= 0)
		{
			ImGui::SetTooltip("%d: %s", hoveredSprite, m_Config.Sprites[hoveredSprite].Name.c_str());
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				m_Selected = hoveredSprite;
			}
		}

		// Ctrl + wheel zooms.
		if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
		{
			m_Zoom = std::clamp(zoom * (ImGui::GetIO().MouseWheel > 0.0f ? 1.25f : 0.8f), 0.05f, 8.0f);
		}

		ImGui::EndChild();
	}

	void SpriteEditorWindow::DrawDiscardPopup()
	{
		if (m_RequestDiscardPopup)
		{
			ImGui::OpenPopup("Discard sprite changes?");
			m_RequestDiscardPopup = false;
		}

		if (ImGui::BeginPopupModal("Discard sprite changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("'%s' has changes that weren't applied.", m_Path.filename().u8string().c_str());
			ImGui::Spacing();
			if (ImGui::Button("Apply", { 120.0f, 0.0f }))
			{
				Apply();
				Load(m_PendingOpen);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard", { 120.0f, 0.0f }))
			{
				Load(m_PendingOpen);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 120.0f, 0.0f }))
			{
				m_PendingOpen.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}
