#include "TerrainSettingsPanel.h"
#include "EditorWindows/PropertyUI.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/Texture2D.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cstdio>

namespace Hydro
{
	void TerrainSettingsPanel::DrawSection(const char* title)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("%s", title);
	}

	std::string TerrainSettingsPanel::GetAssetName(AssetHandle handle)
	{
		if ((uint64_t)handle == 0)
		{
			return "none";
		}
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		return metadata != nullptr ? metadata->Path.stem().u8string() : "missing";
	}

	void TerrainSettingsPanel::Draw(const Ref<Terrain>& terrain)
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
				PropertyUI::Row("Fringe Length", [&]()
					{ PropertyUI::InputFloat("##FringeLength", layer.FringeLength, "%.1f px", 0.0f, 32.0f); });
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
			PropertyUI::Row("Density", [&]()
				{ PropertyUI::InputFloat("##DecorDensity", rule.Density, onWalls ? "%.2f / m" : "%.2f / m2", 0.0f, 64.0f); });
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
					const AssetPickerResult result = AssetPicker::DrawField("##DecorSprite", rule.Sprites[s], AssetType::Texture2D,
						{}, -30.0f);
					if (result.Changed
						&& ((uint64_t)result.Handle == 0 || AssetManager::GetAsset<Texture2D>(result.Handle) != nullptr))
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
		PropertyUI::Row("Cluster Size", [&]()
			{ PropertyUI::InputFloat("##EdgeClusterSize", settings.EdgeClusterSize, "%.1f px", 0.5f, 16.0f); });
		PropertyUI::Row("Pixel Noise", [&]() { PropertyUI::InputFloat("##EdgePixelNoise", settings.EdgePixelNoise, "%.2f", 0.0f, 1.0f); });
		PropertyUI::Row("Seed", [&]() { PropertyUI::InputUInt("##TerrainSeed", settings.Seed, 0, 0x7fffffffu); });

		if (settings != terrain->GetSettings())
		{
			terrain->SetSettings(settings);
		}
	}
}
