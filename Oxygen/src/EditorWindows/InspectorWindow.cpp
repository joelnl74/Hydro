#include "InspectorWindow.h"
#include "../EditorPaths.h"
#include "Utilities/AssetPicker.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/SpriteAnimation.h>
#include <Hydro/Scene/SpriteAnimationSerializer.h>
#include <Hydro/Scene/SpriteAnimator.h>
#include <Hydro/Scene/ScriptableEntity.h>

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

	// The sprite's texture: pick one of the project's images, or drop one from the Project window; X clears it.
	static void DrawTextureField(SpriteRendererComponent& sprite)
	{
		const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(sprite.Texture);
		const std::string detail = texture != nullptr ? std::to_string(texture->GetWidth()) + " x " + std::to_string(texture->GetHeight()) + " px"
			: (static_cast<uint64_t>(sprite.Texture) != 0 ? "The texture is missing or couldn't be loaded (see the console)." : std::string());

		const AssetPickerResult result = AssetPicker::DrawField("##Texture", sprite.Texture, AssetType::Texture2D, detail);
		if (result.Changed == false)
		{
			return;
		}

		// Loading the texture logs why a file couldn't be loaded; a handle that doesn't load is not taken.
		if (static_cast<uint64_t>(result.Handle) == 0 || AssetManager::GetAsset<Texture2D>(result.Handle) != nullptr)
		{
			sprite.Texture = result.Handle;
		}
	}

	// For a sprite sheet (a Multiple texture): which of its sprites to draw.
	static void DrawSpriteField(SpriteRendererComponent& sprite, const Texture2D& texture)
	{
		const uint32_t current = sprite.SpriteIndex < texture.GetSpriteCount() ? sprite.SpriteIndex : 0;

		char preview[160];
		std::snprintf(preview, sizeof(preview), "%u: %s", current, texture.GetSprite(current).Name.c_str());
		if (ImGui::BeginCombo("##Sprite", preview, ImGuiComboFlags_HeightLarge))
		{
			for (uint32_t i = 0; i < texture.GetSpriteCount(); i++)
			{
				char label[160];
				std::snprintf(label, sizeof(label), "%u: %s", i, texture.GetSprite(i).Name.c_str());
				const bool selected = i == current;
				if (ImGui::Selectable(label, selected))
				{
					sprite.SpriteIndex = i;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			const SpriteRegion& region = texture.GetSprite(current);
			ImGui::SetTooltip("%u x %u px at (%u, %u)\nPivot (%.2f, %.2f)\n%u sprites in this sheet",
				region.Size.x, region.Size.y, region.Position.x, region.Position.y, region.Pivot.x, region.Pivot.y, texture.GetSpriteCount());
		}
	}

	// The animator's .hyanim: pick one of the project's animation sets, or drop one from the Project window; X clears it.
	static void DrawAnimationSetField(Entity entity, SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> set = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		const std::string detail = set != nullptr ? std::to_string(set->GetClipCount()) + " clips"
			: (static_cast<uint64_t>(animator.Animations) != 0 ? "The animation set is missing or couldn't be loaded (see the console)." : std::string());

		const AssetPickerResult result = AssetPicker::DrawField("##Animations", animator.Animations, AssetType::SpriteAnimation, detail);
		if (result.Changed == false)
		{
			return;
		}

		// Loading the set logs why a file couldn't be loaded; a handle that doesn't load is not taken.
		if (static_cast<uint64_t>(result.Handle) == 0 || AssetManager::GetAsset<SpriteAnimationSet>(result.Handle) != nullptr)
		{
			animator.Animations = result.Handle;
			SpriteAnimator::ResetPlayback(entity);
		}
	}

	// The clip played on start: the set's default clip, or one picked by name.
	// Returns true when the choice changed.
	static bool DrawStartClipField(SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> setRef = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		const SpriteAnimationSet* set = setRef.get();
		const std::string defaultLabel = set != nullptr && set->GetClipCount() > 0
			? "Default (" + set->GetClip(set->GetDefaultClip()).Name + ")"
			: std::string("Default");

		bool changed = false;
		const char* preview = animator.StartClip.empty() ? defaultLabel.c_str() : animator.StartClip.c_str();
		ImGui::BeginDisabled(set == nullptr);
		if (ImGui::BeginCombo("##StartClip", preview, ImGuiComboFlags_HeightLarge))
		{
			if (ImGui::Selectable(defaultLabel.c_str(), animator.StartClip.empty()))
			{
				animator.StartClip.clear();
				changed = true;
			}
			for (uint32_t i = 0; set != nullptr && i < set->GetClipCount(); i++)
			{
				const std::string& name = set->GetClip(i).Name;
				if (ImGui::Selectable(name.c_str(), animator.StartClip == name))
				{
					animator.StartClip = name;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();

		if (set != nullptr && animator.StartClip.empty() == false && set->FindClip(animator.StartClip) < 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("'%s' isn't in this set; the default clip plays instead.", animator.StartClip.c_str());
		}
		return changed;
	}

	// Shows the start clip's first frame on the entity's sprite, so the editor (where nothing
	// animates) shows the pose the game starts with.
	static void ShowStartFrame(Entity entity, const SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> setRef = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		if (setRef == nullptr || setRef->GetClipCount() == 0 || entity.HasComponent<SpriteRendererComponent>() == false)
		{
			return;
		}

		const SpriteAnimationSet& set = *setRef;
		const int32_t clipIndex = set.FindStartClip(animator.StartClip);

		const SpriteAnimationClip& clip = set.GetClip((uint32_t)clipIndex);
		if (clip.Frames.empty())
		{
			return;
		}

		SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();
		sprite.Texture = set.GetTexture() != nullptr ? set.GetTexture()->GetHandle() : AssetHandle(0);
		sprite.SpriteIndex = clip.Frames[0].Sprite;
		sprite.FlipX = clip.FlipX;
	}

	// The rows both 2D colliders share: how the surface behaves, and whether it is only a trigger.
	static void DrawColliderRows(PhysicsMaterial2D& material, bool& isSensor)
	{
		DrawPropertyRow("Density", [&]() { InputFloatField("##Density", material.Density, "%.2f", 0.0f, 100000.0f); });
		DrawPropertyRow("Friction", [&]() { InputFloatField("##Friction", material.Friction, "%.2f", 0.0f, 100.0f); });
		DrawPropertyRow("Restitution", [&]() { InputFloatField("##Restitution", material.Restitution, "%.2f", 0.0f, 1.0f); });
		DrawPropertyRow("Is Sensor", [&]()
		{
			ImGui::Checkbox("##IsSensor", &isSensor);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("A sensor detects overlaps (trigger events) but nothing bounces off it.");
			}
		});
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
		bool removeAnimator = false;
		bool removeLight = false;
		bool removeShadow = false;
		bool removeRigidbody = false;
		bool removeBoxCollider = false;
		bool removeCircleCollider = false;
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

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, removeSprite, [&](SpriteRendererComponent& sprite)
		{
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit4("##Color", glm::value_ptr(sprite.Color)); });
			DrawPropertyRow("Texture", [&]() { DrawTextureField(sprite); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(sprite.Texture);
			const bool isSheet = texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple;
			if (isSheet)
			{
				DrawPropertyRow("Sprite", [&]() { DrawSpriteField(sprite, *texture); });
			}
			else
			{
				DrawPropertyRow("Tiling", [&]() { InputFloatField("##Tiling", sprite.TilingFactor, "%.2f", 0.0f, 100.0f); });
			}
			DrawPropertyRow("Flip", [&]()
			{
				ImGui::Checkbox("X##FlipX", &sprite.FlipX);
				ImGui::SameLine();
				ImGui::Checkbox("Y##FlipY", &sprite.FlipY);
			});
			if (texture != nullptr && entity.HasComponent<TransformComponent>())
			{
				DrawPropertyRow("##NativeSize", [&]()
				{
					if (ImGui::Button("Set Native Size", { ImGui::GetContentRegionAvail().x, 0.0f }))
					{
						// Getting another component doesn't move this one, so `sprite` stays valid.
						const glm::vec2 size = texture->GetSpriteSizeInUnits(sprite.SpriteIndex);
						TransformComponent& transform = entity.GetComponent<TransformComponent>();
						transform.Scale.x = size.x;
						transform.Scale.y = size.y;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Scale the entity to the sprite's size: pixels / the texture's Pixels Per Unit.");
					}
				});
			}
			DrawPropertyRow("Sort Order", [&]()
			{
				int sortOrder = (int)sprite.SortOrder;
				if (ImGui::InputInt("##SortOrder", &sortOrder))
				{
					sprite.SortOrder = (int32_t)sortOrder;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Higher values are drawn on top. Equal values: lower Z first.");
				}
			});
		});

		bool showStartFrame = false;
		DrawComponent<SpriteAnimatorComponent>("Sprite Animator", entity, removeAnimator, [&](SpriteAnimatorComponent& animator)
		{
			const AssetHandle before = animator.Animations;
			DrawPropertyRow("Animations", [&]() { DrawAnimationSetField(entity, animator); });
			DrawPropertyRow("Start Clip", [&]() { showStartFrame |= DrawStartClipField(animator); });
			DrawPropertyRow("Play On Start", [&]() { ImGui::Checkbox("##PlayOnStart", &animator.PlayOnStart); });
			DrawPropertyRow("Speed", [&]() { InputFloatField("##Speed", animator.Speed, "%.2f", 0.0f, 100.0f); });
			showStartFrame |= animator.Animations != before;

			// Playback status (play mode): read through SpriteAnimator, never from the component's State.
			const std::string_view currentClip = SpriteAnimator::GetCurrentClip(entity);
			if (currentClip.empty() == false)
			{
				const char* status = SpriteAnimator::IsPlaying(entity) ? "" : (SpriteAnimator::IsFinished(entity) ? " (finished)" : " (stopped)");
				ImGui::TextDisabled("Playing %.*s, frame %u%s", (int)currentClip.size(), currentClip.data(), SpriteAnimator::GetFrameIndex(entity), status);
			}
			if (entity.HasComponent<SpriteRendererComponent>() == false)
			{
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Needs a Sprite Renderer to show anything.");
			}
		});
		// Outside the section's lambda: it reads another component of the entity.
		if (showStartFrame)
		{
			ShowStartFrame(entity, entity.GetComponent<SpriteAnimatorComponent>());
		}

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

		DrawComponent<Light2DComponent>("Light 2D", entity, removeLight, [&](Light2DComponent& light)
		{
			const char* typeNames[] = { "Point", "Global" };
			int type = (int)light.Type;
			DrawPropertyRow("Type", [&]()
			{
				if (ImGui::Combo("##LightType", &type, typeNames, 2))
				{
					light.Type = (Light2DType)type;
				}
			});

			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit3("##LightColor", glm::value_ptr(light.Color)); });
			DrawPropertyRow("Intensity", [&]() { InputFloatField("##LightIntensity", light.Intensity, "%.2f", 0.0f, 100.0f); });

			if (light.Type == Light2DType::Point)
			{
				DrawPropertyRow("Radius", [&]() { InputFloatField("##LightRadius", light.Radius, "%.2f", 0.01f, 10000.0f); });
			}
			else
			{
				ImGui::TextDisabled("Lights everything equally: the ambient color.");
			}
		});

		DrawComponent<Shadow2DComponent>("Shadow 2D", entity, removeShadow, [&](Shadow2DComponent& shadow)
		{
			DrawPropertyRow("Offset X", [&]() { InputFloatField("##ShadowOffsetX", shadow.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Offset Y", [&]() { InputFloatField("##ShadowOffsetY", shadow.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Width", [&]() { InputFloatField("##ShadowWidth", shadow.Size.x, "%.2f", 0.01f, 10000.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##ShadowHeight", shadow.Size.y, "%.2f", 0.01f, 10000.0f); });
			DrawPropertyRow("Strength", [&]() { InputFloatField("##ShadowStrength", shadow.Strength, "%.2f", 0.0f, 1.0f); });
			ImGui::TextDisabled("A blob under the entity. Shows in views that have lights.");
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, removeRigidbody, [&](Rigidbody2DComponent& body)
		{
			const char* typeNames[] = { "Static", "Dynamic", "Kinematic" };
			int type = (int)body.Type;
			DrawPropertyRow("Type", [&]()
			{
				if (ImGui::Combo("##BodyType", &type, typeNames, 3))
				{
					body.Type = (Rigidbody2DType)type;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Static: never moves.\nDynamic: moved by forces, gravity and collisions.\nKinematic: moved only by the game; pushes dynamic bodies.");
				}
			});
			DrawPropertyRow("Gravity Scale", [&]() { InputFloatField("##GravityScale", body.GravityScale, "%.2f", -100.0f, 100.0f); });
			DrawPropertyRow("Linear Damping", [&]() { InputFloatField("##LinearDamping", body.LinearDamping, "%.2f", 0.0f, 1000.0f); });
			DrawPropertyRow("Angular Damping", [&]() { InputFloatField("##AngularDamping", body.AngularDamping, "%.2f", 0.0f, 1000.0f); });
			DrawPropertyRow("Fixed Rotation", [&]() { ImGui::Checkbox("##FixedRotation", &body.FixedRotation); });
			DrawPropertyRow("Bullet", [&]()
			{
				ImGui::Checkbox("##IsBullet", &body.IsBullet);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Continuous collision for fast, small bodies such as projectiles. Costs more.");
				}
			});
		});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, removeBoxCollider, [&](BoxCollider2DComponent& collider)
		{
			DrawPropertyRow("Offset X", [&]() { InputFloatField("##BoxOffsetX", collider.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Offset Y", [&]() { InputFloatField("##BoxOffsetY", collider.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Width", [&]() { InputFloatField("##BoxWidth", collider.Size.x, "%.2f", 0.001f, 10000.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##BoxHeight", collider.Size.y, "%.2f", 0.001f, 10000.0f); });
			DrawColliderRows(collider.Material, collider.IsSensor);
			if (entity.HasComponent<Rigidbody2DComponent>() == false)
			{
				ImGui::TextDisabled("No Rigidbody 2D: this is a static collider.");
			}
		});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, removeCircleCollider, [&](CircleCollider2DComponent& collider)
		{
			DrawPropertyRow("Offset X", [&]() { InputFloatField("##CircleOffsetX", collider.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Offset Y", [&]() { InputFloatField("##CircleOffsetY", collider.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Radius", [&]() { InputFloatField("##CircleRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawColliderRows(collider.Material, collider.IsSensor);
			if (entity.HasComponent<Rigidbody2DComponent>() == false)
			{
				ImGui::TextDisabled("No Rigidbody 2D: this is a static collider.");
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
		if (removeAnimator)
		{
			entity.RemoveComponent<SpriteAnimatorComponent>();
		}
		if (removeLight)
		{
			entity.RemoveComponent<Light2DComponent>();
		}
		if (removeShadow)
		{
			entity.RemoveComponent<Shadow2DComponent>();
		}
		if (removeRigidbody)
		{
			entity.RemoveComponent<Rigidbody2DComponent>();
		}
		if (removeBoxCollider)
		{
			entity.RemoveComponent<BoxCollider2DComponent>();
		}
		if (removeCircleCollider)
		{
			entity.RemoveComponent<CircleCollider2DComponent>();
		}

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
			const bool hasAnimator = entity.HasComponent<SpriteAnimatorComponent>();
			const bool hasLight = entity.HasComponent<Light2DComponent>();
			const bool hasShadow = entity.HasComponent<Shadow2DComponent>();
			const bool hasRigidbody = entity.HasComponent<Rigidbody2DComponent>();
			const bool hasBoxCollider = entity.HasComponent<BoxCollider2DComponent>();
			const bool hasCircleCollider = entity.HasComponent<CircleCollider2DComponent>();

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

			if (ImGui::MenuItem("Sprite Animator", nullptr, false, hasAnimator == false))
			{
				entity.AddComponent<SpriteAnimatorComponent>();

				// It animates the entity's sprite, which needs a transform to be drawn.
				if (entity.HasComponent<SpriteRendererComponent>() == false)
				{
					entity.AddComponent<SpriteRendererComponent>();
				}
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::MenuItem("Light 2D", nullptr, false, hasLight == false))
			{
				entity.AddComponent<Light2DComponent>();

				// A point light is placed by the entity's transform.
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::MenuItem("Shadow 2D", nullptr, false, hasShadow == false))
			{
				entity.AddComponent<Shadow2DComponent>();

				// The blob is placed by the entity's transform.
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::BeginMenu("Physics 2D"))
			{
				if (ImGui::MenuItem("Rigidbody 2D", nullptr, false, hasRigidbody == false))
				{
					entity.AddComponent<Rigidbody2DComponent>();

					// A body is placed by the entity's transform.
					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				}

				if (ImGui::MenuItem("Box Collider 2D", nullptr, false, hasBoxCollider == false))
				{
					entity.AddComponent<BoxCollider2DComponent>();

					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				}

				if (ImGui::MenuItem("Circle Collider 2D", nullptr, false, hasCircleCollider == false))
				{
					entity.AddComponent<CircleCollider2DComponent>();

					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				}

				ImGui::EndMenu();
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

		ImGui::TextUnformatted(asset.filename().u8string().c_str());
		ImGui::TextDisabled("Nothing to edit for this kind of file.");
	}

	void InspectorWindow::DrawAnimationSetAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		// Read again only when another file is selected or this one was saved (a file that fails to
		// load would otherwise log every frame).
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

	void InspectorWindow::DrawPrefabButton(Entity entity)
	{
		const std::filesystem::path path = EditorPaths::GetPrefabPath(entity.GetName());

		if (ImGui::Button("Save as Prefab", { ImGui::GetContentRegionAvail().x, 0.0f }))
			PrefabSerializer::Save(entity, path);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", path.string().c_str());
	}
}
