#include "TerrainTool.h"
#include "PropertyUI.h"

#include "EditorCommands/TerrainStrokeCommand.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Renderer/Material.h>
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
	// A section title: a separator and a label (the vendored ImGui, 1.89 WIP, has no SeparatorText).
	static void DrawSection(const char* title)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("%s", title);
	}

	// The file name of an asset ("FarmDirt"), or "none".
	static std::string GetAssetName(AssetHandle handle)
	{
		if ((uint64_t)handle == 0)
		{
			return "none";
		}
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		return metadata != nullptr ? metadata->Path.stem().u8string() : "missing";
	}

	void TerrainTool::DrawInspector(Entity entity, TerrainComponent& component, bool playing)
	{
		// The asset: picked, dropped, or made here.
		Ref<Terrain> terrain = (uint64_t)component.Terrain != 0 ? AssetManager::GetLoadedAsset<Terrain>(component.Terrain) : nullptr;
		std::string detail;
		if (terrain != nullptr)
		{
			const glm::vec2 size = terrain->GetSize();
			char text[96];
			std::snprintf(text, sizeof(text), "%.1f x %.1f m, %u x %u corners", size.x, size.y, terrain->GetCornerCountX(), terrain->GetCornerCountY());
			detail = text;
		}
		PropertyUI::Row("Terrain", [&]()
		{
			const AssetPickerResult result = AssetPicker::DrawField("##TerrainAsset", component.Terrain, AssetType::Terrain, detail);
			if (result.Changed)
			{
				// Only a terrain that loads is kept; its layers' materials are loaded with it.
				const Ref<Terrain> picked = (uint64_t)result.Handle != 0 ? AssetManager::GetAsset<Terrain>(result.Handle) : nullptr;
				if ((uint64_t)result.Handle == 0 || picked != nullptr)
				{
					component.Terrain = result.Handle;
					if (picked != nullptr)
					{
						for (const TerrainLayer& layer : picked->GetSettings().Layers)
						{
							if ((uint64_t)layer.Material != 0)
							{
								AssetManager::GetAsset<MaterialAsset>(layer.Material);
							}
						}
					}
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

		// The terrain's own settings: edited on a copy, applied when something changed (saved with the scene).
		TerrainSettings settings = terrain->GetSettings();
		DrawSection("Layers");
		for (uint32_t i = 0; i < (uint32_t)settings.Layers.size(); i++)
		{
			ImGui::PushID((int)i);
			TerrainLayer& layer = settings.Layers[i];
			char label[32];
			std::snprintf(label, sizeof(label), "Layer %u", i + 1);
			PropertyUI::Row(label, [&]()
			{
				const AssetPickerResult result = AssetPicker::DrawField("##LayerMaterial", layer.Material, AssetType::Material);
				if (result.Changed && ((uint64_t)result.Handle == 0 || AssetManager::GetAsset<MaterialAsset>(result.Handle) != nullptr))
				{
					layer.Material = result.Handle;
				}
			});
			PropertyUI::Row("Edge Roughness", [&]()
			{
				PropertyUI::InputFloat("##EdgeRoughness", layer.EdgeRoughness, "%.2f", 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How ragged this layer's border with the others is: 0 a clean painted edge, 1 very ragged.");
				}
			});
			ImGui::PopID();
		}
		if (settings.Layers.size() < MaxTerrainLayers && ImGui::Button("Add Layer"))
		{
			settings.Layers.push_back(TerrainLayer{});
		}
		if (settings.Layers.size() > 1)
		{
			if (settings.Layers.size() < MaxTerrainLayers)
			{
				ImGui::SameLine();
			}
			if (ImGui::Button("Remove Last Layer"))
			{
				settings.Layers.pop_back();
			}
		}

		DrawSection("Edge Noise");
		PropertyUI::Row("Cluster Size", [&]() { PropertyUI::InputFloat("##EdgeClusterSize", settings.EdgeClusterSize, "%.1f px", 0.5f, 16.0f); });
		PropertyUI::Row("Pixel Noise", [&]() { PropertyUI::InputFloat("##EdgePixelNoise", settings.EdgePixelNoise, "%.2f", 0.0f, 1.0f); });
		PropertyUI::Row("Seed", [&]() { PropertyUI::InputUInt("##TerrainSeed", settings.Seed, 0, 0x7fffffffu); });

		if (settings != terrain->GetSettings())
		{
			terrain->SetSettings(settings);
		}

		// The brush.
		DrawSection("Paint");
		ImGui::Checkbox("Paint in the Editor view", &m_Painting);
		const uint32_t layerCount = (uint32_t)terrain->GetSettings().Layers.size();
		m_Layer = std::min(m_Layer, layerCount - 1);
		PropertyUI::Row("Brush Layer", [&]()
		{
			const std::string current = std::to_string(m_Layer + 1) + ": " + GetAssetName(terrain->GetSettings().Layers[m_Layer].Material);
			if (ImGui::BeginCombo("##BrushLayer", current.c_str()))
			{
				for (uint32_t i = 0; i < layerCount; i++)
				{
					const std::string name = std::to_string(i + 1) + ": " + GetAssetName(terrain->GetSettings().Layers[i].Material);
					if (ImGui::Selectable(name.c_str(), i == m_Layer))
					{
						m_Layer = i;
					}
				}
				ImGui::EndCombo();
			}
		});
		PropertyUI::Row("Radius", [&]() { PropertyUI::InputFloat("##BrushRadius", m_Radius, "%.2f m", 0.1f, 50.0f); });
		PropertyUI::Row("Strength", [&]() { PropertyUI::InputFloat("##BrushStrength", m_Strength, "%.2f", 0.01f, 1.0f); });
		PropertyUI::Row("Hardness", [&]() { PropertyUI::InputFloat("##BrushHardness", m_Hardness, "%.2f", 0.0f, 1.0f); });
		ImGui::TextDisabled("Left-drag paints the layer, Shift+left-drag removes it.\n[ and ] change the radius. Ctrl+Z / Ctrl+Y undo and redo.\nWhile painting, clicks don't select: untick Paint to pick\nanother entity in the view.");
	}

	bool TerrainTool::UpdateViewport(Entity selected, const EditorCamera& camera, const glm::vec2& imageMin, const glm::vec2& imageSize, bool hovered,
		bool playing, float deltaTime, EditorCommandStack& commands)
	{
		m_HasHit = false;
		const bool hasTerrain = m_Painting && playing == false && selected.IsValid() && selected.HasComponent<TerrainComponent>()
			&& selected.HasComponent<WorldTransformComponent>();
		const AssetHandle handle = hasTerrain ? selected.GetComponent<TerrainComponent>().Terrain : AssetHandle(0);
		const Ref<Terrain> terrain = (uint64_t)handle != 0 ? AssetManager::GetLoadedAsset<Terrain>(handle) : nullptr;
		if (terrain == nullptr || (m_Stroking && handle != m_StrokeTerrain))
		{
			EndStroke(commands);
			return terrain != nullptr;
		}

		const ImGuiIO& io = ImGui::GetIO();
		if (hovered && io.WantTextInput == false)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket))
			{
				m_Radius = std::max(0.1f, m_Radius / 1.25f);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_RightBracket))
			{
				m_Radius = std::min(50.0f, m_Radius * 1.25f);
			}
		}

		// A ray from the camera through the cursor: unproject the cursor at the near and far planes (zero-to-one depth,
		// the projection's Y flip puts the image's top at -1).
		const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
		const glm::vec2 uv = (mouse - imageMin) / glm::max(imageSize, glm::vec2(1.0f));
		const glm::vec3 origin = selected.GetComponent<WorldTransformComponent>().Translation;
		if ((hovered || m_Stroking) && uv.x >= 0.0f && uv.y >= 0.0f && uv.x <= 1.0f && uv.y <= 1.0f)
		{
			const glm::mat4 inverse = glm::inverse(camera.GetViewProjection());
			const glm::vec2 ndc = uv * 2.0f - 1.0f;
			glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
			glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
			nearPoint /= nearPoint.w;
			farPoint /= farPoint.w;
			const glm::vec3 start = glm::vec3(nearPoint) - origin;
			const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
			float distance = 0.0f;
			if (TerrainCache::Raycast(terrain, start, direction, distance))
			{
				m_HasHit = true;
				m_HitWorld = start + direction * distance + origin;
			}
		}

		// Painting: the left button without Alt (Alt+drag orbits the camera), not while flying (right button).
		m_Lowering = io.KeyShift;
		const bool paintButton = ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyAlt == false && camera.IsFlying() == false;
		if (paintButton && m_HasHit && (hovered || m_Stroking))
		{
			if (m_Stroking == false)
			{
				BeginStroke(terrain, handle);
			}
			Paint(*terrain, glm::vec2(m_HitWorld - origin), m_Lowering, deltaTime);
		}
		else if (paintButton == false && m_Stroking)
		{
			EndStroke(commands);
		}
		return true;
	}

	void TerrainTool::BeginStroke(const Ref<Terrain>& terrain, AssetHandle handle)
	{
		m_Stroking = true;
		m_StrokeChanged = false;
		m_StrokeTerrain = handle;
		m_StrokeBefore = terrain->GetWeights();
		m_StrokeWeights.resize(m_StrokeBefore.size());
		for (size_t i = 0; i < m_StrokeBefore.size(); i++)
		{
			m_StrokeWeights[i] = (float)m_StrokeBefore[i] / 255.0f;
		}
	}

	void TerrainTool::Paint(Terrain& terrain, const glm::vec2& centre, bool lower, float deltaTime)
	{
		const TerrainSettings& settings = terrain.GetSettings();
		const uint32_t layerCount = std::min((uint32_t)settings.Layers.size(), MaxTerrainLayers);
		const uint32_t layer = std::min(m_Layer, layerCount - 1);
		if (lower && layerCount < 2)
		{
			return; // the only layer can't give way to another
		}

		// The texels whose centres ((k + 0.5) / SplatResolution metres) are inside the circle.
		const float resolution = (float)settings.SplatResolution;
		const int width = (int)terrain.GetWeightMapWidth();
		const int height = (int)terrain.GetWeightMapHeight();
		const int minX = std::max(0, (int)std::floor((centre.x - m_Radius) * resolution));
		const int minY = std::max(0, (int)std::floor((centre.y - m_Radius) * resolution));
		const int maxX = std::min(width - 1, (int)std::ceil((centre.x + m_Radius) * resolution));
		const int maxY = std::min(height - 1, (int)std::ceil((centre.y + m_Radius) * resolution));
		if (minX > maxX || minY > maxY)
		{
			return;
		}

		const float inner = m_Radius * glm::clamp(m_Hardness, 0.0f, 0.99f);
		// Full strength reaches the target weight in about a quarter of a second.
		const float rate = glm::clamp(m_Strength, 0.0f, 1.0f) * 4.0f * deltaTime;
		bool changed = false;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				const glm::vec2 texelCentre((x + 0.5f) / resolution, (y + 0.5f) / resolution);
				const float distance = glm::distance(texelCentre, centre);
				if (distance > m_Radius)
				{
					continue;
				}
				const float falloff = distance <= inner ? 1.0f : 1.0f - glm::smoothstep(inner, m_Radius, distance);
				const float amount = glm::clamp(rate * falloff, 0.0f, 1.0f);
				if (amount <= 0.0f)
				{
					continue;
				}

				float* w = &m_StrokeWeights[((size_t)y * width + x) * 4];
				const float old = w[layer];
				float others = 0.0f;
				for (uint32_t k = 0; k < layerCount; k++)
				{
					others += k == layer ? 0.0f : w[k];
				}
				if (lower == false)
				{
					// Raise the layer towards 1; the others shrink in proportion.
					const float raised = old + amount * (1.0f - old);
					const float scale = others > 1e-6f ? (1.0f - raised) / others : 0.0f;
					for (uint32_t k = 0; k < layerCount; k++)
					{
						w[k] = k == layer ? raised : w[k] * scale;
					}
				}
				else
				{
					// Lower the layer; what it gives up goes to the others in proportion (to the first other layer when
					// they are all empty).
					const float lowered = old * (1.0f - amount);
					const float freed = old - lowered;
					w[layer] = lowered;
					if (others > 1e-6f)
					{
						for (uint32_t k = 0; k < layerCount; k++)
						{
							w[k] += k == layer ? 0.0f : w[k] / others * freed;
						}
					}
					else
					{
						w[layer == 0 ? 1 : 0] += freed;
					}
				}
				for (uint32_t k = layerCount; k < 4; k++)
				{
					w[k] = 0.0f;
				}

				// To bytes that sum to exactly 255: the rounding error goes to the largest channel.
				int bytes[4] = { 0, 0, 0, 0 };
				int sum = 0;
				int largest = 0;
				for (int k = 0; k < 4; k++)
				{
					bytes[k] = (int)std::lround(glm::clamp(w[k], 0.0f, 1.0f) * 255.0f);
					sum += bytes[k];
					largest = bytes[k] > bytes[largest] ? k : largest;
				}
				bytes[largest] = glm::clamp(bytes[largest] + 255 - sum, 0, 255);
				const glm::u8vec4 value((uint8_t)bytes[0], (uint8_t)bytes[1], (uint8_t)bytes[2], (uint8_t)bytes[3]);
				if (value != terrain.GetWeight((uint32_t)x, (uint32_t)y))
				{
					terrain.SetWeight((uint32_t)x, (uint32_t)y, value);
					changed = true;
				}
			}
		}

		if (changed)
		{
			const TerrainRect dab{ glm::uvec2((uint32_t)minX, (uint32_t)minY), glm::uvec2((uint32_t)maxX, (uint32_t)maxY) };
			terrain.MarkWeightsChanged(dab);
			if (m_StrokeChanged)
			{
				m_StrokeTexels.Min = glm::min(m_StrokeTexels.Min, dab.Min);
				m_StrokeTexels.Max = glm::max(m_StrokeTexels.Max, dab.Max);
			}
			else
			{
				m_StrokeTexels = dab;
				m_StrokeChanged = true;
			}
		}
	}

	void TerrainTool::EndStroke(EditorCommandStack& commands)
	{
		if (m_Stroking == false)
		{
			return;
		}
		m_Stroking = false;

		const Ref<Terrain> terrain = AssetManager::GetLoadedAsset<Terrain>(m_StrokeTerrain);
		if (m_StrokeChanged && terrain != nullptr && terrain->GetWeights().size() == m_StrokeBefore.size())
		{
			const uint32_t width = m_StrokeTexels.Max.x - m_StrokeTexels.Min.x + 1;
			const uint32_t height = m_StrokeTexels.Max.y - m_StrokeTexels.Min.y + 1;
			const size_t stride = (size_t)terrain->GetWeightMapWidth() * 4;
			std::vector<uint8_t> before((size_t)width * height * 4);
			std::vector<uint8_t> after(before.size());
			for (uint32_t y = 0; y < height; y++)
			{
				const size_t from = (size_t)(m_StrokeTexels.Min.y + y) * stride + (size_t)m_StrokeTexels.Min.x * 4;
				std::copy(m_StrokeBefore.begin() + from, m_StrokeBefore.begin() + from + (size_t)width * 4, before.begin() + (size_t)y * width * 4);
				std::copy(terrain->GetWeights().begin() + from, terrain->GetWeights().begin() + from + (size_t)width * 4, after.begin() + (size_t)y * width * 4);
			}
			commands.Push(CreateScope<TerrainStrokeCommand>(m_StrokeTerrain, m_StrokeTexels, std::move(before), std::move(after)));
		}

		m_StrokeChanged = false;
		m_StrokeBefore.clear();
		m_StrokeBefore.shrink_to_fit();
		m_StrokeWeights.clear();
		m_StrokeWeights.shrink_to_fit();
	}

	void TerrainTool::FillOverlay(SceneRenderer::EditorOverlay& overlay) const
	{
		if (m_Painting == false || m_HasHit == false)
		{
			return;
		}
		overlay.TerrainBrush = true;
		overlay.TerrainBrushCentre = m_HitWorld;
		overlay.TerrainBrushRadius = m_Radius;
		// Lowering shows in a warm red, painting in white (drawn over the scene, any theme).
		overlay.TerrainBrushColor = m_Lowering ? glm::vec4(1.0f, 0.45f, 0.35f, 0.95f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.9f);
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
			m_Painting = true;
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
