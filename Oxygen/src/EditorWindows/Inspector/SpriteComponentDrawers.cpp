#include "ComponentDrawers.h"
#include "InspectorUI.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/SpriteAnimation.h>
#include <Hydro/Scene/SpriteAnimator.h>
#include "EditorWindows/PropertyUI.h"
#include "Utilities/AssetPicker.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <string>

namespace Hydro
{
	// A FloatRange as two fields, Min and Max, clamped to [min, max] when left; Max never ends up below Min.
	static void DrawRangeField(const char* id, FloatRange& range, float min, float max, const char* format = "%.2f")
	{
		ImGui::PushID(id);
		const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		ImGui::SetNextItemWidth(half);
		InspectorUI::InputFloat("##Min", range.Min, format, min, max);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Min: each particle picks a value between Min and Max.");
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		InspectorUI::InputFloat("##Max", range.Max, format, min, max);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Max");
		}
		if (ImGui::IsAnyItemActive() == false && range.Max < range.Min)
		{
			std::swap(range.Min, range.Max);
		}
		ImGui::PopID();
	}

	// A small heading between groups of rows in a long component section.
	static void DrawSubheading(const char* text)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", text);
		ImGui::Separator();
	}

	// The animator's .hyanim: pick one of the project's animation sets, or drop one from the Project window; X clears it.
	static void DrawAnimationSetField(Entity entity, SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> set = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		const std::string detail = set != nullptr ? std::to_string(set->GetClipCount()) + " clips"
			: (static_cast<uint64_t>(animator.Animations) != 0 ? "The animation set is missing or couldn't be loaded (see the console)."
				: std::string());

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

	// The clip played on start: the set's default clip, or one picked by name. True when the choice changed.
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

	// Shows the start clip's first frame on the entity's sprite, so the editor shows the pose the game starts with.
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

	static void DrawSpriteRenderer(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<SpriteRendererComponent>("Sprite Renderer", entity, context, [&](SpriteRendererComponent& sprite)
		{
			InspectorUI::Row("Color", [&]() { ImGui::ColorEdit4("##Color", glm::value_ptr(sprite.Color)); });
			InspectorUI::Row("Texture", [&]() { InspectorUI::TextureField("##Texture", sprite.Texture); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(sprite.Texture);
			const bool isSheet = texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple;
			if (isSheet)
			{
				InspectorUI::Row("Sprite", [&]() { InspectorUI::SpriteField(sprite.SpriteIndex, *texture); });
			}
			else
			{
				InspectorUI::Row("Tiling", [&]() { InspectorUI::InputFloat("##Tiling", sprite.TilingFactor, "%.2f", 0.0f, 100.0f); });
			}
			InspectorUI::Row("Flip", [&]()
			{
				ImGui::Checkbox("X##FlipX", &sprite.FlipX);
				ImGui::SameLine();
				ImGui::Checkbox("Y##FlipY", &sprite.FlipY);
			});
			InspectorUI::Row("Facing", [&]()
			{
				const char* facingNames[] = { "Flat", "Upright", "Billboard" };
				int facing = (int)sprite.Facing;
				if (ImGui::Combo("##Facing", &facing, facingNames, 3))
				{
					sprite.Facing = (SpriteFacing)facing;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How the sprite turns towards a tilted camera.\nFlat: lies on the ground (roads, decals, water).\nUpright: stands on its bottom edge and turns to the camera (trees, houses, characters);\nRender Settings > Upright sprites decides whether it stands vertical or leans with the camera.\nBillboard: faces the camera about its centre (glows, icons, world-space UI).\nA top-down camera draws all three the same.");
				}
			});
			InspectorUI::Row("Alpha", [&]()
			{
				const char* alphaNames[] = { "Default", "Cut-out", "Translucent", "Additive" };
				int alpha = (int)sprite.AlphaMode;
				if (ImGui::Combo("##AlphaMode", &alpha, alphaNames, 4))
				{
					sprite.AlphaMode = (SpriteAlphaMode)alpha;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How the sprite is drawn with shared depth (Render Settings > Rendering).\nDefault: the render features' default sprite alpha.\nCut-out: half-transparent pixels are dropped, the rest hide what is behind (trees, characters).\nTranslucent: blended and sorted back to front (glows, water, smoke).\nAdditive: adds light onto what is behind, unlit (light shafts, glows); Emission makes it bloom.\nWithout shared depth every sprite is translucent, except additive ones.");
				}
			});
			InspectorUI::Row("Overlay", [&]()
			{
				ImGui::Checkbox("##Overlay", &sprite.Overlay);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("HUD sprite: drawn last, on top of everything, unlit and never hidden by depth\n(world-space prompts and menus).");
				}
			});
			InspectorUI::Row("Cast shadows", [&]()
			{
				ImGui::Checkbox("##SpriteCastShadows", &sprite.CastShadows);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Sun shadows (Render Settings > Rendering: Shadows = Sun): a cut-out Upright or Billboard\nsprite casts one, as set by Sprite shadows. Off for grass and flowers.");
				}
			});
			InspectorUI::Row("Emission", [&]()
			{
				ImGui::ColorEdit3("##SpriteEmissionColor", glm::value_ptr(sprite.EmissionColor));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Multiplies the sprite's own colours (white = the texture's colours) for its glow.");
				}
			});
			InspectorUI::Row("Emission intensity", [&]()
			{
				InspectorUI::InputFloat("##SpriteEmissionIntensity", sprite.EmissionIntensity, "%.2f", 0.0f, 100.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The sprite glows: its colours times Emission times this are added after the lighting,\nso it stays bright at night and in shadow (flames, candles, lit windows). 0 = none.\nAbove about 1 it blooms (Render Settings > Rendering: HDR and Bloom). The whole sprite glows:\ngive a torch's flame its own sprite.");
				}
			});
			InspectorUI::Row("Opacity", [&]()
			{
				InspectorUI::InputFloat("##SpriteOpacity", sprite.Opacity, "%.2f", 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How much of the sprite shows (1 = all). Cut-out sprites below 1 are screen-door faded,\ntranslucent ones blended; the shadow stays. Game scripts set it (the camera's occluder fade).");
				}
			});
			if (texture != nullptr && entity.HasComponent<TransformComponent>())
			{
				InspectorUI::Row("##NativeSize", [&]()
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
			InspectorUI::Row("Sort Order", [&]()
			{
				int sortOrder = (int)sprite.SortOrder;
				if (ImGui::InputInt("##SortOrder", &sortOrder))
				{
					sprite.SortOrder = (int32_t)sortOrder;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Higher values are drawn on top. Equal values: Flat sprites first (lower Z first),\nthen Upright and Billboard ones from far to near.");
				}
			});
		});
	}

	static void DrawSpriteAnimator(Entity entity, InspectorContext& context)
	{
		bool showStartFrame = false;
		ComponentDrawers::Section<SpriteAnimatorComponent>("Sprite Animator", entity, context, [&](SpriteAnimatorComponent& animator)
		{
			const AssetHandle before = animator.Animations;
			InspectorUI::Row("Animations", [&]() { DrawAnimationSetField(entity, animator); });
			InspectorUI::Row("Start Clip", [&]() { showStartFrame |= DrawStartClipField(animator); });
			InspectorUI::Row("Play On Start", [&]() { ImGui::Checkbox("##PlayOnStart", &animator.PlayOnStart); });
			InspectorUI::Row("Speed", [&]() { InspectorUI::InputFloat("##Speed", animator.Speed, "%.2f", 0.0f, 100.0f); });
			showStartFrame |= animator.Animations != before;

			// Playback status (play mode): read through SpriteAnimator, never from the component's State.
			const std::string_view currentClip = SpriteAnimator::GetCurrentClip(entity);
			if (currentClip.empty() == false)
			{
				const char* status = SpriteAnimator::IsPlaying(entity) ? ""
					: (SpriteAnimator::IsFinished(entity) ? " (finished)" : " (stopped)");
				ImGui::TextDisabled("Playing %.*s, frame %u%s", (int)currentClip.size(), currentClip.data(),
					SpriteAnimator::GetFrameIndex(entity), status);
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
	}

	static void DrawShadow2D(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<Shadow2DComponent>("Shadow 2D", entity, context, [&](Shadow2DComponent& shadow)
		{
			InspectorUI::Row("Offset X",
				[&]() { InspectorUI::InputFloat("##ShadowOffsetX", shadow.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Offset Y",
				[&]() { InspectorUI::InputFloat("##ShadowOffsetY", shadow.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Width", [&]() { InspectorUI::InputFloat("##ShadowWidth", shadow.Size.x, "%.2f", 0.01f, 10000.0f); });
			InspectorUI::Row("Height", [&]() { InspectorUI::InputFloat("##ShadowHeight", shadow.Size.y, "%.2f", 0.01f, 10000.0f); });
			InspectorUI::Row("Strength", [&]() { InspectorUI::InputFloat("##ShadowStrength", shadow.Strength, "%.2f", 0.0f, 1.0f); });
			ImGui::TextDisabled("A blob under the entity. Shows in views that have lights.");
		});
	}

	static void DrawParticleEmitter2D(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<ParticleEmitter2DComponent>("Particle Emitter 2D", entity, context, [&](ParticleEmitter2DComponent& emitter)
		{
			const float limit = (float)ParticleEmitter2DComponent::MaxParticlesLimit;

			DrawSubheading("Emission");
			InspectorUI::Row("Play On Start", [&]() { ImGui::Checkbox("##EmitterPlayOnStart", &emitter.PlayOnStart); });
			InspectorUI::Row("Looping", [&]() { ImGui::Checkbox("##EmitterLooping", &emitter.Looping); });
			InspectorUI::Row("Duration",
				[&]() { InspectorUI::InputFloat("##EmitterDuration", emitter.Duration, "%.2f s", 0.01f, 3600.0f); });
			InspectorUI::Row("Rate", [&]() { InspectorUI::InputFloat("##EmitterRate", emitter.Rate, "%.1f / s", 0.0f, 10000.0f); });
			InspectorUI::Row("Burst", [&]() { PropertyUI::InputUInt("##EmitterBurst", emitter.BurstCount, 0, (uint32_t)limit); });
			InspectorUI::Row("Max Particles",
				[&]() { PropertyUI::InputUInt("##EmitterMaxParticles", emitter.MaxParticles, 1, (uint32_t)limit); });
			InspectorUI::Row("Seed", [&]()
			{
				PropertyUI::InputUInt("##EmitterSeed", emitter.Seed, 0, 1000000000u);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("0: a new random seed each time it plays. Any other value: the same pattern every time.");
				}
			});
			const char* shapeNames[] = { "Point", "Circle", "Box" };
			int shape = (int)emitter.Shape;
			InspectorUI::Row("Shape", [&]()
			{
				if (ImGui::Combo("##EmitterShape", &shape, shapeNames, 3))
				{
					emitter.Shape = (EmitterShape)shape;
				}
			});
			if (emitter.Shape == EmitterShape::Circle)
			{
				InspectorUI::Row("Radius",
					[&]() { InspectorUI::InputFloat("##EmitterRadius", emitter.ShapeSize.x, "%.2f", 0.0f, 10000.0f); });
			}
			else if (emitter.Shape == EmitterShape::Box)
			{
				InspectorUI::Row("Width",
					[&]() { InspectorUI::InputFloat("##EmitterWidth", emitter.ShapeSize.x, "%.2f", 0.0f, 10000.0f); });
				InspectorUI::Row("Height",
					[&]() { InspectorUI::InputFloat("##EmitterHeight", emitter.ShapeSize.y, "%.2f", 0.0f, 10000.0f); });
			}

			DrawSubheading("Particles at birth");
			InspectorUI::Row("Lifetime", [&]() { DrawRangeField("EmitterLifetime", emitter.Lifetime, 0.01f, 3600.0f); });
			InspectorUI::Row("Speed", [&]() { DrawRangeField("EmitterSpeed", emitter.Speed, -10000.0f, 10000.0f); });
			InspectorUI::Row("Direction",
				[&]() { InspectorUI::InputFloat("##EmitterDirection", emitter.Direction, "%.1f deg", -360.0f, 360.0f); });
			InspectorUI::Row("Spread", [&]() { InspectorUI::InputFloat("##EmitterSpread", emitter.Spread, "%.1f deg", 0.0f, 180.0f); });
			InspectorUI::Row("Size", [&]() { DrawRangeField("EmitterSize", emitter.StartSize, 0.0f, 10000.0f); });
			InspectorUI::Row("Rotation", [&]() { DrawRangeField("EmitterRotation", emitter.StartRotation, -360.0f, 360.0f, "%.1f"); });
			InspectorUI::Row("Spin", [&]() { DrawRangeField("EmitterSpin", emitter.AngularVelocity, -10000.0f, 10000.0f, "%.1f"); });

			DrawSubheading("Over lifetime");
			InspectorUI::Row("Start Color", [&]() { ImGui::ColorEdit4("##EmitterStartColor", glm::value_ptr(emitter.StartColor)); });
			InspectorUI::Row("End Color", [&]() { ImGui::ColorEdit4("##EmitterEndColor", glm::value_ptr(emitter.EndColor)); });
			InspectorUI::Row("End Size",
				[&]() { InspectorUI::InputFloat("##EmitterEndSize", emitter.EndSizeScale, "x %.2f", 0.0f, 100.0f); });
			InspectorUI::Row("Gravity X",
				[&]() { InspectorUI::InputFloat("##EmitterGravityX", emitter.Gravity.x, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Gravity Y",
				[&]() { InspectorUI::InputFloat("##EmitterGravityY", emitter.Gravity.y, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Drag", [&]() { InspectorUI::InputFloat("##EmitterDrag", emitter.Drag, "%.2f", 0.0f, 1000.0f); });
			InspectorUI::Row("Turbulence",
				[&]() { InspectorUI::InputFloat("##EmitterTurbulence", emitter.Turbulence, "%.2f", 0.0f, 10000.0f); });

			DrawSubheading("Look");
			InspectorUI::Row("Texture", [&]() { InspectorUI::TextureField("##EmitterTexture", emitter.Texture); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(emitter.Texture);
			if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
			{
				InspectorUI::Row("Sprite", [&]()
					{ PropertyUI::InputUInt("##EmitterSprite", emitter.SpriteIndex, 0, texture->GetSpriteCount() > 0 ? texture->GetSpriteCount() - 1 : 0); });
			}
			const char* blendNames[] = { "Alpha", "Additive" };
			int blend = (int)emitter.Blend;
			InspectorUI::Row("Blend", [&]()
			{
				if (ImGui::Combo("##EmitterBlend", &blend, blendNames, 2))
				{
					emitter.Blend = (ParticleBlend)blend;
				}
			});
			const char* lightingNames[] = { "Lit", "Unlit" };
			int lighting = (int)emitter.Lighting;
			InspectorUI::Row("Lighting", [&]()
			{
				if (ImGui::Combo("##EmitterLighting", &lighting, lightingNames, 2))
				{
					emitter.Lighting = (ParticleLighting)lighting;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Unlit: not darkened by scene lighting. Unlit + Additive stays bright at night.");
				}
			});
			const char* spaceNames[] = { "Ground", "View" };
			int space = (int)emitter.Space;
			InspectorUI::Row("Space", [&]()
			{
				if (ImGui::Combo("##EmitterSpace", &space, spaceNames, 2))
				{
					emitter.Space = (ParticleSpace)space;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("With a tilted camera: Ground keeps the particles on the ground plane (dust, leaves);\nView sprays them in the plane facing the camera (sparks, impacts).");
				}
			});
			InspectorUI::Row("Sort Order", [&]()
			{
				int sortOrder = (int)emitter.SortOrder;
				if (ImGui::InputInt("##EmitterSortOrder", &sortOrder))
				{
					emitter.SortOrder = (int32_t)sortOrder;
				}
			});

			InspectorUI::Row("Emission", [&]()
			{
				InspectorUI::InputFloat("##EmitterEmissionIntensity", emitter.EmissionIntensity, "%.2f", 0.0f, 100.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Each particle's colour times this is added after the lighting: sparks and embers glow\nat night and bloom above about 1 (HDR and Bloom). 0 = none.");
				}
			});

			DrawSubheading("Light (optional)");
			InspectorUI::Row("Light Radius",
				[&]() { InspectorUI::InputFloat("##EmitterLightRadius", emitter.LightRadius, "%.2f", 0.0f, 10000.0f); });
			if (emitter.LightRadius > 0.0f)
			{
				InspectorUI::Row("Light Intensity",
					[&]() { InspectorUI::InputFloat("##EmitterLightIntensity", emitter.LightIntensity, "%.2f", 0.0f, 100.0f); });
				InspectorUI::Row("Max Lights",
					[&]() { PropertyUI::InputUInt("##EmitterMaxLights", emitter.MaxLights, 0, (uint32_t)limit); });
			}

			DrawSubheading("Cleanup");
			InspectorUI::Row("Destroy When Done", [&]()
			{
				ImGui::Checkbox("##EmitterDestroyWhenFinished", &emitter.DestroyWhenFinished);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("For one-shot effects: the entity is destroyed once it stopped emitting and no particles are left.");
				}
			});

			ImGui::TextDisabled("Settings only for now: particles are simulated and drawn from Roadmap V2.");
		});
	}

	// It animates the entity's sprite, which needs a transform to be drawn.
	static void AddSpriteAnimator(Entity entity)
	{
		entity.AddComponent<SpriteAnimatorComponent>();
		if (entity.HasComponent<SpriteRendererComponent>() == false)
		{
			entity.AddComponent<SpriteRendererComponent>();
		}
		if (entity.HasComponent<TransformComponent>() == false)
		{
			entity.AddComponent<TransformComponent>();
		}
	}

	void ComponentDrawers::AddSpriteDrawers(std::vector<ComponentDrawer>& drawers)
	{
		drawers.push_back(Make<SpriteRendererComponent>("Sprite Renderer", &DrawSpriteRenderer,
			&AddWithTransform<SpriteRendererComponent>));
		drawers.push_back(Make<SpriteAnimatorComponent>("Sprite Animator", &DrawSpriteAnimator, &AddSpriteAnimator));
		drawers.push_back(Make<Shadow2DComponent>("Shadow 2D", &DrawShadow2D, &AddWithTransform<Shadow2DComponent>));
		// Particles are emitted at the entity's position.
		drawers.push_back(Make<ParticleEmitter2DComponent>("Particle Emitter 2D", &DrawParticleEmitter2D,
			&AddWithTransform<ParticleEmitter2DComponent>));
	}
}
