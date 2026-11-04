#include "TerrainTool.h"
#include "PropertyUI.h"
#include "Terrain/TerrainSettingsPanel.h"

#include "EditorCommands/EditorCommand.h"
#include "EditorCommands/TerrainStrokeCommand.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Renderer/TerrainCache.h>
#include <Hydro/Terrain/TerrainSerializer.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>

namespace Hydro
{
	static const char* BrushName(TerrainBrush brush)
	{
		switch (brush)
		{
			case TerrainBrush::Paint: return "Paint";
			case TerrainBrush::Raise: return "Raise";
			case TerrainBrush::Lower: return "Lower";
			case TerrainBrush::Flatten: return "Flatten";
			case TerrainBrush::Wall: return "Wall";
			case TerrainBrush::Ramp: return "Ramp";
			case TerrainBrush::Path: return "Path";
		}
		return "Paint";
	}

	static std::string LayerName(const TerrainSettings& settings, uint32_t layer)
	{
		return std::to_string(layer + 1) + ": " + TerrainSettingsPanel::GetAssetName(settings.Layers[layer].Material);
	}

	static std::string WallLayerName(const TerrainSettings& settings, uint8_t layer)
	{
		if (layer == TerrainWallHidden)
		{
			return "Hidden (collision only)";
		}
		const AssetHandle material = layer < settings.WallLayers.size() ? settings.WallLayers[layer].Material : AssetHandle(0);
		return "Wall " + std::to_string(layer + 1) + ": " + TerrainSettingsPanel::GetAssetName(material);
	}

	TerrainTool::TerrainTool() = default;
	TerrainTool::~TerrainTool() = default;

	void TerrainTool::DrawInspector(Entity entity, TerrainComponent& component, bool playing)
	{
		// The asset: picked, dropped, or made here.
		Ref<Terrain> terrain = (uint64_t)component.Terrain != 0 ? AssetManager::GetLoadedAsset<Terrain>(component.Terrain)
			: nullptr;
		std::string detail;
		if (terrain != nullptr)
		{
			const glm::vec2 size = terrain->GetSize();
			char text[96];
			std::snprintf(text, sizeof(text), "%.1f x %.1f m, %u x %u corners", size.x, size.y, terrain->GetCornerCountX(),
				terrain->GetCornerCountY());
			detail = text;
		}
		PropertyUI::Row("Terrain", [&]()
		{
			const AssetPickerResult result = AssetPicker::DrawField("##TerrainAsset", component.Terrain, AssetType::Terrain, detail);
			if (result.Changed)
			{
				// Only a terrain that loads is kept.
				const Ref<Terrain> picked = (uint64_t)result.Handle != 0 ? AssetManager::GetAsset<Terrain>(result.Handle)
					: nullptr;
				if ((uint64_t)result.Handle == 0 || picked != nullptr)
				{
					component.Terrain = result.Handle;
				}
			}
		});

		if (terrain == nullptr)
		{
			if ((uint64_t)component.Terrain != 0)
			{
				ImGui::TextDisabled("The terrain asset is missing or can't be read (see the Console).");
				return;
			}
			ImGui::Spacing();
			PropertyUI::Row("Size", [&]()
			{
				float values[2] = { m_NewTerrainSize.x, m_NewTerrainSize.y };
				if (ImGui::InputFloat2("##NewTerrainSize", values, "%.0f m"))
				{
					m_NewTerrainSize = glm::clamp(glm::vec2(values[0], values[1]), glm::vec2(1.0f), glm::vec2(1024.0f));
				}
			});
			if (ImGui::Button("Create Terrain", ImVec2(-1.0f, 0.0f)))
			{
				CreateTerrainAsset(entity, component);
			}
			ImGui::TextDisabled("Makes Assets/Terrains/<entity name>.hyterrain: flat, 0.5 m cells,\nits corner (0, 0) at the entity's position.");
			return;
		}

		if (playing)
		{
			ImGui::TextDisabled("Stop playing to edit the terrain.");
			return;
		}

		// What the mesher found wrong (an invalid ramp run).
		const TerrainRenderData* data = TerrainCache::Prepare(terrain);
		if (data != nullptr)
		{
			for (const std::string& problem : data->Problems)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "%s", problem.c_str());
			}
		}

		DrawBrush(terrain);
		TerrainSettingsPanel::Draw(terrain);
	}

	void TerrainTool::DrawBrush(const Ref<Terrain>& terrain)
	{
		TerrainBrushSettings& brush = m_BrushSettings;
		const TerrainSettings& settings = terrain->GetSettings();
		TerrainSettingsPanel::DrawSection("Brush");
		ImGui::Checkbox("Edit in the Editor view", &m_Editing);
		const TerrainBrush brushes[] = { TerrainBrush::Paint, TerrainBrush::Raise, TerrainBrush::Lower, TerrainBrush::Flatten,
			TerrainBrush::Wall, TerrainBrush::Ramp, TerrainBrush::Path };
		for (size_t i = 0; i < sizeof(brushes) / sizeof(brushes[0]); i++)
		{
			if (i != 0 && i != 4)
			{
				ImGui::SameLine();
			}
			if (ImGui::RadioButton(BrushName(brushes[i]), m_Brush == brushes[i]))
			{
				m_Brush = brushes[i];
				m_Editing = true;
			}
		}

		const uint32_t layerCount = (uint32_t)settings.Layers.size();
		brush.Layer = std::min(brush.Layer, layerCount - 1);
		if (m_Brush == TerrainBrush::Paint || m_Brush == TerrainBrush::Path)
		{
			PropertyUI::Row("Layer", [&]()
			{
				const std::string current = LayerName(settings, brush.Layer);
				if (ImGui::BeginCombo("##BrushLayer", current.c_str()))
				{
					for (uint32_t i = 0; i < layerCount; i++)
					{
						const std::string name = LayerName(settings, i);
						if (ImGui::Selectable(name.c_str(), i == brush.Layer))
						{
							brush.Layer = i;
						}
					}
					ImGui::EndCombo();
				}
			});
		}
		if (m_Brush == TerrainBrush::Wall)
		{
			PropertyUI::Row("Wall Layer", [&]()
			{
				if (ImGui::BeginCombo("##BrushWall", WallLayerName(settings, brush.WallLayer).c_str()))
				{
					for (uint32_t i = 0; i < std::max<uint32_t>((uint32_t)settings.WallLayers.size(), 1u); i++)
					{
						if (ImGui::Selectable(WallLayerName(settings, (uint8_t)i).c_str(), brush.WallLayer == i))
						{
							brush.WallLayer = (uint8_t)i;
						}
					}
					if (ImGui::Selectable(WallLayerName(settings, TerrainWallHidden).c_str(), brush.WallLayer == TerrainWallHidden))
					{
						brush.WallLayer = TerrainWallHidden;
					}
					ImGui::EndCombo();
				}
			});
		}
		if (m_Brush != TerrainBrush::Path)
		{
			PropertyUI::Row("Radius", [&]() { PropertyUI::InputFloat("##BrushRadius", brush.Radius, "%.2f m", 0.1f, 50.0f); });
		}
		if (m_Brush == TerrainBrush::Paint)
		{
			PropertyUI::Row("Strength", [&]() { PropertyUI::InputFloat("##BrushStrength", brush.Strength, "%.2f", 0.01f, 1.0f); });
			PropertyUI::Row("Hardness", [&]() { PropertyUI::InputFloat("##BrushHardness", brush.Hardness, "%.2f", 0.0f, 1.0f); });
		}
		if (m_Brush == TerrainBrush::Path)
		{
			PropertyUI::Row("Width", [&]() { PropertyUI::InputFloat("##PathWidth", brush.PathWidth, "%.2f m", 0.2f, 20.0f); });
			PropertyUI::Row("Wander", [&]() { PropertyUI::InputFloat("##PathWander", brush.PathWander, "%.2f m", 0.0f, 2.0f); });
			ImGui::Text("%u point(s)", (uint32_t)m_PathPoints.size());
		}

		switch (m_Brush)
		{
			case TerrainBrush::Paint:
				ImGui::TextDisabled("Left-drag paints the layer, Shift+left-drag removes it.");
				break;
			case TerrainBrush::Raise:
			case TerrainBrush::Lower:
				ImGui::TextDisabled("Left-click or drag: the corners under the brush move one\nlevel (%.2f m), once per stroke. Shift does the opposite.\nClick again for the next level.",
					settings.StepHeight);
				break;
			case TerrainBrush::Flatten:
				ImGui::TextDisabled("Left-drag: the corners under the brush take the level\nwhere the drag started.");
				break;
			case TerrainBrush::Wall:
				ImGui::TextDisabled("Left-drag: the walls of the cells under the brush use the\nwall layer. Hidden walls collide but aren't drawn (a model\nstands there instead).");
				break;
			case TerrainBrush::Ramp:
				ImGui::TextDisabled("Raise the upper shelf first. Then left-drag from the low\nground towards the shelf over the cells that should slope:\none cell per level is steep, two are gentle. Shift+drag\nclears ramps. A run's low and high edges must each be\nat one level.");
				break;
			case TerrainBrush::Path:
				ImGui::TextDisabled("Left-click to add points, Backspace removes the last one.\nEnter paints the layer along the curve through them,\nEscape clears them.");
				break;
		}
		ImGui::TextDisabled("[ and ] change the radius. Ctrl+Z / Ctrl+Y undo and redo.\nWhile editing, clicks don't select: untick Edit to pick\nanother entity in the view.");
	}

	bool TerrainTool::UpdateViewport(Entity selected, const EditorCamera& camera, const glm::vec2& imageMin, const glm::vec2& imageSize,
		bool hovered, bool playing, float deltaTime, EditorCommandStack& commands)
	{
		m_HasHit = false;
		const bool hasTerrain = m_Editing && playing == false && selected.IsValid() && selected.HasComponent<TerrainComponent>()
			&& selected.HasComponent<WorldTransformComponent>();
		const AssetHandle handle = hasTerrain ? selected.GetComponent<TerrainComponent>().Terrain : AssetHandle(0);
		const Ref<Terrain> terrain = (uint64_t)handle != 0 ? AssetManager::GetLoadedAsset<Terrain>(handle) : nullptr;
		if (terrain == nullptr || (m_Stroking && handle != m_StrokeTerrain))
		{
			EndStroke(commands);
			return terrain != nullptr;
		}
		if (handle != m_PathTerrain)
		{
			m_PathPoints.clear();
			m_PathTerrain = handle;
		}

		const ImGuiIO& io = ImGui::GetIO();
		if (hovered && io.WantTextInput == false)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket))
			{
				m_BrushSettings.Radius = std::max(0.1f, m_BrushSettings.Radius / 1.25f);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_RightBracket))
			{
				m_BrushSettings.Radius = std::min(50.0f, m_BrushSettings.Radius * 1.25f);
			}
			if (m_Brush == TerrainBrush::Path)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
				{
					ApplyPath(terrain, handle, commands);
				}
				if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && m_PathPoints.empty() == false)
				{
					m_PathPoints.pop_back();
				}
				if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					m_PathPoints.clear();
				}
			}
		}

		// A ray from the camera through the cursor, unprojected at the near and far planes (zero-to-one depth; the
		// projection's Y flip puts the image's top at -1).
		const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
		const glm::vec2 uv = (mouse - imageMin) / glm::max(imageSize, glm::vec2(1.0f));
		m_Origin = selected.GetComponent<WorldTransformComponent>().Translation;
		if ((hovered || m_Stroking) && uv.x >= 0.0f && uv.y >= 0.0f && uv.x <= 1.0f && uv.y <= 1.0f)
		{
			const glm::mat4 inverse = glm::inverse(camera.GetViewProjection());
			const glm::vec2 ndc = uv * 2.0f - 1.0f;
			glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
			glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
			nearPoint /= nearPoint.w;
			farPoint /= farPoint.w;
			const glm::vec3 start = glm::vec3(nearPoint) - m_Origin;
			const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
			float distance = 0.0f;
			if (TerrainCache::Raycast(terrain, start, direction, distance))
			{
				m_HasHit = true;
				m_HitWorld = start + direction * distance + m_Origin;
			}
		}

		// The left button without Alt (Alt+drag orbits the camera), not while flying (right button).
		m_Shift = io.KeyShift;
		const bool button = ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyAlt == false && camera.IsFlying() == false;
		const glm::vec2 centre = glm::vec2(m_HitWorld - m_Origin);

		if (m_Brush == TerrainBrush::Path)
		{
			if (hovered && m_HasHit && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt == false && camera.IsFlying() == false)
			{
				m_PathPoints.push_back(m_HitWorld - m_Origin);
			}
			return true;
		}

		if (button && m_HasHit && (hovered || m_Stroking))
		{
			if (m_Stroking == false)
			{
				BeginStroke(terrain, handle);
				m_StrokeStart = centre;
				// Flatten: the level of the corner nearest the first click.
				const float cell = terrain->GetSettings().CellSize;
				const uint32_t cx = (uint32_t)glm::clamp((int)std::lround(centre.x / cell), 0, (int)terrain->GetCornerCountX() - 1);
				const uint32_t cy = (uint32_t)glm::clamp((int)std::lround(centre.y / cell), 0, (int)terrain->GetCornerCountY() - 1);
				m_FlattenLevel = terrain->GetLevel(cx, cy);
			}
			switch (m_StrokeBrush)
			{
				case TerrainBrush::Paint:
					TerrainBrushes::Paint(*terrain, m_BrushSettings, m_StrokeWeights, centre, m_Shift, deltaTime);
					break;
				case TerrainBrush::Raise:
				case TerrainBrush::Lower:
				case TerrainBrush::Flatten:
				{
					// Raise goes up a level and Lower down; Shift does the opposite.
					const int32_t step = (m_StrokeBrush == TerrainBrush::Raise) == m_Shift ? -1 : 1;
					const bool flatten = m_StrokeBrush == TerrainBrush::Flatten;
					TerrainBrushes::ChangeLevels(*terrain, m_BrushSettings, m_StrokeTouched, centre, flatten ? 0 : step, flatten,
						m_FlattenLevel);
					break;
				}
				case TerrainBrush::Wall: TerrainBrushes::PaintWalls(*terrain, m_BrushSettings, centre); break;
				case TerrainBrush::Ramp: TerrainBrushes::PaintRamps(*terrain, m_BrushSettings, m_StrokeStart, centre, m_Shift); break;
				case TerrainBrush::Path: break;
			}
		}
		else if (button == false && m_Stroking)
		{
			EndStroke(commands);
		}
		return true;
	}

	void TerrainTool::BeginStroke(const Ref<Terrain>& terrain, AssetHandle handle)
	{
		m_Stroking = true;
		m_StrokeTerrain = handle;
		m_StrokeBrush = m_Brush;
		m_StrokeBefore = CreateScope<TerrainSnapshot>(TerrainSnapshot::Take(*terrain));
		if (m_Brush == TerrainBrush::Paint)
		{
			const std::vector<uint8_t>& weights = terrain->GetWeights();
			m_StrokeWeights.resize(weights.size());
			for (size_t i = 0; i < weights.size(); i++)
			{
				m_StrokeWeights[i] = (float)weights[i] / 255.0f;
			}
		}
		if (m_Brush == TerrainBrush::Raise || m_Brush == TerrainBrush::Lower)
		{
			m_StrokeTouched.assign(terrain->GetLevels().size(), 0);
		}
	}

	void TerrainTool::ApplyPath(const Ref<Terrain>& terrain, AssetHandle handle, EditorCommandStack& commands)
	{
		if (m_PathPoints.size() < 2)
		{
			return;
		}

		BeginStroke(terrain, handle);
		TerrainBrushes::PaintPath(*terrain, m_BrushSettings, m_PathPoints);
		m_StrokeBrush = TerrainBrush::Path;
		EndStroke(commands);
		m_PathPoints.clear();
	}

	void TerrainTool::EndStroke(EditorCommandStack& commands)
	{
		if (m_Stroking == false)
		{
			return;
		}
		m_Stroking = false;

		const Ref<Terrain> terrain = AssetManager::GetLoadedAsset<Terrain>(m_StrokeTerrain);
		if (terrain != nullptr && m_StrokeBefore != nullptr)
		{
			const std::string name = std::string(BrushName(m_StrokeBrush)) + " Terrain";
			Scope<TerrainStrokeCommand> command = CreateScope<TerrainStrokeCommand>(m_StrokeTerrain, name, *m_StrokeBefore, *terrain);
			if (command->IsEmpty() == false)
			{
				commands.Push(std::move(command));
			}
		}

		m_StrokeBefore.reset();
		m_StrokeWeights.clear();
		m_StrokeWeights.shrink_to_fit();
		m_StrokeTouched.clear();
		m_StrokeTouched.shrink_to_fit();
	}

	void TerrainTool::FillOverlay(SceneRenderer::EditorOverlay& overlay) const
	{
		if (m_Editing == false)
		{
			return;
		}
		if (m_Brush == TerrainBrush::Path)
		{
			for (const glm::vec3& point : m_PathPoints)
			{
				overlay.TerrainPath.push_back(point + m_Origin);
			}
			if (m_HasHit)
			{
				overlay.TerrainPath.push_back(m_HitWorld);
			}
			return;
		}
		if (m_HasHit == false)
		{
			return;
		}
		overlay.TerrainBrush = true;
		overlay.TerrainBrushCentre = m_HitWorld;
		overlay.TerrainBrushRadius = m_BrushSettings.Radius;
		// The opposite action (Shift) shows in a warm red, the brush in white (drawn over the scene, any theme).
		overlay.TerrainBrushColor = m_Shift ? glm::vec4(1.0f, 0.45f, 0.35f, 0.95f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.9f);
	}

	void TerrainTool::CreateTerrainAsset(Entity entity, TerrainComponent& component)
	{
		if (AssetManager::IsProjectOpen() == false)
		{
			HY_CORE_WARN("Terrain tool: open a project first; terrains are saved in its Assets/Terrains folder");
			return;
		}

		const std::filesystem::path folder = AssetManager::GetAssetDirectory() / "Terrains";
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		std::string name = entity.HasComponent<TagComponent>() ? entity.GetComponent<TagComponent>().Tag : std::string("Terrain");
		for (char& c : name)
		{
			if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			{
				c = '_';
			}
		}
		if (name.empty())
		{
			name = "Terrain";
		}
		std::filesystem::path path = folder / std::filesystem::u8path(name + TerrainSerializer::FileExtension);
		for (int number = 2; std::filesystem::exists(path, error); number++)
		{
			path = folder / std::filesystem::u8path(name + " " + std::to_string(number) + TerrainSerializer::FileExtension);
		}

		const TerrainSettings settings = Terrain::GetDefaultSettings();
		const glm::uvec2 cells = glm::uvec2(glm::max(glm::round(m_NewTerrainSize / settings.CellSize), glm::vec2(1.0f)));
		const Terrain terrain(settings, cells.x + 1, cells.y + 1);
		if (TerrainSerializer::Serialize(path, terrain) == false)
		{
			return;
		}
		const AssetHandle handle = AssetManager::FindHandle(path);
		if ((uint64_t)handle != 0 && AssetManager::GetAsset<Terrain>(handle) != nullptr)
		{
			component.Terrain = handle;
			m_Editing = true;
			m_Brush = TerrainBrush::Paint;
			HY_CORE_INFO("Terrain tool: made '{}' ({} x {} m)", path.filename().u8string(), m_NewTerrainSize.x, m_NewTerrainSize.y);
		}
	}

	void TerrainTool::SaveModifiedTerrains(Scene& scene)
	{
		std::set<uint64_t> handles;
		auto view = scene.m_Registry.view<TerrainComponent>();
		for (auto entity : view)
		{
			handles.insert((uint64_t)view.get<TerrainComponent>(entity).Terrain);
		}

		for (uint64_t value : handles)
		{
			const AssetHandle handle(value);
			const Ref<Terrain> terrain = value != 0 ? AssetManager::GetLoadedAsset<Terrain>(handle) : nullptr;
			if (terrain == nullptr || terrain->IsModified() == false)
			{
				continue;
			}
			const std::filesystem::path path = AssetManager::GetAbsolutePath(handle);
			if (path.empty() == false && TerrainSerializer::Serialize(path, *terrain))
			{
				terrain->SetModified(false);
			}
		}
	}
}
