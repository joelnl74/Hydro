#include "ComponentDrawers.h"
#include "InspectorUI.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Font.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/Scene.h>
#include <Hydro/Scene/UI/UI.h>
#include <Hydro/Scene/UI/UILayout.h>
#include <Hydro/Scene/UI/UIInteraction.h>

#include <glm/gtc/type_ptr.hpp>

#include <string>

namespace Hydro
{
	// The canvas the entity is drawn on: the nearest ancestor with a UICanvasComponent, the entity included.
	// `nested` is set when that canvas is inside another one, which isn't supported.
	static Entity FindUICanvas(Entity entity, bool& nested)
	{
		nested = false;
		Scene* scene = entity.GetScene();
		Entity canvas;
		for (Entity current = entity; (bool)current; current = scene->GetParent(current))
		{
			if (current.HasComponent<UICanvasComponent>())
			{
				if ((bool)canvas)
				{
					nested = true;
					break;
				}
				canvas = current;
			}
		}
		return canvas;
	}

	// One axis of the anchor presets: Unity's left / centre / right / stretch (min, max).
	struct UIAnchorPreset
	{
		const char* Name;
		float Min;
		float Max;
	};

	static const UIAnchorPreset s_HorizontalAnchors[4] = { { "Left", 0.0f, 0.0f }, { "Center", 0.5f, 0.5f }, { "Right", 1.0f, 1.0f },
		{ "Stretch", 0.0f, 1.0f } };
	static const UIAnchorPreset s_VerticalAnchors[4] = { { "Top", 1.0f, 1.0f }, { "Middle", 0.5f, 0.5f }, { "Bottom", 0.0f, 0.0f },
		{ "Stretch", 0.0f, 1.0f } };

	// The preset an axis matches, or -1 (custom anchors).
	static int FindAnchorPreset(const UIAnchorPreset (&presets)[4], float min, float max)
	{
		for (int i = 0; i < 4; i++)
		{
			if (std::abs(presets[i].Min - min) < 1.0e-4f && std::abs(presets[i].Max - max) < 1.0e-4f)
			{
				return i;
			}
		}
		return -1;
	}

	// One axis of a preset onto the rect. A stretched axis fills the parent. With `setPivot` (Alt) the pivot goes to
	// the anchors too and the element moves onto them.
	static void ApplyAnchorPreset(UIRectComponent& rect, int axis, const UIAnchorPreset& preset, bool setPivot)
	{
		rect.AnchorMin[axis] = preset.Min;
		rect.AnchorMax[axis] = preset.Max;
		const bool stretch = preset.Max > preset.Min;
		if (stretch)
		{
			rect.SizeDelta[axis] = 0.0f;
			rect.AnchoredPosition[axis] = 0.0f;
		}
		if (setPivot)
		{
			rect.Pivot[axis] = stretch ? 0.5f : preset.Min;
			rect.AnchoredPosition[axis] = 0.0f;
		}
	}

	// A button showing the rect's anchors that opens the 4 x 4 grid of presets. Hold Alt to set the pivot as well.
	static void DrawAnchorPresets(UIRectComponent& rect)
	{
		const int horizontal = FindAnchorPreset(s_HorizontalAnchors, rect.AnchorMin.x, rect.AnchorMax.x);
		const int vertical = FindAnchorPreset(s_VerticalAnchors, rect.AnchorMin.y, rect.AnchorMax.y);
		char current[64];
		if (horizontal >= 0 && vertical >= 0)
		{
			std::snprintf(current, sizeof(current), "%s %s", s_VerticalAnchors[vertical].Name, s_HorizontalAnchors[horizontal].Name);
		}
		else
		{
			std::snprintf(current, sizeof(current), "Custom");
		}

		if (ImGui::Button(current, { -1.0f, 0.0f }))
		{
			ImGui::OpenPopup("AnchorPresets");
		}
		if (ImGui::BeginPopup("AnchorPresets"))
		{
			ImGui::TextDisabled("Anchor presets (hold Alt to set the pivot too)");
			const bool setPivot = ImGui::GetIO().KeyAlt;
			const float cell = ImGui::GetFrameHeight() * 2.0f;
			for (int row = 0; row < 4; row++)
			{
				for (int column = 0; column < 4; column++)
				{
					const UIAnchorPreset& h = s_HorizontalAnchors[column];
					const UIAnchorPreset& v = s_VerticalAnchors[row];
					char label[32];
					std::snprintf(label, sizeof(label), "%c%c##preset%d", v.Name[0], h.Name[0], row * 4 + column);
					const bool selected = row == vertical && column == horizontal;
					if (selected)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
					}
					if (ImGui::Button(label, { cell, cell }))
					{
						ApplyAnchorPreset(rect, 0, h, setPivot);
						ApplyAnchorPreset(rect, 1, v, setPivot);
						ImGui::CloseCurrentPopup();
					}
					if (selected)
					{
						ImGui::PopStyleColor();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s, %s", v.Name, h.Name);
					}
					if (column < 3)
					{
						ImGui::SameLine();
					}
				}
			}
			ImGui::EndPopup();
		}
	}

	// A button's explicit neighbour: one of the other buttons on the same canvas, by name; (none) clears it.
	static void DrawButtonReferenceField(const char* id, Entity entity, UUID& reference)
	{
		Scene& scene = *entity.GetScene();
		bool nested = false;
		const Entity canvas = UI::FindCanvas(entity, nested);
		Entity current = (uint64_t)reference != 0 ? scene.GetEntityByUUID(reference) : Entity{};
		const std::string preview = (bool)current ? current.GetName()
			: ((uint64_t)reference != 0 ? std::string("(missing)") : std::string("(none)"));
		if (ImGui::BeginCombo(id, preview.c_str()))
		{
			if (ImGui::Selectable("(none)", (uint64_t)reference == 0))
			{
				reference = 0;
			}
			auto buttons = scene.m_Registry.view<UIButtonComponent>();
			for (entt::entity handle : buttons)
			{
				Entity other{ handle, &scene };
				bool otherNested = false;
				if (other == entity || other.HasComponent<IDComponent>() == false || UI::FindCanvas(other, otherNested) != canvas)
				{
					continue;
				}
				ImGui::PushID((int)(uint32_t)handle);
				if (ImGui::Selectable(other.GetName().c_str(), other == current))
				{
					reference = other.GetUUID();
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
	}

	static void DrawUICanvas(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UICanvasComponent>("UI Canvas", entity, context, [&](UICanvasComponent& canvas)
		{
			InspectorUI::DrawVector("Reference", glm::value_ptr(canvas.ReferenceResolution), 2, 0.0f, 1.0f, 16384.0f);
			InspectorUI::Row("Scale Mode", [&]()
			{
				const char* modes[] = { "Integer scale", "Scale with screen", "Constant pixel size" };
				int mode = (int)canvas.ScaleMode;
				if (ImGui::Combo("##ScaleMode", &mode, modes, 3))
				{
					canvas.ScaleMode = (UIScaleMode)mode;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Integer scale: the largest whole multiple of the reference that fits, at least 1 (pixel UI stays crisp);\nthe leftover space widens the canvas, and a view smaller than the reference crops it.\nScale with screen: a fractional scale that fits the reference (smooth UI).\nConstant pixel size: one canvas pixel per view pixel.");
				}
			});
			InspectorUI::Row("Space", [&]()
			{
				const char* spaces[] = { "Screen", "World" };
				int space = (int)canvas.Space;
				if (ImGui::Combo("##CanvasSpace", &space, spaces, 2))
				{
					canvas.Space = (UICanvasSpace)space;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Screen: covers the view.\nWorld: a box of World Size canvas pixels pinned to this entity's position (where the\nprimary camera sees it), the same size on screen at any distance: labels over characters, prompts.\nGive the entity a Transform, or put it under the character it follows.");
				}
			});
			if (canvas.Space == UICanvasSpace::World)
			{
				InspectorUI::DrawVector("World Size", glm::value_ptr(canvas.WorldSize), 2, 100.0f, 1.0f, 16384.0f);
				InspectorUI::DrawVector("World Pivot", glm::value_ptr(canvas.WorldPivot), 2, 0.5f, 0.0f, 1.0f);
			}
			InspectorUI::Row("Sort Order", [&]() { ImGui::InputInt("##CanvasSortOrder", &canvas.SortOrder); });
			InspectorUI::Row("Visible", [&]() { ImGui::Checkbox("##CanvasVisible", &canvas.Visible); });
			InspectorUI::Row("Blocks Input", [&]()
			{
				ImGui::Checkbox("##CanvasBlocksInput", &canvas.BlocksGameplayInput);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("While the canvas is visible, gameplay scripts don't get keyboard input (menus, dialogue).");
				}
			});
			InspectorUI::Row("Draw Debug", [&]() { ImGui::Checkbox("##CanvasDrawDebug", &canvas.DrawDebug); });

			Scene* scene = entity.GetScene();
			const glm::vec2 viewSize{ (float)scene->GetViewportWidth(), (float)scene->GetViewportHeight() };
			UICanvasMetrics metrics;
			if (scene->GetUICanvasMetrics(entity.GetUUID(), metrics) == false)
			{
				metrics = UILayout::GetCanvasMetrics(canvas, viewSize);
				if (canvas.Space == UICanvasSpace::World)
				{
					metrics.Size = canvas.WorldSize;
				}
			}
			ImGui::TextDisabled("Game view %.0f x %.0f: canvas %.0f x %.0f at x%.2f", viewSize.x, viewSize.y, metrics.Size.x,
				metrics.Size.y, metrics.Scale);
			bool nested = false;
			FindUICanvas(entity, nested);
			if (nested)
			{
				ImGui::TextWrapped("This canvas is inside another canvas. Nested canvases aren't supported: it isn't drawn.");
			}
		});
	}

	static void DrawUIRect(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UIRectComponent>("UI Rect", entity, context, [&](UIRectComponent& rect)
		{
			// Under a layout group only the size counts: the group places the element.
			const Entity parent = entity.GetScene()->GetParent(entity);
			const bool driven = (bool)parent && parent.HasComponent<UILayoutGroupComponent>() && parent.HasComponent<UIRectComponent>();
			if (driven)
			{
				ImGui::TextWrapped("Driven by layout group %s: it places this element; only Size (along the stack) and Visible count.",
					parent.GetName().c_str());
			}
			ImGui::BeginDisabled(driven);
			InspectorUI::Row("Anchors", [&]() { DrawAnchorPresets(rect); });
			InspectorUI::DrawVector("Position", glm::value_ptr(rect.AnchoredPosition), 2, 0.0f);
			ImGui::EndDisabled();
			// Added to the anchors' span: the size itself when the anchors meet, a margin on a stretched axis.
			InspectorUI::DrawVector("Size", glm::value_ptr(rect.SizeDelta), 2, 100.0f);
			ImGui::BeginDisabled(driven);
			InspectorUI::DrawVector("Anchor Min", glm::value_ptr(rect.AnchorMin), 2, 0.5f, 0.0f, 1.0f);
			InspectorUI::DrawVector("Anchor Max", glm::value_ptr(rect.AnchorMax), 2, 0.5f, 0.0f, 1.0f);
			InspectorUI::DrawVector("Pivot", glm::value_ptr(rect.Pivot), 2, 0.5f);
			ImGui::EndDisabled();
			InspectorUI::Row("Visible", [&]() { ImGui::Checkbox("##RectVisible", &rect.Visible); });

			bool nested = false;
			const Entity canvas = FindUICanvas(entity, nested);
			if ((bool)canvas == false)
			{
				ImGui::TextWrapped("No canvas above this element: it isn't drawn. Put it under an entity with a UI Canvas.");
			}
			else if (nested)
			{
				ImGui::TextWrapped("Its canvas is inside another canvas. Nested canvases aren't supported: it isn't drawn.");
			}
			else
			{
				ImGui::TextDisabled("Canvas pixels, Y up, on %s. Size adds to the anchors' span.", canvas.GetName().c_str());
			}
#ifdef HY_DEBUG
			if (entity.HasComponent<UIResolvedRectComponent>())
			{
				const UIResolvedRectComponent& resolved = entity.GetComponent<UIResolvedRectComponent>();
				ImGui::TextDisabled("Resolved (%.0f, %.0f) - (%.0f, %.0f)%s", resolved.Min.x, resolved.Min.y, resolved.Max.x,
					resolved.Max.y, resolved.Visible ? "" : ", hidden");
			}
#endif
		});
	}

	static void DrawUIImage(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UIImageComponent>("UI Image", entity, context, [&](UIImageComponent& image)
		{
			InspectorUI::Row("Texture", [&]() { InspectorUI::TextureField("##UIImageTexture", image.Texture); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(image.Texture);
			if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
			{
				InspectorUI::Row("Sprite", [&]() { InspectorUI::SpriteField(image.SpriteIndex, *texture); });
			}
			InspectorUI::Row("Color", [&]() { ImGui::ColorEdit4("##UIImageColor", glm::value_ptr(image.Color)); });
			InspectorUI::Row("Slice Left", [&]() { InspectorUI::InputFloat("##SliceLeft", image.NineSlice.x, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Slice Bottom",
				[&]() { InspectorUI::InputFloat("##SliceBottom", image.NineSlice.y, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Slice Right",
				[&]() { InspectorUI::InputFloat("##SliceRight", image.NineSlice.z, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Slice Top", [&]() { InspectorUI::InputFloat("##SliceTop", image.NineSlice.w, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Flip", [&]()
			{
				ImGui::Checkbox("X##UIImageFlipX", &image.FlipX);
				ImGui::SameLine();
				ImGui::Checkbox("Y##UIImageFlipY", &image.FlipY);
			});
			ImGui::TextDisabled(UILayout::IsNineSliced(image) ? "Nine-sliced: the borders (texture pixels) keep their size."
				: "Stretched over the rect (set slice borders for a nine-slice frame).");
		});
	}

	static void DrawUIText(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UITextComponent>("UI Text", entity, context, [&](UITextComponent& text)
		{
			InspectorUI::Row("Font", [&]() { InspectorUI::FontField("##UITextFont", text.Font); });
			InspectorUI::Row("Text", [&]()
			{
				ImGui::InputTextMultiline("##UIText", text.Text.data(), text.Text.capacity() + 1,
					{ -1.0f, ImGui::GetTextLineHeight() * 4.0f },
					ImGuiInputTextFlags_CallbackResize, InspectorUI::StringResizeCallback, &text.Text);
			});
			InspectorUI::Row("Color", [&]() { ImGui::ColorEdit4("##UITextColor", glm::value_ptr(text.Color)); });
			InspectorUI::Row("Scale", [&]()
			{
				int scale = (int)text.Scale;
				if (ImGui::InputInt("##UITextScale", &scale))
				{
					text.Scale = (uint32_t)std::clamp(scale, 1, 64);
				}
			});
			InspectorUI::Row("Align", [&]()
			{
				const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
				const char* horizontal[] = { "Left", "Center", "Right" };
				const char* vertical[] = { "Top", "Middle", "Bottom" };
				int alignH = (int)text.AlignH;
				int alignV = (int)text.AlignV;
				ImGui::SetNextItemWidth(half);
				if (ImGui::Combo("##UITextAlignH", &alignH, horizontal, 3))
				{
					text.AlignH = (TextAlignH)alignH;
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(half);
				if (ImGui::Combo("##UITextAlignV", &alignV, vertical, 3))
				{
					text.AlignV = (TextAlignV)alignV;
				}
			});
			InspectorUI::Row("Wrap", [&]() { ImGui::Checkbox("##UITextWrap", &text.Wrap); });
			InspectorUI::Row("Line Spacing",
				[&]() { InspectorUI::InputFloat("##UITextLineSpacing", text.LineSpacing, "%.2f", 0.0f, 10.0f); });
			InspectorUI::Row("Shadow", [&]()
			{
				ImGui::ColorEdit4("##UITextShadow", glm::value_ptr(text.ShadowColor));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("A drop shadow under the glyphs (alpha 0 = none): readable text over a busy scene.");
				}
			});
			if (text.ShadowColor.a > 0.0f)
			{
				InspectorUI::DrawVector("Shadow Offset", glm::value_ptr(text.ShadowOffset), 2, 0.0f);
			}
			InspectorUI::Row("Visible Chars", [&]()
			{
				int visible = (int)text.VisibleCharacters;
				if (ImGui::InputInt("##UITextVisible", &visible))
				{
					text.VisibleCharacters = std::max(visible, -1);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("-1 shows all of it; a typewriter counts this up without the text moving.");
				}
			});
			const Ref<Font> font = AssetManager::GetLoadedAsset<Font>(text.Font);
			if (font == nullptr)
			{
				ImGui::TextDisabled("Pick a font (.fnt) to draw it.");
			}
			else if (entity.HasComponent<UIResolvedRectComponent>())
			{
				// What the Game view draws, at the rect's current width.
				const UIResolvedRectComponent& rect = entity.GetComponent<UIResolvedRectComponent>();
				TextLayoutProperties settings;
				settings.Scale = text.Scale;
				settings.BoxWidth = rect.Max.x - rect.Min.x;
				settings.Wrap = text.Wrap;
				settings.LineSpacing = text.LineSpacing;
				TextLayoutResult layout;
				TextLayout::Layout(text.Text, font->GetData(), settings, layout);
				ImGui::TextDisabled("%u lines, %u characters, %.0f px tall (rect %.0f)", layout.LineCount, layout.CharacterCount,
					layout.Size.y, rect.Max.y - rect.Min.y);
			}
		});
	}

	static void DrawUILayoutGroup(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UILayoutGroupComponent>("UI Layout Group", entity, context, [&](UILayoutGroupComponent& group)
		{
			InspectorUI::Row("Direction", [&]()
			{
				const char* directions[] = { "Vertical", "Horizontal" };
				int direction = (int)group.Direction;
				if (ImGui::Combo("##GroupDirection", &direction, directions, 2))
				{
					group.Direction = (UILayoutDirection)direction;
				}
			});
			InspectorUI::Row("Spacing", [&]() { InspectorUI::InputFloat("##GroupSpacing", group.Spacing, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Padding L", [&]() { InspectorUI::InputFloat("##GroupPadLeft", group.Padding.x, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Padding B",
				[&]() { InspectorUI::InputFloat("##GroupPadBottom", group.Padding.y, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Padding R", [&]() { InspectorUI::InputFloat("##GroupPadRight", group.Padding.z, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Padding T", [&]() { InspectorUI::InputFloat("##GroupPadTop", group.Padding.w, "%.0f px", 0.0f, 4096.0f); });
			InspectorUI::Row("Alignment", [&]()
			{
				const char* alignments[] = { "Start (top / left)", "Center", "End (bottom / right)" };
				int alignment = (int)group.ChildAlignment;
				if (ImGui::Combo("##GroupAlignment", &alignment, alignments, 3))
				{
					group.ChildAlignment = (UILayoutAlignment)alignment;
				}
			});
			InspectorUI::Row("Stretch", [&]()
			{
				ImGui::Checkbox("##GroupStretch", &group.StretchCrossAxis);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Children take the content's width (vertical) or height (horizontal).");
				}
			});
			InspectorUI::Row("Fit Content", [&]()
			{
				ImGui::Checkbox("##GroupFit", &group.FitContent);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The group's own length along the stack follows its children; its pivot stays put.");
				}
			});
			ImGui::TextDisabled("Stacks the visible UI children; their own rects are left as authored.");
		});
	}

	static void DrawUIScrollView(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UIScrollViewComponent>("UI Scroll View", entity, context, [&](UIScrollViewComponent& scroll)
		{
			InspectorUI::Row("Scroll",
				[&]() { InspectorUI::InputFloat("##ScrollPosition", scroll.ScrollPosition, "%.0f px", 0.0f, 100000.0f); });
			InspectorUI::Row("Follow Focus", [&]()
			{
				ImGui::Checkbox("##ScrollFollowFocus", &scroll.FollowFocus);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Scrolls so the focused button inside it is always whole in view.");
				}
			});
			InspectorUI::Row("Scrollbar", [&]()
			{
				// Any element of the canvas outside this view: the thumb, in its parent (the track).
				Scene& scene = *entity.GetScene();
				bool nested = false;
				const Entity canvas = UI::FindCanvas(entity, nested);
				Entity current = (uint64_t)scroll.Scrollbar != 0 ? scene.GetEntityByUUID(scroll.Scrollbar) : Entity{};
				const std::string preview = (bool)current ? current.GetName()
					: ((uint64_t)scroll.Scrollbar != 0 ? std::string("(missing)") : std::string("(none)"));
				if (ImGui::BeginCombo("##ScrollScrollbar", preview.c_str()))
				{
					if (ImGui::Selectable("(none)", (uint64_t)scroll.Scrollbar == 0))
					{
						scroll.Scrollbar = 0;
					}
					auto rects = scene.m_Registry.view<UIRectComponent>();
					for (entt::entity handle : rects)
					{
						Entity other{ handle, &scene };
						bool otherNested = false;
						if (other == entity || other.HasComponent<IDComponent>() == false || UI::FindCanvas(other, otherNested) != canvas
							|| scene.IsAncestorOf(entity, other))
						{
							continue;
						}
						ImGui::PushID((int)(uint32_t)handle);
						if (ImGui::Selectable(other.GetName().c_str(), other == current))
						{
							scroll.Scrollbar = other.GetUUID();
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
			});
			ImGui::TextDisabled("Moves its children up by Scroll and cuts everything below it to its rect.");
		});
	}

	static void DrawUIFocusVisibility(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UIFocusVisibilityComponent>("UI Focus Visibility", entity, context, [&](UIFocusVisibilityComponent& visibility)
		{
			InspectorUI::Row("Show", [&]()
			{
				const char* modes[] = { "While focused", "While not focused" };
				int mode = (int)visibility.Show;
				if (ImGui::Combo("##FocusVisibilityShow", &mode, modes, 2))
				{
					visibility.Show = (UIFocusVisibility)mode;
				}
			});
			ImGui::TextDisabled("By the state of its button (itself or the nearest one above it): a cursor, a highlight.");
		});
	}

	static void DrawUIButton(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<UIButtonComponent>("UI Button", entity, context, [&](UIButtonComponent& button)
		{
			InspectorUI::Row("Interactable", [&]() { ImGui::Checkbox("##ButtonInteractable", &button.Interactable); });
			InspectorUI::Row("Event", [&]() { InspectorUI::InputText("##ButtonEvent", button.Event); });
			InspectorUI::Row("Normal", [&]() { ImGui::ColorEdit4("##ButtonNormal", glm::value_ptr(button.NormalTint)); });
			InspectorUI::Row("Focused", [&]() { ImGui::ColorEdit4("##ButtonFocused", glm::value_ptr(button.FocusedTint)); });
			InspectorUI::Row("Pressed", [&]() { ImGui::ColorEdit4("##ButtonPressed", glm::value_ptr(button.PressedTint)); });
			InspectorUI::Row("Disabled", [&]() { ImGui::ColorEdit4("##ButtonDisabled", glm::value_ptr(button.DisabledTint)); });
			InspectorUI::Row("Navigation", [&]()
			{
				const char* modes[] = { "Auto", "Explicit", "None" };
				int mode = (int)button.Navigation;
				if (ImGui::Combo("##ButtonNavigation", &mode, modes, 3))
				{
					button.Navigation = (UINavigation)mode;
				}
			});
			if (button.Navigation == UINavigation::Explicit)
			{
				InspectorUI::Row("Up", [&]() { DrawButtonReferenceField("##ButtonUp", entity, button.Up); });
				InspectorUI::Row("Down", [&]() { DrawButtonReferenceField("##ButtonDown", entity, button.Down); });
				InspectorUI::Row("Left", [&]() { DrawButtonReferenceField("##ButtonLeft", entity, button.Left); });
				InspectorUI::Row("Right", [&]() { DrawButtonReferenceField("##ButtonRight", entity, button.Right); });
			}

			Scene& scene = *entity.GetScene();
			if (const UIInteraction* ui = scene.GetUIInteraction())
			{
				const char* states[] = { "Normal", "Focused", "Pressed", "Disabled" };
				ImGui::TextDisabled("Now: %s", states[(int)ui->GetState(scene, entity)]);
			}
			else
			{
				ImGui::TextDisabled("Arrows / WASD move the focus, Enter / Space / Z press, Esc / Backspace / X cancel.");
			}
		});
	}

	void ComponentDrawers::AddUIDrawers(std::vector<ComponentDrawer>& drawers)
	{
		drawers.push_back(Make<UICanvasComponent>("UI Canvas", &DrawUICanvas, &AddOnly<UICanvasComponent>, "UI", "Canvas"));
		drawers.push_back(Make<UIRectComponent>("UI Rect", &DrawUIRect, &AddOnly<UIRectComponent>, "UI", "Rect"));
		drawers.push_back(Make<UIImageComponent>("UI Image", &DrawUIImage, &AddWithRect<UIImageComponent>, "UI", "Image"));
		drawers.push_back(Make<UITextComponent>("UI Text", &DrawUIText, &AddWithRect<UITextComponent>, "UI", "Text"));
		drawers.push_back(Make<UILayoutGroupComponent>("UI Layout Group", &DrawUILayoutGroup,
			&AddWithRect<UILayoutGroupComponent>, "UI", "Layout Group"));
		drawers.push_back(Make<UIScrollViewComponent>("UI Scroll View", &DrawUIScrollView,
			&AddWithRect<UIScrollViewComponent>, "UI", "Scroll View"));
		drawers.push_back(Make<UIFocusVisibilityComponent>("UI Focus Visibility", &DrawUIFocusVisibility,
			&AddWithRect<UIFocusVisibilityComponent>, "UI", "Focus Visibility"));
		drawers.push_back(Make<UIButtonComponent>("UI Button", &DrawUIButton, &AddWithRect<UIButtonComponent>, "UI", "Button"));
	}
}
