#include "MaterialEditor.h"
#include "InspectorUI.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/MaterialSerializer.h>

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <string>

namespace Hydro
{
	// The Standard material's fields (a mesh slot's inline settings or a material asset). True when something changed.
	static bool DrawMaterialFields(MaterialData& material)
	{
		const MaterialData before = material;
		InspectorUI::Row("Albedo", [&]() { InspectorUI::TextureField("##MaterialAlbedo", material.Albedo); });
		InspectorUI::Row("Tint", [&]() { ImGui::ColorEdit4("##MaterialTint", glm::value_ptr(material.Tint)); });
		InspectorUI::Row("Tiling", [&]()
		{
			ImGui::DragFloat2("##MaterialTiling", glm::value_ptr(material.Tiling), 0.05f, 0.01f, 100.0f, "%.2f");
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Texture repeats per metre.");
			}
		});
		InspectorUI::Row("Normal map", [&]()
		{
			InspectorUI::TextureField("##MaterialNormal", material.Normal);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Tangent space, green up (glTF / OpenGL). Import it as RGBA (data), not SRGB.\nscripts/game/textures/make_normal_maps.py makes them from albedos.");
			}
		});
		if ((uint64_t)material.Normal != 0)
		{
			InspectorUI::Row("Normal strength", [&]()
			{
				InspectorUI::InputFloat("##MaterialNormalStrength", material.NormalStrength, "%.2f", 0.0f, 4.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("0 = flat, 1 = as the map says, 2 = twice as steep.");
				}
			});
		}
		InspectorUI::Row("Emissive", [&]() { InspectorUI::TextureField("##MaterialEmissive", material.Emissive); });
		InspectorUI::Row("Emission", [&]() { ImGui::ColorEdit3("##MaterialEmission", glm::value_ptr(material.EmissionColor)); });
		InspectorUI::Row("Intensity",
			[&]() { InspectorUI::InputFloat("##MaterialEmissionIntensity", material.EmissionIntensity, "%.2f", 0.0f, 100.0f); });
		InspectorUI::Row("Alpha cut-off", [&]()
		{
			InspectorUI::InputFloat("##MaterialAlphaCutoff", material.AlphaCutoff, "%.2f", 0.0f, 1.0f);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Pixels with less albedo alpha than this are dropped (railings, leaves). 0 = opaque.");
			}
		});
		InspectorUI::Row("Filter", [&]()
		{
			const char* filterNames[] = { "Nearest", "Linear" };
			int filter = material.Filter == TextureFilter::Linear ? 1 : 0;
			if (ImGui::Combo("##MaterialFilter", &filter, filterNames, 2))
			{
				material.Filter = filter == 1 ? TextureFilter::Linear : TextureFilter::Nearest;
			}
		});
		return material != before;
	}

	// "<folder>/<name>.hymat", or "<name> 2.hymat" when taken.
	static std::filesystem::path MakeUniqueMaterialPath(const std::filesystem::path& folder, std::string name)
	{
		// Characters Windows doesn't allow in file names become '_'.
		for (char& c : name)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
			{
				c = '_';
			}
		}
		if (name.empty())
		{
			name = "New Material";
		}

		std::error_code error;
		std::filesystem::path path = folder / std::filesystem::u8path(name + MaterialSerializer::FileExtension);
		for (int number = 2; std::filesystem::exists(path, error); number++)
		{
			path = folder / std::filesystem::u8path(name + " " + std::to_string(number) + MaterialSerializer::FileExtension);
		}
		return path;
	}

	void MaterialEditor::EditAsset(const Ref<MaterialAsset>& asset)
	{
		MaterialData data = asset->GetData();
		if (DrawMaterialFields(data))
		{
			// Every mesh using it changes now; the file is written once the edit is finished.
			if (m_UnsavedMaterial != asset->GetHandle())
			{
				Save(true);
			}
			asset->SetData(data);
			m_UnsavedMaterial = asset->GetHandle();
		}
	}

	void MaterialEditor::Save(bool force)
	{
		if ((uint64_t)m_UnsavedMaterial == 0)
		{
			return;
		}
		// Not while a field is being dragged or typed in: a slider would write the file every frame.
		if (force == false && ImGui::IsAnyItemActive())
		{
			return;
		}

		const AssetHandle handle = m_UnsavedMaterial;
		m_UnsavedMaterial = 0;
		const Ref<MaterialAsset> asset = AssetManager::GetLoadedAsset<MaterialAsset>(handle);
		const std::filesystem::path path = AssetManager::GetAbsolutePath(handle);
		if (asset == nullptr || path.empty())
		{
			return;
		}
		MaterialSerializer::Serialize(path, asset->GetData());
	}

	void MaterialEditor::AssignModelMaterials(MeshComponent& mesh, const Mesh& model)
	{
		// One slot per model material, each using the material asset the import made for it.
		const uint32_t count = model.GetMaterialSlotCount();
		mesh.Materials.assign(count, MeshMaterialSlot{});
		const std::vector<MeshMaterialInfo>& infos = model.GetMaterials();
		for (uint32_t slot = 0; slot < count && slot < (uint32_t)infos.size(); slot++)
		{
			const AssetHandle handle = infos[slot].DefaultMaterial;
			const Ref<MaterialAsset> asset = (uint64_t)handle != 0 ? AssetManager::GetAsset<MaterialAsset>(handle) : nullptr;
			if (asset != nullptr)
			{
				mesh.Materials[slot].Material = handle;
				mesh.Materials[slot].Inline = asset->GetData();
			}
		}
	}

	void MaterialEditor::DrawMeshSlots(Entity entity, MeshComponent& mesh, const Mesh* model)
	{
		// A built-in shape has one slot: shown open. A model's slots are listed by the model's material names.
		if (model == nullptr && mesh.Materials.size() == 1)
		{
			ImGui::Separator();
			ImGui::TextUnformatted("Material");
			DrawMeshSlot(entity, mesh.Materials[0]);
			return;
		}

		ImGui::Separator();
		ImGui::Text("Materials (%u)", (uint32_t)mesh.Materials.size());
		for (uint32_t i = 0; i < (uint32_t)mesh.Materials.size(); i++)
		{
			std::string name = "Slot " + std::to_string(i);
			if (model != nullptr && i < model->GetMaterials().size() && model->GetMaterials()[i].Name.empty() == false)
			{
				name += ": " + model->GetMaterials()[i].Name;
			}
			ImGui::PushID((int)i);
			if (ImGui::TreeNodeEx(name.c_str(), mesh.Materials.size() <= 2 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
			{
				DrawMeshSlot(entity, mesh.Materials[i]);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void MaterialEditor::DrawMeshSlot(Entity entity, MeshMaterialSlot& slot)
	{
		const Ref<MaterialAsset> asset = (uint64_t)slot.Material != 0 ? AssetManager::GetAsset<MaterialAsset>(slot.Material) : nullptr;

		InspectorUI::Row("Asset", [&]()
		{
			const std::string detail = (uint64_t)slot.Material != 0 && asset == nullptr
				? "The material is missing or couldn't be loaded (see the console); the settings below are drawn." : std::string();
			const AssetPickerResult result = AssetPicker::DrawField("##MeshMaterialAsset", slot.Material, AssetType::Material, detail);
			if (result.Changed == false)
			{
				return;
			}

			if ((uint64_t)result.Handle == 0)
			{
				// Detached: the slot keeps looking the same, now with its own copy of the settings.
				if (asset != nullptr)
				{
					slot.Inline = asset->GetData();
				}
				slot.Material = 0;
				return;
			}

			// Loading logs why a file couldn't be loaded; a handle that doesn't load is not taken.
			const Ref<MaterialAsset> picked = AssetManager::GetAsset<MaterialAsset>(result.Handle);
			if (picked != nullptr)
			{
				slot.Material = result.Handle;
				// The fallback, should the file go missing later.
				slot.Inline = picked->GetData();
			}
		});

		if (asset != nullptr)
		{
			const std::filesystem::path path = AssetManager::GetAbsolutePath(slot.Material);
			ImGui::TextDisabled("Shared: edits change every mesh using %s.", path.filename().u8string().c_str());
			EditAsset(asset);
			return;
		}

		if ((uint64_t)slot.Material != 0)
		{
			ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Material asset missing: drawing these settings.");
		}
		DrawMaterialFields(slot.Inline);

		// Turns these settings into a material asset other meshes can share.
		if ((uint64_t)slot.Material == 0 && AssetManager::IsProjectOpen() && ImGui::Button("Save as Material Asset"))
		{
			const std::filesystem::path folder = AssetManager::GetAssetDirectory() / "Materials";
			std::error_code error;
			std::filesystem::create_directories(folder, error);
			const std::string name = entity.HasComponent<TagComponent>() ? entity.GetComponent<TagComponent>().Tag : std::string();
			const std::filesystem::path path = MakeUniqueMaterialPath(folder, name);
			if (MaterialSerializer::Serialize(path, slot.Inline))
			{
				const AssetHandle handle = AssetManager::FindHandle(path);
				if ((uint64_t)handle != 0 && AssetManager::GetAsset<MaterialAsset>(handle) != nullptr)
				{
					slot.Material = handle;
					HY_INFO("Created {}", path.filename().u8string());
				}
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Writes these settings to Assets/Materials as a .hymat and uses it here; pick it on other meshes to share it.");
		}
	}
}
