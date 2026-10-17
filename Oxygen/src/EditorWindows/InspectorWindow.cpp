#include "InspectorWindow.h"
#include "../EditorPaths.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <Hydro/Scene/PrefabSerializer.h>

#include <cstdio>
#include <string>

namespace Hydro
{
	namespace
	{
		constexpr float LabelColumnWidth = 100.0f;

		// One row: label on the left, X/Y/Z fields on the right. Clicking an axis button
		// resets that axis. Returns true when a value changed.
		bool DrawVec3Control(const char* label, glm::vec3& values, float resetValue = 0.0f, float speed = 0.1f)
		{
			bool changed = false;

			ImGui::PushID(label);

			if (ImGui::BeginTable("##vec3", 2, ImGuiTableFlags_None))
			{
				ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
				ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);

				ImGui::TableSetColumnIndex(1);

				const ImGuiStyle& style = ImGui::GetStyle();
				const float lineHeight = ImGui::GetFrameHeight();
				const ImVec2 buttonSize = { lineHeight, lineHeight };
				const float fieldWidth = (ImGui::GetContentRegionAvail().x - 3.0f * buttonSize.x - 2.0f * style.ItemSpacing.x) / 3.0f;

				struct Axis { const char* Name; ImVec4 Color; ImVec4 Hovered; };
				const Axis axes[3] =
				{
					{ "X", { 0.80f, 0.10f, 0.15f, 1.0f }, { 0.90f, 0.20f, 0.20f, 1.0f } },
					{ "Y", { 0.20f, 0.70f, 0.20f, 1.0f }, { 0.30f, 0.80f, 0.30f, 1.0f } },
					{ "Z", { 0.10f, 0.25f, 0.80f, 1.0f }, { 0.20f, 0.35f, 0.90f, 1.0f } },
				};

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, style.ItemSpacing.y });
				for (int i = 0; i < 3; i++)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, axes[i].Color);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, axes[i].Hovered);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, axes[i].Color);
					if (ImGui::Button(axes[i].Name, buttonSize))
					{
						values[i] = resetValue;
						changed = true;
					}
					ImGui::PopStyleColor(3);

					ImGui::SameLine();
					ImGui::SetNextItemWidth(fieldWidth);
					ImGui::PushID(i);
					changed |= ImGui::DragFloat("##value", &values[i], speed, 0.0f, 0.0f, "%.2f");
					ImGui::PopID();

					if (i < 2)
					{
						ImGui::SameLine(0.0f, style.ItemSpacing.x);
					}
				}
				ImGui::PopStyleVar();

				ImGui::EndTable();
			}

			ImGui::PopID();
			return changed;
		}

		// A label column + one widget, matching the Vec3 rows.
		template<typename WidgetFunction>
		void DrawPropertyRow(const char* label, WidgetFunction widget)
		{
			ImGui::PushID(label);
			if (ImGui::BeginTable("##property", 2, ImGuiTableFlags_None))
			{
				ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
				ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1.0f);
				widget();

				ImGui::EndTable();
			}
			ImGui::PopID();
		}

		// Collapsible section for one component type. Right-click the header to remove it.
		// Removal is only *requested* here; the caller applies it after drawing, so no
		// component reference is held while EnTT storage changes.
		template<typename T, typename UIFunction>
		void DrawComponent(const char* name, Entity entity, bool& removeRequested, UIFunction uiFunction)
		{
			if (entity.HasComponent<T>() == false)
				return;

			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

			ImGui::PushID(name);
			ImGui::Spacing();
			bool open = ImGui::TreeNodeEx("##component", flags, "%s", name);

			if (ImGui::BeginPopupContextItem("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove Component"))
					removeRequested = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(entity.GetComponent<T>());
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorWindow::OnImguiRender(Entity entity)
	{
		ImGui::Begin("Inspector");

		const bool hasEntity = entity && entity.GetScene() != nullptr && entity.GetScene()->m_Registry.valid(entity);
		if (hasEntity == false)
		{
			ImGui::TextDisabled("Select an entity in the hierarchy.");
			ImGui::End();
			return;
		}

		DrawEntityHeader(entity);
		DrawComponents(entity);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		DrawAddComponentButton(entity);
		DrawPrefabButton(entity);

		ImGui::End();
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
				tag = buffer;
		}

		if (entity.HasComponent<IDComponent>())
			ImGui::TextDisabled("UUID %llu", (unsigned long long)(uint64_t)entity.GetComponent<IDComponent>().Id);
	}

	void InspectorWindow::DrawComponents(Entity entity)
	{
		bool removeTransform = false;
		bool removeSprite = false;

		DrawComponent<TransformComponent>("Transform", entity, removeTransform, [](TransformComponent& transform)
		{
			DrawVec3Control("Translation", transform.Translation);

			// Stored in radians, edited in degrees.
			glm::vec3 rotation = glm::degrees(transform.Rotation);
			if (DrawVec3Control("Rotation", rotation, 0.0f, 1.0f))
				transform.Rotation = glm::radians(rotation);

			DrawVec3Control("Scale", transform.Scale, 1.0f);
		});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, removeSprite, [](SpriteRendererComponent& sprite)
		{
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit4("##Color", glm::value_ptr(sprite.Color)); });
			DrawPropertyRow("Tiling", [&]() { ImGui::DragFloat("##Tiling", &sprite.TilingFactor, 0.1f, 0.0f, 100.0f, "%.2f"); });
		});

		// Apply removals after drawing, when no component references are alive.
		if (removeTransform)
			entity.RemoveComponent<TransformComponent>();
		if (removeSprite)
			entity.RemoveComponent<SpriteRendererComponent>();
	}

	void InspectorWindow::DrawAddComponentButton(Entity entity)
	{
		const float width = ImGui::GetContentRegionAvail().x;

		if (ImGui::Button("Add Component", { width, 0.0f }))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			const bool hasTransform = entity.HasComponent<TransformComponent>();
			const bool hasSprite = entity.HasComponent<SpriteRendererComponent>();

			if (ImGui::MenuItem("Transform", nullptr, false, hasTransform == false))
				entity.AddComponent<TransformComponent>();

			if (ImGui::MenuItem("Sprite Renderer", nullptr, false, hasSprite == false))
			{
				entity.AddComponent<SpriteRendererComponent>();

				// A sprite needs a transform to be drawn (SceneRenderer groups on both).
				if (entity.HasComponent<TransformComponent>() == false)
					entity.AddComponent<TransformComponent>(glm::vec3(200.0f, 200.0f, 0.0f), glm::vec3(100.0f, 100.0f, 1.0f));
			}

			if (hasTransform && hasSprite)
				ImGui::TextDisabled("All components added");

			ImGui::EndPopup();
		}
	}

	void InspectorWindow::DrawPrefabButton(Entity entity)
	{
		const std::filesystem::path path = EditorPaths::GetPrefabPath(entity.GetName());

		if (ImGui::Button("Save as Prefab", { ImGui::GetContentRegionAvail().x, 0.0f }))
			PrefabSerializer::Save(entity, path);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", path.string().c_str());
	}
}
