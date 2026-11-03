#include "InspectorWindow.h"
#include "../EditorPaths.h"
#include "Inspector/ComponentDrawers.h"
#include "Inspector/InspectorUI.h"
#include "RenderLookUI.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Asset/AssetMetaSerializer.h>
#include <Hydro/Renderer/MaterialSerializer.h>
#include <Hydro/Renderer/Mesh.h>
#include <Hydro/Renderer/MeshImportSerializer.h>
#include <Hydro/Renderer/RenderProfileSerializer.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/EntityReferences.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/Scene.h>
#include <Hydro/Scene/SpriteAnimationSerializer.h>

#include <cstdio>
#include <string>

namespace Hydro
{
	void InspectorWindow::OnImguiRender(Entity entity, const std::filesystem::path& asset)
	{
		ImGui::Begin("Inspector");

		const bool hasEntity = entity && entity.GetScene() != nullptr && entity.GetScene()->m_Registry.valid(entity);
		if (hasEntity == false)
		{
			if (asset.empty())
			{
				ImGui::TextDisabled("Select an entity in the hierarchy, or an asset in the Project window.");
			}
			else
			{
				DrawAsset(asset);
			}
			SaveEdits();
			ImGui::End();
			return;
		}

		DrawEntityHeader(entity);
		DrawUsedBy(entity);

		InspectorContext context;
		context.Terrain = m_TerrainTool;
		context.Materials = &m_Materials;
		context.Playing = m_Playing;
		ComponentDrawers::DrawAll(entity, context);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		DrawAddComponentButton(entity);
		DrawPrefabButton(entity);

		SaveEdits();
		ImGui::End();
	}

	void InspectorWindow::SaveEdits()
	{
		m_Materials.Save(false);
		m_DataAssets.Save(false);
		RenderLookUI::SaveEditedProfile(false);
	}

	void InspectorWindow::DrawEntityHeader(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			std::string& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			std::snprintf(buffer, sizeof(buffer), "%s", tag.c_str());

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = buffer;
			}
		}

		if (entity.HasComponent<IDComponent>())
		{
			ImGui::TextDisabled("UUID %llu", (unsigned long long)(uint64_t)entity.GetComponent<IDComponent>().Id);
		}
	}

	void InspectorWindow::DrawUsedBy(Entity entity)
	{
		Scene& scene = *entity.GetScene();
		const UUID id = entity.GetUUID();
		const double now = ImGui::GetTime();
		if (id != m_ReferrersOf || m_ReferrersTime < 0.0 || now - m_ReferrersTime > 0.5)
		{
			m_ReferrersOf = id;
			m_ReferrersTime = now;
			m_Referrers.clear();
			for (const EntityReference& reference : EntityReferences::FindReferrers(scene, id))
			{
				Referrer referrer;
				referrer.Source = reference.Source.GetUUID();
				referrer.Where = reference.Field != nullptr ? std::string(reference.Holder) + " > " + reference.Field : reference.Holder;
				m_Referrers.push_back(std::move(referrer));
			}
		}
		if (m_Referrers.empty())
		{
			return;
		}

		ImGui::Spacing();
		char label[48];
		std::snprintf(label, sizeof(label), "Used by (%zu)###UsedBy", m_Referrers.size());
		if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_SpanAvailWidth) == false)
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Entities whose fields or script properties refer to this one.");
			}
			return;
		}
		Entity clicked;
		for (size_t i = 0; i < m_Referrers.size(); i++)
		{
			const Entity source = scene.GetEntityByUUID(m_Referrers[i].Source);
			if ((bool)source == false)
			{
				continue;
			}
			ImGui::PushID((int)i);
			if (ImGui::Selectable(source.GetName().c_str()))
			{
				clicked = source;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Select %s", source.GetName().c_str());
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%s", m_Referrers[i].Where.c_str());
			ImGui::PopID();
		}
		ImGui::TreePop();

		if ((bool)clicked && OnSelectEntity)
		{
			OnSelectEntity(clicked);
		}
	}

	void InspectorWindow::DrawAddComponentButton(Entity entity)
	{
		const float width = ImGui::GetContentRegionAvail().x;

		if (ImGui::Button("Add Component", { width, 0.0f }))
		{
			ImGui::OpenPopup("AddComponent");
		}

		if (ImGui::BeginPopup("AddComponent"))
		{
			ComponentDrawers::DrawAddMenuItems(entity);
			ImGui::EndPopup();
		}
	}

	void InspectorWindow::DrawPrefabButton(Entity entity)
	{
		const std::filesystem::path path = EditorPaths::GetPrefabPath(entity.GetName());

		if (ImGui::Button("Save as Prefab", { ImGui::GetContentRegionAvail().x, 0.0f }))
		{
			PrefabSerializer::Save(entity, path);
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", path.string().c_str());
		}
	}

	void InspectorWindow::DrawAsset(const std::filesystem::path& asset)
	{
		if (Texture2D::IsSupportedFile(asset))
		{
			m_TextureImportPanel.OnReimported = OnTextureReimported;
			m_TextureImportPanel.OnOpenSpriteEditor = OnOpenSpriteEditor;
			m_TextureImportPanel.OnImguiRender(asset);
			return;
		}

		if (asset.extension() == SpriteAnimationSerializer::FileExtension)
		{
			DrawAnimationSetAsset(asset);
			return;
		}

		if (asset.extension() == MaterialSerializer::FileExtension)
		{
			DrawMaterialAsset(asset);
			return;
		}

		if (GetAssetTypeFromExtension(asset) == AssetType::Mesh)
		{
			DrawModelAsset(asset);
			return;
		}

		if (asset.extension() == RenderProfileSerializer::FileExtension)
		{
			DrawRenderProfileAsset(asset);
			return;
		}

		if (GetAssetTypeFromExtension(asset) == AssetType::Definition)
		{
			m_DataAssets.DrawDefinition(asset);
			return;
		}

		if (GetAssetTypeFromExtension(asset) == AssetType::DataTable)
		{
			m_DataAssets.DrawTable(asset);
			return;
		}

		ImGui::TextUnformatted(asset.filename().u8string().c_str());
		ImGui::TextDisabled("Nothing to edit for this kind of file.");
	}

	void InspectorWindow::DrawAnimationSetAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		// Read again only when another file is selected or this one was saved, so a broken file doesn't log every frame.
		std::error_code error;
		const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
		if (path != m_InspectedAnimationPath || writeTime != m_InspectedAnimationTime)
		{
			m_InspectedAnimationPath = path;
			m_InspectedAnimationTime = writeTime;
			m_InspectedAnimationSet = SpriteAnimationSet::LoadFromFile(path);
		}
		const Ref<SpriteAnimationSet>& set = m_InspectedAnimationSet;
		if (set == nullptr)
		{
			ImGui::TextDisabled("This file or its sprite sheet can't be read (see the Console).");
		}
		else
		{
			ImGui::TextDisabled("Sheet: %s", set->GetTexture()->GetPath().filename().u8string().c_str());
			ImGui::Spacing();
			if (ImGui::BeginTable("##Clips", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Clip");
				ImGui::TableSetupColumn("Frames");
				ImGui::TableSetupColumn("Loop");
				ImGui::TableHeadersRow();
				const char* loopNames[] = { "Once", "Loop", "Ping Pong" };
				for (uint32_t i = 0; i < set->GetClipCount(); i++)
				{
					const SpriteAnimationClip& clip = set->GetClip(i);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s%s", clip.Name.c_str(), i == set->GetDefaultClip() ? " (default)" : "");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%zu, %.2f s", clip.Frames.size(), clip.GetLength());
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(loopNames[(int)clip.Loop]);
				}
				ImGui::EndTable();
			}
		}

		ImGui::Spacing();
		if (ImGui::Button("Open in Animation Window", { ImGui::GetContentRegionAvail().x, 0.0f }) && OnOpenAnimation)
		{
			OnOpenAnimation(path);
		}
	}

	void InspectorWindow::DrawModelAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		ImGui::TextDisabled("Model (glTF)");

		const AssetHandle handle = AssetManager::FindHandle(path);
		const std::filesystem::path sidecar = AssetMetaSerializer::GetSidecarPath(path);
		MeshImportMapping mapping;
		if (path != m_ModelSettingsPath)
		{
			m_ModelSettingsPath = path;
			MeshImportSerializer::Deserialize(sidecar, m_ModelSettings, mapping);
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Import settings");
		InspectorUI::Row("Scale", [&]() { InspectorUI::InputFloat("##ModelScale", m_ModelSettings.Scale, "%.3f", 0.001f, 1000.0f); });
		InspectorUI::Row("Y up to Z up", [&]()
		{
			ImGui::Checkbox("##ModelYUp", &m_ModelSettings.ConvertYUpToZUp);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("glTF is Y-up; the engine is Z-up. On: the model stands up and its front (+Z) faces -Y, towards the diorama camera.");
			}
		});
		InspectorUI::Row("Make normals", [&]() { ImGui::Checkbox("##ModelNormals", &m_ModelSettings.GenerateNormals); });
		if (ImGui::Button("Apply (re-import)") && (uint64_t)handle != 0)
		{
			// Keeps the mapping, so its textures and materials keep their handles.
			MeshImportConfig unused;
			MeshImportSerializer::Deserialize(sidecar, unused, mapping);
			MeshImportSerializer::Serialize(sidecar, m_ModelSettings, mapping);
			AssetManager::ReloadAsset<Mesh>(handle);
		}

		const Ref<Mesh> mesh = (uint64_t)handle != 0 ? AssetManager::GetAsset<Mesh>(handle) : nullptr;
		ImGui::Separator();
		if (mesh == nullptr)
		{
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not import the model (see the console).");
			return;
		}

		const glm::vec3 size = mesh->GetBoundsMax() - mesh->GetBoundsMin();
		ImGui::Text("%u sub-meshes, %u triangles, %u vertices", (uint32_t)mesh->GetSubMeshes().size(), mesh->GetIndexCount() / 3,
			mesh->GetVertexCount());
		ImGui::Text("Size %.2f x %.2f x %.2f m (bottom at z %.2f)", size.x, size.y, size.z, mesh->GetBoundsMin().z);
		if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (uint32_t i = 0; i < (uint32_t)mesh->GetMaterials().size(); i++)
			{
				const MeshMaterialInfo& info = mesh->GetMaterials()[i];
				const std::filesystem::path file = (uint64_t)info.DefaultMaterial != 0
					? AssetManager::GetAbsolutePath(info.DefaultMaterial) : std::filesystem::path();
				ImGui::BulletText("%u %s: %s", i, info.Name.c_str(), file.empty() ? "(none)" : file.filename().u8string().c_str());
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Sub-meshes"))
		{
			for (const SubMesh& subMesh : mesh->GetSubMeshes())
			{
				ImGui::BulletText("%s (primitive %u): %u triangles, slot %u", subMesh.SourceName.c_str(), subMesh.SourcePrimitive,
					subMesh.IndexCount / 3, subMesh.MaterialSlot);
			}
			ImGui::TreePop();
		}
		ImGui::Spacing();
		ImGui::TextDisabled("Drag it into the viewport to place it, or pick it in a Mesh component's Model field.");
	}

	void InspectorWindow::DrawMaterialAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		ImGui::TextDisabled("Material (Standard)");

		const AssetHandle handle = AssetManager::FindHandle(path);
		const Ref<MaterialAsset> asset = (uint64_t)handle != 0 ? AssetManager::GetAsset<MaterialAsset>(handle) : nullptr;
		if (asset == nullptr)
		{
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read the material (see the console).");
			return;
		}

		ImGui::Separator();
		m_Materials.EditAsset(asset);
		ImGui::Spacing();
		ImGui::TextDisabled("Edits change every mesh that uses this material; saved when an edit is finished.");
	}

	void InspectorWindow::DrawRenderProfileAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		ImGui::TextDisabled("Render profile");

		const AssetHandle handle = AssetManager::FindHandle(path);
		const Ref<RenderProfile> profile = (uint64_t)handle != 0 ? AssetManager::GetAsset<RenderProfile>(handle) : nullptr;
		if (profile == nullptr)
		{
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read the render profile (see the console).");
			return;
		}

		ImGui::Separator();
		RenderLookUI::EditProfile(profile);
		ImGui::Spacing();
		ImGui::TextDisabled("Edits change every scene that uses this profile; saved when an edit is finished.\nA scene uses it from Render Settings > Look (scene).");
	}
}
