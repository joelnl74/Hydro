#include "InspectorWindow.h"
#include "../EditorPaths.h"
#include "Utilities/AssetDragDrop.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/ScriptableEntity.h>
#include <Hydro/Utils/PlatformUtils.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

namespace Hydro
{
	constexpr float LabelColumnWidth = 100.0f;

	// A number you type into (click, type, Enter or click away). The value updates as you type;
	// it is clamped to [min, max] once you leave the field, so a half-typed number is never
	// clamped out from under you. Returns true when the value changed.
	static bool InputFloatField(const char* id, float& value, const char* format = "%.3f",
		float min = -FLT_MAX, float max = FLT_MAX)
	{
		bool changed = ImGui::InputFloat(id, &value, 0.0f, 0.0f, format);

		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			const float clamped = std::clamp(value, min, max);
			changed |= clamped != value;
			value = clamped;
		}

		return changed;
	}

	// One row: label on the left, X/Y/Z input fields on the right. Clicking an axis button
	// resets that axis. Returns true when a value changed.
	static bool DrawVec3Control(const char* label, glm::vec3& values, float resetValue = 0.0f)
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
				changed |= InputFloatField("##value", values[i], "%.3f");
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
	static void DrawPropertyRow(const char* label, WidgetFunction widget)
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

	// The sprite's texture: click to pick a file, drop an image from the Project window, X to clear.
	// TODO(assets): pick from the AssetManager's textures (with a thumbnail) once it exists.
	static void DrawTextureField(SpriteRendererComponent& sprite)
	{
		const float clearWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const bool hasTexture = sprite.Texture != nullptr;

		std::string label = "None";
		if (hasTexture)
		{
			label = sprite.Texture->GetPath().empty() ? "(made in code)" : sprite.Texture->GetPath().filename().u8string();
		}

		Ref<Texture2D> picked;
		if (ImGui::Button(label.c_str(), { ImGui::GetContentRegionAvail().x - clearWidth - spacing, 0.0f }))
		{
			const std::string file = FileDialogs::OpenFile("Image (*.png;*.jpg;*.jpeg;*.tga;*.bmp)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
			if (file.empty() == false)
			{
				picked = Texture2D::LoadFromFile(file);
			}
		}

		if (ImGui::IsItemHovered())
		{
			if (hasTexture)
			{
				ImGui::SetTooltip("%s\n%u x %u\nClick to pick another image, or drop one from the Project window.",
					sprite.Texture->GetPath().u8string().c_str(), sprite.Texture->GetWidth(), sprite.Texture->GetHeight());
			}
			else
			{
				ImGui::SetTooltip("Click to pick an image, or drop one from the Project window.");
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			std::filesystem::path droppedPath;
			if (AssetDragDrop::AcceptPayload(droppedPath) && Texture2D::IsSupportedFile(droppedPath))
			{
				picked = Texture2D::LoadFromFile(droppedPath);
			}
			ImGui::EndDragDropTarget();
		}

		// LoadFromFile logs why a file couldn't be loaded; the old texture is kept then.
		if (picked != nullptr)
		{
			sprite.Texture = picked;
		}

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(hasTexture == false);
		if (ImGui::Button("X", { clearWidth, 0.0f }))
		{
			sprite.Texture = nullptr;
		}
		ImGui::EndDisabled();
	}

	// Collapsible section for one component type. Right-click the header to remove it.
	// Removal is only *requested* here; the caller applies it after drawing, so no
	// component reference is held while EnTT storage changes.
	template<typename T, typename UIFunction>
	static void DrawComponent(const char* name, Entity entity, bool& removeRequested, UIFunction uiFunction)
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
		bool removeCamera = false;
		bool removeScripts = false;
		bool makePrimary = false;

		DrawComponent<TransformComponent>("Transform", entity, removeTransform, [](TransformComponent& transform)
		{
			DrawVec3Control("Translation", transform.Translation);

			// Stored in radians, edited in degrees.
			glm::vec3 rotation = glm::degrees(transform.Rotation);
			if (DrawVec3Control("Rotation", rotation, 0.0f))
				transform.Rotation = glm::radians(rotation);

			DrawVec3Control("Scale", transform.Scale, 1.0f);
		});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, removeSprite, [](SpriteRendererComponent& sprite)
		{
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit4("##Color", glm::value_ptr(sprite.Color)); });
			DrawPropertyRow("Texture", [&]() { DrawTextureField(sprite); });
			DrawPropertyRow("Tiling", [&]() { InputFloatField("##Tiling", sprite.TilingFactor, "%.2f", 0.0f, 100.0f); });
		});

		DrawComponent<CameraComponent>("Camera", entity, removeCamera, [&](CameraComponent& cameraComponent)
		{
			SceneCamera& camera = cameraComponent.Camera;

			DrawPropertyRow("Primary", [&]() { makePrimary = ImGui::Checkbox("##Primary", &cameraComponent.Primary) && cameraComponent.Primary; });

			const char* projectionNames[] = { "Perspective", "Orthographic" };
			int projection = (int)camera.Projection;
			DrawPropertyRow("Projection", [&]()
			{
				if (ImGui::Combo("##Projection", &projection, projectionNames, 2))
					camera.Projection = (ProjectionType)projection;
			});

			if (camera.Projection == ProjectionType::Perspective)
			{
				DrawPropertyRow("Vertical FOV", [&]() { InputFloatField("##FOV", camera.PerspectiveFOV, "%.1f deg", 1.0f, 179.0f); });
				DrawPropertyRow("Near", [&]() { InputFloatField("##Near", camera.PerspectiveNear, "%.3f", 0.001f, camera.PerspectiveFar); });
				DrawPropertyRow("Far", [&]() { InputFloatField("##Far", camera.PerspectiveFar, "%.1f", camera.PerspectiveNear, 100000.0f); });
			}
			else
			{
				DrawPropertyRow("Size", [&]() { InputFloatField("##Size", camera.OrthographicSize, "%.2f", 0.01f, 10000.0f); });
				DrawPropertyRow("Near", [&]() { InputFloatField("##Near", camera.OrthographicNear, "%.2f", -10000.0f, camera.OrthographicFar); });
				DrawPropertyRow("Far", [&]() { InputFloatField("##Far", camera.OrthographicFar, "%.2f", camera.OrthographicNear, 10000.0f); });
			}
		});

		int scriptToRemove = -1;
		DrawComponent<NativeScriptComponent>("Scripts", entity, removeScripts, [&](NativeScriptComponent& scripts)
		{
			for (int i = 0; i < (int)scripts.Scripts.size(); i++)
			{
				const NativeScriptComponent::Script& script = scripts.Scripts[i];
				ImGui::PushID(i);

				ImGui::AlignTextToFramePadding();
				if (script.Name.empty())
					ImGui::TextDisabled("(bound in code, not saved)");
				else if (script.Instantiate == nullptr)
					ImGui::TextColored({ 1.0f, 0.45f, 0.35f, 1.0f }, "%s (not registered)", script.Name.c_str());
				else
					ImGui::TextUnformatted(script.Name.c_str());

				if (ImGui::IsItemHovered() && script.Instantiate == nullptr && script.Name.empty() == false)
					ImGui::SetTooltip("No script is registered under this name. It is kept in the scene but doesn't run.");

				ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetFrameHeight());
				if (ImGui::Button("X", { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
					scriptToRemove = i;

				ImGui::PopID();
			}

			if (ImGui::Button("Add Script", { ImGui::GetContentRegionAvail().x, 0.0f }))
				ImGui::OpenPopup("AddScript");

			if (ImGui::BeginPopup("AddScript"))
			{
				DrawScriptMenuItems(entity);
				ImGui::EndPopup();
			}
		});

		// Only one primary camera: ticking Primary here unticks it everywhere else.
		if (makePrimary)
		{
			auto cameras = entity.GetScene()->m_Registry.view<CameraComponent>();
			for (auto other : cameras)
			{
				if (other != (entt::entity)entity)
					cameras.get<CameraComponent>(other).Primary = false;
			}
		}

		// Apply removals after drawing, when no component references are alive.
		if (removeTransform)
			entity.RemoveComponent<TransformComponent>();
		if (removeSprite)
			entity.RemoveComponent<SpriteRendererComponent>();
		if (removeCamera)
			entity.RemoveComponent<CameraComponent>();

		// Scripts that are running (play mode) get OnDestroy before they go.
		if (removeScripts)
			entity.GetScene()->RemoveScripts(entity);
		else if (scriptToRemove >= 0)
			entity.GetScene()->RemoveScript(entity, (size_t)scriptToRemove);
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
			const bool hasCamera = entity.HasComponent<CameraComponent>();

			if (ImGui::MenuItem("Transform", nullptr, false, hasTransform == false))
				entity.AddComponent<TransformComponent>();

			if (ImGui::MenuItem("Sprite Renderer", nullptr, false, hasSprite == false))
			{
				entity.AddComponent<SpriteRendererComponent>();

				// A sprite needs a transform to be drawn (SceneRenderer views both).
				if (entity.HasComponent<TransformComponent>() == false)
					entity.AddComponent<TransformComponent>();
			}

			if (ImGui::MenuItem("Camera", nullptr, false, hasCamera == false))
			{
				// Only the first camera in a scene is primary by default.
				const bool firstCamera = (bool)entity.GetScene()->GetPrimaryCameraEntity() == false;
				entity.AddComponent<CameraComponent>().Primary = firstCamera;

				if (entity.HasComponent<TransformComponent>() == false)
					entity.AddComponent<TransformComponent>();
			}

			if (ImGui::BeginMenu("Script"))
			{
				DrawScriptMenuItems(entity);
				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}
	}

	void InspectorWindow::DrawScriptMenuItems(Entity entity)
	{
		const std::vector<std::string> names = ScriptRegistry::GetNames();
		if (names.empty())
		{
			ImGui::TextDisabled("No scripts registered");
			ImGui::TextDisabled("Register them with ScriptRegistry::Register<T>(\"Name\")");
			return;
		}

		// A script runs once per entity, so ones already added are greyed out. The component is only
		// added when a script is picked.
		const bool hasScripts = entity.HasComponent<NativeScriptComponent>();
		for (const std::string& name : names)
		{
			const bool added = hasScripts && entity.GetComponent<NativeScriptComponent>().Has(name);
			if (ImGui::MenuItem(name.c_str(), nullptr, false, added == false))
			{
				if (hasScripts)
					entity.GetComponent<NativeScriptComponent>().Bind(name);
				else
					entity.AddComponent<NativeScriptComponent>().Bind(name);
				return;
			}
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
