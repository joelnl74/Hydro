#include "TerrainTool.h"
#include "PropertyUI.h"

#include "EditorCommands/EditorCommand.h"
#include "EditorCommands/TerrainStrokeCommand.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/TerrainCache.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Terrain/TerrainSerializer.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <cfloat>
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

	static std::string WallLayerName(const TerrainSettings& settings, uint8_t layer)
	{
		if (layer == TerrainWallHidden)
		{
			return "Hidden (collision only)";
		}
		const AssetHandle material = layer < settings.WallLayers.size() ? settings.WallLayers[layer].Material : AssetHandle(0);
		return "Wall " + std::to_string(layer + 1) + ": " + GetAssetName(material);
	}

	// Raises `layer` of the weights `w` to at least `target` (0..1); the other layers shrink in proportion.
	static void RaiseLayerTo(float* w, uint32_t layer, uint32_t layerCount, float target)
	{
		if (w[layer] >= target)
		{
			return;
		}
		float others = 0.0f;
		for (uint32_t k = 0; k < layerCount; k++)
		{
			others += k == layer ? 0.0f : w[k];
		}
		const float scale = others > 1e-6f ? (1.0f - target) / others : 0.0f;
		for (uint32_t k = 0; k < layerCount; k++)
		{
			w[k] = k == layer ? target : w[k] * scale;
		}
	}

	// Weights 0..1 to bytes that sum to exactly 255: the rounding error goes to the largest channel.
	static glm::u8vec4 WeightsToBytes(const float* w)
	{
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
		return glm::u8vec4((uint8_t)bytes[0], (uint8_t)bytes[1], (uint8_t)bytes[2], (uint8_t)bytes[3]);
	}

	TerrainTool::TerrainTool() = default;
	TerrainTool::~TerrainTool() = default;

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
				// Only a terrain that loads is kept.
				const Ref<Terrain> picked = (uint64_t)result.Handle != 0 ? AssetManager::GetAsset<Terrain>(result.Handle) : nullptr;
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
		DrawTerrainSettings(terrain);
	}

	void TerrainTool::DrawBrush(const Ref<Terrain>& terrain)
	{
		const TerrainSettings& settings = terrain->GetSettings();
		DrawSection("Brush");
		ImGui::Checkbox("Edit in the Editor view", &m_Editing);
		const TerrainBrush brushes[] = { TerrainBrush::Paint, TerrainBrush::Raise, TerrainBrush::Lower, TerrainBrush::Flatten, TerrainBrush::Wall,
			TerrainBrush::Ramp, TerrainBrush::Path };
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
		m_Layer = std::min(m_Layer, layerCount - 1);
		if (m_Brush == TerrainBrush::Paint || m_Brush == TerrainBrush::Path)
		{
			PropertyUI::Row("Layer", [&]()
			{
				const std::string current = std::to_string(m_Layer + 1) + ": " + GetAssetName(settings.Layers[m_Layer].Material);
				if (ImGui::BeginCombo("##BrushLayer", current.c_str()))
				{
					for (uint32_t i = 0; i < layerCount; i++)
					{
						const std::string name = std::to_string(i + 1) + ": " + GetAssetName(settings.Layers[i].Material);
						if (ImGui::Selectable(name.c_str(), i == m_Layer))
						{
							m_Layer = i;
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
				if (ImGui::BeginCombo("##BrushWall", WallLayerName(settings, m_WallLayer).c_str()))
				{
					for (uint32_t i = 0; i < std::max<uint32_t>((uint32_t)settings.WallLayers.size(), 1u); i++)
					{
						if (ImGui::Selectable(WallLayerName(settings, (uint8_t)i).c_str(), m_WallLayer == i))
						{
							m_WallLayer = (uint8_t)i;
						}
					}
					if (ImGui::Selectable(WallLayerName(settings, TerrainWallHidden).c_str(), m_WallLayer == TerrainWallHidden))
					{
						m_WallLayer = TerrainWallHidden;
					}
					ImGui::EndCombo();
				}
			});
		}
		if (m_Brush != TerrainBrush::Path)
		{
			PropertyUI::Row("Radius", [&]() { PropertyUI::InputFloat("##BrushRadius", m_Radius, "%.2f m", 0.1f, 50.0f); });
		}
		if (m_Brush == TerrainBrush::Paint)
		{
			PropertyUI::Row("Strength", [&]() { PropertyUI::InputFloat("##BrushStrength", m_Strength, "%.2f", 0.01f, 1.0f); });
			PropertyUI::Row("Hardness", [&]() { PropertyUI::InputFloat("##BrushHardness", m_Hardness, "%.2f", 0.0f, 1.0f); });
		}
		if (m_Brush == TerrainBrush::Path)
		{
			PropertyUI::Row("Width", [&]() { PropertyUI::InputFloat("##PathWidth", m_PathWidth, "%.2f m", 0.2f, 20.0f); });
			PropertyUI::Row("Wander", [&]() { PropertyUI::InputFloat("##PathWander", m_PathWander, "%.2f m", 0.0f, 2.0f); });
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

	void TerrainTool::DrawTerrainSettings(const Ref<Terrain>& terrain)
	{
		// The terrain's own settings: edited on a copy, applied when something changed (saved with the scene).
		TerrainSettings settings = terrain->GetSettings();
		auto pickMaterial = [](const char* id, AssetHandle& handle)
		{
			const AssetPickerResult result = AssetPicker::DrawField(id, handle, AssetType::Material);
			if (result.Changed && ((uint64_t)result.Handle == 0 || AssetManager::GetAsset<MaterialAsset>(result.Handle) != nullptr))
			{
				handle = result.Handle;
			}
		};

		DrawSection("Layers");
		for (uint32_t i = 0; i < (uint32_t)settings.Layers.size(); i++)
		{
			ImGui::PushID((int)i);
			TerrainLayer& layer = settings.Layers[i];
			char label[32];
			std::snprintf(label, sizeof(label), "Layer %u", i + 1);
			PropertyUI::Row(label, [&]() { pickMaterial("##LayerMaterial", layer.Material); });
			PropertyUI::Row("Edge Roughness", [&]()
			{
				PropertyUI::InputFloat("##EdgeRoughness", layer.EdgeRoughness, "%.2f", 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How ragged this layer's border with the others is: 0 a clean painted edge, 1 very ragged.");
				}
			});
			PropertyUI::Row("Blend", [&]()
			{
				PropertyUI::InputFloat("##LayerBlend", layer.Blend, "%.2f", 0.0f, 2.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How wide the band is where this layer's colour mixes with its neighbour's (in two steps,\n"
						"so it stays pixel art). 0: hard pixel borders. Paint with a soft brush (low Hardness) to widen it.");
				}
			});
			PropertyUI::Row("Height Blend", [&]()
			{
				PropertyUI::InputFloat("##LayerHeightBlend", layer.HeightBlend, "%.2f", 0.0f, 3.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How much this layer's own texture shapes the border: its bright parts (grass blades, stones)\n"
						"reach into the neighbour and its dark gaps let the neighbour through. 0: off.");
				}
			});
			PropertyUI::Row("Fringe", [&]()
			{
				pickMaterial("##LayerFringe", layer.Fringe);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("A cut-out strip hanging over the walls below this layer (grass), or none.");
				}
			});
			if ((uint64_t)layer.Fringe != 0)
			{
				PropertyUI::Row("Fringe Length", [&]() { PropertyUI::InputFloat("##FringeLength", layer.FringeLength, "%.1f px", 0.0f, 32.0f); });
			}
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

		DrawSection("Wall Layers");
		for (uint32_t i = 0; i < (uint32_t)settings.WallLayers.size(); i++)
		{
			ImGui::PushID(100 + (int)i);
			TerrainWallLayer& wall = settings.WallLayers[i];
			char label[32];
			std::snprintf(label, sizeof(label), "Wall %u", i + 1);
			PropertyUI::Row(label, [&]() { pickMaterial("##WallMaterial", wall.Material); });
			PropertyUI::Row("Variation", [&]()
			{
				PropertyUI::InputFloat("##WallVariation", wall.Variation, "%.2f", 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Scales the outline jitter and wall noise: stone 0.65, earth banks 0.57 (approved look).");
				}
			});
			PropertyUI::Row("Cap Stones", [&]() { ImGui::Checkbox("##WallCaps", &wall.CapStones); });
			PropertyUI::Row("Slope", [&]()
			{
				PropertyUI::InputFloat("##WallSlope", wall.Slope, "%.2f", 0.0f, 0.8f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How far the wall's foot steps out per metre of height: 0 upright, 0.4 to 0.55 a grassy earth bank\n"
						"(still too steep to walk up). The slope covers the lower ground at its foot.");
				}
			});
			ImGui::PopID();
		}
		if (settings.WallLayers.size() < MaxTerrainWallLayers && ImGui::Button("Add Wall Layer"))
		{
			settings.WallLayers.push_back(TerrainWallLayer{});
		}
		if (settings.WallLayers.empty() == false)
		{
			if (settings.WallLayers.size() < MaxTerrainWallLayers)
			{
				ImGui::SameLine();
			}
			if (ImGui::Button("Remove Last Wall Layer"))
			{
				settings.WallLayers.pop_back();
			}
		}

		DrawSection("Edges");
		PropertyUI::Row("Outline Jitter", [&]() { PropertyUI::InputFloat("##Jitter", settings.Jitter, "%.2f", 0.0f, 0.45f); });
		PropertyUI::Row("Lip", [&]() { PropertyUI::InputFloat("##LipWidth", settings.LipWidth, "%.1f px", 0.0f, 4.0f); });
		PropertyUI::Row("Lip Face", [&]() { PropertyUI::InputFloat("##LipFace", settings.LipFaceHeight, "%.1f px", 0.0f, 8.0f); });
		PropertyUI::Row("Wall Lean", [&]() { PropertyUI::InputFloat("##WallLean", settings.WallLean, "%.1f px/m", 0.0f, 8.0f); });
		PropertyUI::Row("Wall Noise", [&]() { PropertyUI::InputFloat("##WallNoise", settings.WallNoise, "%.1f px", 0.0f, 8.0f); });
		PropertyUI::Row("Wall Light", [&]()
		{
			PropertyUI::InputFloat("##WallLight", settings.WallLight, "%.2f", 0.0f, 1.0f);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Lights the walls as if they faced partly up (0: by their face, 1: like the ground), so walls\n"
					"turned away from the sun still read.");
			}
		});

		DrawSection("Cap Stones");
		PropertyUI::Row("Material", [&]() { pickMaterial("##CapMaterial", settings.CapStoneMaterial); });
		PropertyUI::Row("Pitch", [&]() { PropertyUI::InputFloat("##CapPitch", settings.CapPitch, "%.2f m", 0.05f, 2.0f); });
		PropertyUI::Row("Missing", [&]() { PropertyUI::InputFloat("##CapMissing", settings.CapMissing, "%.2f", 0.0f, 1.0f); });

		DrawSection("Decor");
		static const char* places[] = { "Layer", "Layer Border", "Wall Top", "Wall Foot" };
		static const char* shapes[] = { "Upright", "Flat", "Stone" };
		int removeRule = -1;
		for (uint32_t r = 0; r < (uint32_t)settings.Decor.size(); r++)
		{
			ImGui::PushID(200 + (int)r);
			TerrainDecorRule& rule = settings.Decor[r];
			char label[32];
			std::snprintf(label, sizeof(label), "Rule %u", r + 1);
			PropertyUI::Row(label, [&]()
			{
				int place = (int)rule.Place;
				if (ImGui::Combo("##DecorPlace", &place, places, 4))
				{
					rule.Place = (TerrainDecorPlace)place;
				}
			});
			const bool onWalls = rule.Place == TerrainDecorPlace::WallTop || rule.Place == TerrainDecorPlace::WallFoot;
			PropertyUI::Row(onWalls ? "Wall Layer" : "Layer", [&]()
			{
				int layer = rule.Layer == TerrainDecorAnyLayer ? 0 : (int)rule.Layer + (onWalls ? 1 : 0);
				if (onWalls)
				{
					const char* names[] = { "Any", "Wall 1", "Wall 2", "Wall 3", "Wall 4" };
					if (ImGui::Combo("##DecorWall", &layer, names, 5))
					{
						rule.Layer = layer == 0 ? TerrainDecorAnyLayer : (uint32_t)(layer - 1);
					}
				}
				else
				{
					uint32_t value = std::min(rule.Layer, MaxTerrainLayers - 1) + 1;
					if (PropertyUI::InputUInt("##DecorLayer", value, 1, MaxTerrainLayers))
					{
						rule.Layer = value - 1;
					}
				}
			});
			PropertyUI::Row("Shape", [&]()
			{
				int shape = (int)rule.Shape;
				if (ImGui::Combo("##DecorShape", &shape, shapes, 3))
				{
					rule.Shape = (TerrainDecorShape)shape;
				}
			});
			PropertyUI::Row("Density", [&]() { PropertyUI::InputFloat("##DecorDensity", rule.Density, onWalls ? "%.2f / m" : "%.2f / m2", 0.0f, 64.0f); });
			PropertyUI::Row("Scale", [&]()
			{
				float values[2] = { rule.Scale.x, rule.Scale.y };
				if (ImGui::InputFloat2("##DecorScale", values, "%.2f"))
				{
					rule.Scale = glm::clamp(glm::vec2(values[0], std::max(values[0], values[1])), glm::vec2(0.05f), glm::vec2(8.0f));
				}
			});
			int removeSprite = -1;
			for (uint32_t s = 0; s < (uint32_t)rule.Sprites.size(); s++)
			{
				ImGui::PushID((int)s);
				PropertyUI::Row(s == 0 ? "Sprites" : "", [&]()
				{
					const AssetPickerResult result = AssetPicker::DrawField("##DecorSprite", rule.Sprites[s], AssetType::Texture2D, {}, -30.0f);
					if (result.Changed && ((uint64_t)result.Handle == 0 || AssetManager::GetAsset<Texture2D>(result.Handle) != nullptr))
					{
						rule.Sprites[s] = result.Handle;
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("x"))
					{
						removeSprite = (int)s;
					}
				});
				ImGui::PopID();
			}
			if (removeSprite >= 0)
			{
				rule.Sprites.erase(rule.Sprites.begin() + removeSprite);
			}
			if (rule.Sprites.size() < MaxTerrainDecorSprites && ImGui::Button("Add Sprite"))
			{
				rule.Sprites.push_back(AssetHandle(0));
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove Rule"))
			{
				removeRule = (int)r;
			}
			ImGui::PopID();
		}
		if (removeRule >= 0)
		{
			settings.Decor.erase(settings.Decor.begin() + removeRule);
		}
		if (settings.Decor.size() < MaxTerrainDecorRules && ImGui::Button("Add Decor Rule"))
		{
			settings.Decor.push_back(TerrainDecorRule{});
		}

		DrawSection("Edge Noise");
		PropertyUI::Row("Cluster Size", [&]() { PropertyUI::InputFloat("##EdgeClusterSize", settings.EdgeClusterSize, "%.1f px", 0.5f, 16.0f); });
		PropertyUI::Row("Pixel Noise", [&]() { PropertyUI::InputFloat("##EdgePixelNoise", settings.EdgePixelNoise, "%.2f", 0.0f, 1.0f); });
		PropertyUI::Row("Seed", [&]() { PropertyUI::InputUInt("##TerrainSeed", settings.Seed, 0, 0x7fffffffu); });

		if (settings != terrain->GetSettings())
		{
			terrain->SetSettings(settings);
		}
	}

	bool TerrainTool::UpdateViewport(Entity selected, const EditorCamera& camera, const glm::vec2& imageMin, const glm::vec2& imageSize, bool hovered,
		bool playing, float deltaTime, EditorCommandStack& commands)
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
				m_Radius = std::max(0.1f, m_Radius / 1.25f);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_RightBracket))
			{
				m_Radius = std::min(50.0f, m_Radius * 1.25f);
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

		// A ray from the camera through the cursor: unproject the cursor at the near and far planes (zero-to-one depth,
		// the projection's Y flip puts the image's top at -1).
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
				case TerrainBrush::Paint: Paint(*terrain, centre, m_Shift, deltaTime); break;
				case TerrainBrush::Raise: ChangeLevels(*terrain, centre, m_Shift ? -1 : 1, false); break;
				case TerrainBrush::Lower: ChangeLevels(*terrain, centre, m_Shift ? 1 : -1, false); break;
				case TerrainBrush::Flatten: ChangeLevels(*terrain, centre, 0, true); break;
				case TerrainBrush::Wall: PaintWalls(*terrain, centre); break;
				case TerrainBrush::Ramp: PaintRamps(*terrain, centre, m_Shift); break;
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

	void TerrainTool::ChangeLevels(Terrain& terrain, const glm::vec2& centre, int32_t step, bool flatten)
	{
		const float cell = terrain.GetSettings().CellSize;
		const int minX = std::max(0, (int)std::ceil((centre.x - m_Radius) / cell));
		const int minY = std::max(0, (int)std::ceil((centre.y - m_Radius) / cell));
		const int maxX = std::min((int)terrain.GetCornerCountX() - 1, (int)std::floor((centre.x + m_Radius) / cell));
		const int maxY = std::min((int)terrain.GetCornerCountY() - 1, (int)std::floor((centre.y + m_Radius) / cell));
		bool changed = false;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				if (glm::distance(glm::vec2((float)x, (float)y) * cell, centre) > m_Radius)
				{
					continue;
				}
				const size_t index = (size_t)y * terrain.GetCornerCountX() + x;
				const int32_t old = terrain.GetLevel((uint32_t)x, (uint32_t)y);
				int32_t level = old;
				if (flatten)
				{
					level = m_FlattenLevel;
				}
				else if (m_StrokeTouched[index] == 0)
				{
					level = glm::clamp(old + step, -128, 127);
					m_StrokeTouched[index] = 1;
				}
				if (level != old)
				{
					terrain.SetLevel((uint32_t)x, (uint32_t)y, (int8_t)level);
					changed = true;
				}
			}
		}
		if (changed)
		{
			// The cells touching the corners.
			TerrainRect cells;
			cells.Min = glm::uvec2((uint32_t)std::max(minX - 1, 0), (uint32_t)std::max(minY - 1, 0));
			cells.Max = glm::uvec2((uint32_t)std::min(maxX, (int)terrain.GetCellCountX() - 1), (uint32_t)std::min(maxY, (int)terrain.GetCellCountY() - 1));
			terrain.MarkCellsChanged(cells);
		}
	}

	// The cells whose centres are inside the brush.
	template<typename Visit>
	static void ForCellsUnderBrush(const Terrain& terrain, const glm::vec2& centre, float radius, Visit&& visit)
	{
		const float cell = terrain.GetSettings().CellSize;
		const int minX = std::max(0, (int)std::floor((centre.x - radius) / cell));
		const int minY = std::max(0, (int)std::floor((centre.y - radius) / cell));
		const int maxX = std::min((int)terrain.GetCellCountX() - 1, (int)std::floor((centre.x + radius) / cell));
		const int maxY = std::min((int)terrain.GetCellCountY() - 1, (int)std::floor((centre.y + radius) / cell));
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				if (glm::distance((glm::vec2((float)x, (float)y) + 0.5f) * cell, centre) <= radius)
				{
					visit((uint32_t)x, (uint32_t)y);
				}
			}
		}
	}

	void TerrainTool::PaintWalls(Terrain& terrain, const glm::vec2& centre)
	{
		bool changed = false;
		TerrainRect rect{ glm::uvec2(UINT32_MAX), glm::uvec2(0) };
		ForCellsUnderBrush(terrain, centre, m_Radius, [&](uint32_t x, uint32_t y)
		{
			if (terrain.GetWallMaterial(x, y) != m_WallLayer)
			{
				terrain.SetWallMaterial(x, y, m_WallLayer);
				rect.Min = glm::min(rect.Min, glm::uvec2(x, y));
				rect.Max = glm::max(rect.Max, glm::uvec2(x, y));
				changed = true;
			}
		});
		if (changed)
		{
			terrain.MarkCellsChanged(rect);
		}
	}

	void TerrainTool::PaintRamps(Terrain& terrain, const glm::vec2& centre, bool clear)
	{
		// The direction is the main axis of the drag so far (from the low side towards the high side).
		TerrainRamp ramp = TerrainRamp::None;
		if (clear == false)
		{
			const glm::vec2 drag = centre - m_StrokeStart;
			if (glm::length(drag) < 0.25f)
			{
				return;
			}
			if (std::abs(drag.x) > std::abs(drag.y))
			{
				ramp = drag.x > 0.0f ? TerrainRamp::East : TerrainRamp::West;
			}
			else
			{
				ramp = drag.y > 0.0f ? TerrainRamp::North : TerrainRamp::South;
			}
		}
		bool changed = false;
		TerrainRect rect{ glm::uvec2(UINT32_MAX), glm::uvec2(0) };
		ForCellsUnderBrush(terrain, centre, m_Radius, [&](uint32_t x, uint32_t y)
		{
			if (terrain.GetRamp(x, y) != ramp)
			{
				terrain.SetRamp(x, y, ramp);
				rect.Min = glm::min(rect.Min, glm::uvec2(x, y));
				rect.Max = glm::max(rect.Max, glm::uvec2(x, y));
				changed = true;
			}
		});
		if (changed)
		{
			terrain.MarkCellsChanged(rect);
		}
	}

	void TerrainTool::ApplyPath(const Ref<Terrain>& terrain, AssetHandle handle, EditorCommandStack& commands)
	{
		if (m_PathPoints.size() < 2)
		{
			return;
		}

		// Catmull-Rom through the points, so the road bends instead of kinking.
		std::vector<glm::vec2> curve;
		std::vector<glm::vec2> points;
		points.push_back(glm::vec2(m_PathPoints.front()));
		for (const glm::vec3& point : m_PathPoints)
		{
			points.push_back(glm::vec2(point));
		}
		points.push_back(glm::vec2(m_PathPoints.back()));
		for (size_t k = 1; k + 2 < points.size(); k++)
		{
			const glm::vec2 p0 = points[k - 1];
			const glm::vec2 p1 = points[k];
			const glm::vec2 p2 = points[k + 1];
			const glm::vec2 p3 = points[k + 2];
			const int samples = std::max(2, (int)std::ceil(glm::distance(p1, p2) / 0.1f));
			for (int n = 0; n < samples; n++)
			{
				const float t = (float)n / (float)samples;
				const float t2 = t * t;
				const float t3 = t2 * t;
				curve.push_back(0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
			}
		}
		curve.push_back(glm::vec2(m_PathPoints.back()));

		BeginStroke(terrain, handle);
		const TerrainSettings& settings = terrain->GetSettings();
		const uint32_t layerCount = std::min((uint32_t)settings.Layers.size(), MaxTerrainLayers);
		const uint32_t layer = std::min(m_Layer, layerCount - 1);
		const float resolution = (float)settings.SplatResolution;
		const float half = 0.5f * m_PathWidth;
		const float soft = 0.35f; // the painted edge's width; the edge noise makes it ragged
		const float reach = half + m_PathWander + soft;
		glm::vec2 low = curve.front();
		glm::vec2 high = curve.front();
		for (const glm::vec2& point : curve)
		{
			low = glm::min(low, point);
			high = glm::max(high, point);
		}
		const int width = (int)terrain->GetWeightMapWidth();
		const int height = (int)terrain->GetWeightMapHeight();
		const int minX = std::max(0, (int)std::floor((low.x - reach) * resolution));
		const int minY = std::max(0, (int)std::floor((low.y - reach) * resolution));
		const int maxX = std::min(width - 1, (int)std::ceil((high.x + reach) * resolution));
		const int maxY = std::min(height - 1, (int)std::ceil((high.y + reach) * resolution));
		const float seed = (float)(settings.Seed % 1000u);
		bool changed = false;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				const glm::vec2 p((x + 0.5f) / resolution, (y + 0.5f) / resolution);
				float distance = FLT_MAX;
				for (size_t k = 0; k + 1 < curve.size(); k++)
				{
					const glm::vec2 a = curve[k];
					const glm::vec2 d = curve[k + 1] - a;
					const float length2 = glm::dot(d, d);
					const float t = length2 > 0.0f ? glm::clamp(glm::dot(p - a, d) / length2, 0.0f, 1.0f) : 0.0f;
					distance = std::min(distance, glm::distance(p, a + d * t));
				}
				// The edge wanders in and out slowly along the road.
				const float wander = m_PathWander * (0.6f * std::sin(1.7f * p.x + 0.9f * p.y + seed) + 0.4f * std::sin(0.6f * p.x - 1.3f * p.y + 2.0f * seed));
				const float target = glm::clamp((half + wander - distance) / soft + 0.5f, 0.0f, 1.0f);
				if (target <= 0.0f)
				{
					continue;
				}
				const glm::u8vec4 bytes = terrain->GetWeight((uint32_t)x, (uint32_t)y);
				float w[4] = { bytes.x / 255.0f, bytes.y / 255.0f, bytes.z / 255.0f, bytes.w / 255.0f };
				RaiseLayerTo(w, layer, layerCount, target);
				const glm::u8vec4 value = WeightsToBytes(w);
				if (value != bytes)
				{
					terrain->SetWeight((uint32_t)x, (uint32_t)y, value);
					changed = true;
				}
			}
		}
		if (changed)
		{
			terrain->MarkWeightsChanged(TerrainRect{ glm::uvec2((uint32_t)minX, (uint32_t)minY), glm::uvec2((uint32_t)maxX, (uint32_t)maxY) });
		}
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
		overlay.TerrainBrushRadius = m_Radius;
		// The opposite action (Shift) shows in a warm red, the brush in white (drawn over the scene, any theme).
		overlay.TerrainBrushColor = m_Shift ? glm::vec4(1.0f, 0.45f, 0.35f, 0.95f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.9f);
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
			terrain.MarkWeightsChanged(TerrainRect{ glm::uvec2((uint32_t)minX, (uint32_t)minY), glm::uvec2((uint32_t)maxX, (uint32_t)maxY) });
		}
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
