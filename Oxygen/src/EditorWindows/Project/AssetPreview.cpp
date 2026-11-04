#include "AssetPreview.h"
#include "Utilities/ThumbnailManager.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Data/Data.h>
#include <Hydro/Data/DataTypeRegistry.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Renderer/RenderProfile.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/Components/Components.h>
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/SpriteAnimation.h>

#include <algorithm>
#include <cmath>

namespace Hydro
{

	static ImU32 ToColor(float r, float g, float b, float a = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32({ r, g, b, a });
	}

	// Point inside a square tile, in 0..1 tile units.
	static ImVec2 TilePoint(const glm::vec2& min, float size, float u, float v)
	{
		return { min.x + u * size, min.y + v * size };
	}

	static void DrawCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float cell)
	{
		drawList->AddRectFilled(min, max, ToColor(0.30f, 0.30f, 0.32f));
		drawList->PushClipRect(min, max, true);

		const ImU32 dark = ToColor(0.22f, 0.22f, 0.24f);
		int row = 0;
		for (float y = min.y; y < max.y; y += cell, row++)
		{
			int column = 0;
			for (float x = min.x; x < max.x; x += cell, column++)
			{
				if ((row + column) % 2 == 0)
				{
					drawList->AddRectFilled({ x, y }, { x + cell, y + cell }, dark);
				}
			}
		}

		drawList->PopClipRect();
	}

	// The loaded asset for a tile. A full image only loads while `loadsLeft` allows it, so a folder of many prefabs
	// fills in over a few frames. An asset that failed is never retried.
	template<typename T>
	static Ref<T> FindAsset(AssetHandle handle, int& loadsLeft)
	{
		if (static_cast<uint64_t>(handle) == 0)
		{
			return nullptr;
		}

		Ref<T> loaded = AssetManager::GetLoadedAsset<T>(handle);
		if (loaded != nullptr)
		{
			return loaded;
		}

		if (loadsLeft > 0 && AssetManager::GetLoadState(handle) == AssetLoadState::NotLoaded)
		{
			loadsLeft--;
			return AssetManager::GetAsset<T>(handle);
		}
		return nullptr;
	}

	// The first frame of the clip an animator starts with.
	static bool GetStartFrame(const SpriteAnimationSet& set, const std::string& startClip, Ref<Texture2D>& texture, uint32_t& spriteIndex,
		bool& flipX)
	{
		const int32_t clipIndex = set.FindStartClip(startClip);
		if (clipIndex < 0 || set.GetTexture() == nullptr)
		{
			return false;
		}

		const SpriteAnimationClip& clip = set.GetClip((uint32_t)clipIndex);
		if (clip.Frames.empty())
		{
			return false;
		}

		texture = set.GetTexture();
		spriteIndex = clip.Frames[0].Sprite;
		flipX = clip.FlipX;
		return true;
	}

	// What a sprite entity looks like at the start: its animation's first frame, else its own sprite. False when
	// there is nothing to draw yet.
	static bool ResolveSprite(AssetHandle textureHandle, uint32_t spriteIndex, bool flipX, AssetHandle animations,
		const std::string& startClip,
		int& loadsLeft, Ref<Texture2D>& texture, uint32_t& outIndex, bool& outFlipX)
	{
		if (static_cast<uint64_t>(animations) != 0)
		{
			const Ref<SpriteAnimationSet> set = FindAsset<SpriteAnimationSet>(animations, loadsLeft);
			if (set != nullptr && GetStartFrame(*set, startClip, texture, outIndex, outFlipX))
			{
				return true;
			}
		}

		texture = FindAsset<Texture2D>(textureHandle, loadsLeft);
		outIndex = spriteIndex;
		outFlipX = flipX;
		return texture != nullptr;
	}

	// Sprite `spriteIndex` of `texture` fitted into the box, keeping its aspect ratio, tinted.
	static void DrawTileSprite(ImDrawList* drawList, const Ref<Texture2D>& texture, uint32_t spriteIndex, bool flipX, ImU32 tint,
		const ImVec2& boxMin, const ImVec2& boxMax)
	{
		const SpriteRegion& sprite = texture->GetSprite(spriteIndex);
		const float width = (float)std::max(sprite.Size.x, 1u);
		const float height = (float)std::max(sprite.Size.y, 1u);
		const float boxWidth = boxMax.x - boxMin.x;
		const float boxHeight = boxMax.y - boxMin.y;
		const float scale = std::min(boxWidth / width, boxHeight / height);
		const ImVec2 imageMin = { boxMin.x + (boxWidth - width * scale) * 0.5f, boxMin.y + (boxHeight - height * scale) * 0.5f };
		const ImVec2 imageMax = { imageMin.x + width * scale, imageMin.y + height * scale };

		TextureRect rect = texture->GetSpriteUVRect(spriteIndex);
		if (flipX)
		{
			std::swap(rect.Min.x, rect.Max.x);
		}
		drawList->AddImage((ImTextureID)Renderer2D::GetImGuiTexture(texture), imageMin, imageMax, { rect.Min.x, rect.Min.y },
			{ rect.Max.x, rect.Max.y }, tint);
	}

	// A sheet of paper: for files whose picture isn't ready or can't be made.
	static void DrawPageIcon(ImDrawList* drawList, const glm::vec2& min, float size)
	{
		const ImU32 page = ToColor(0.62f, 0.64f, 0.68f);
		drawList->AddRectFilled(TilePoint(min, size, 0.28f, 0.18f), TilePoint(min, size, 0.72f, 0.82f), page, 2.0f);
		drawList->AddTriangleFilled(TilePoint(min, size, 0.58f, 0.18f), TilePoint(min, size, 0.72f, 0.18f),
			TilePoint(min, size, 0.72f, 0.32f), ToColor(0.17f, 0.17f, 0.19f));
		for (int line = 0; line < 3; line++)
		{
			const float v = 0.42f + 0.12f * line;
			drawList->AddLine(TilePoint(min, size, 0.36f, v), TilePoint(min, size, 0.64f, v), ToColor(0.40f, 0.42f, 0.46f), 2.0f);
		}
	}

	void AssetPreview::DrawThumbnail(const AssetItem& item, const glm::vec2& min, float size, bool hovered, bool selected)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 tileMin = { min.x, min.y };
		const ImVec2 tileMax = { min.x + size, min.y + size };
		const float rounding = 4.0f;

		// Tile background + selection.
		drawList->AddRectFilled(tileMin, tileMax, hovered ? ToColor(0.24f, 0.24f, 0.27f) : ToColor(0.17f, 0.17f, 0.19f), rounding);
		if (selected)
		{
			drawList->AddRect(tileMin, tileMax, ToColor(0.26f, 0.59f, 0.98f), rounding, 0, 2.0f);
		}

		switch (item.Type)
		{
			case AssetItemType::Folder:
			{
				const ImU32 folder = ToColor(0.86f, 0.66f, 0.28f);
				const ImU32 folderDark = ToColor(0.72f, 0.53f, 0.20f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.26f), TilePoint(min, size, 0.46f, 0.36f), folderDark, 3.0f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.32f), TilePoint(min, size, 0.84f, 0.78f), folder, 3.0f);
				break;
			}
			case AssetItemType::Scene:
			{
				const ImVec2 frameMin = TilePoint(min, size, 0.16f, 0.22f);
				const ImVec2 frameMax = TilePoint(min, size, 0.84f, 0.78f);
				drawList->AddRectFilled(frameMin, frameMax, ToColor(0.16f, 0.30f, 0.50f), 3.0f);
				drawList->AddCircleFilled(TilePoint(min, size, 0.68f, 0.36f), size * 0.06f, ToColor(0.98f, 0.85f, 0.35f));
				drawList->AddTriangleFilled(TilePoint(min, size, 0.20f, 0.76f), TilePoint(min, size, 0.42f, 0.44f),
					TilePoint(min, size, 0.62f, 0.76f), ToColor(0.30f, 0.62f, 0.35f));
				drawList->AddTriangleFilled(TilePoint(min, size, 0.46f, 0.76f), TilePoint(min, size, 0.62f, 0.54f),
					TilePoint(min, size, 0.80f, 0.76f), ToColor(0.22f, 0.50f, 0.28f));
				drawList->AddRect(frameMin, frameMax, ToColor(0.60f, 0.75f, 0.95f), 3.0f, 0, 1.5f);
				// Green strip = scene.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.30f, 0.70f, 0.40f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Prefab:
			{
				const PrefabPreview& preview = GetPrefabPreview(item.Path);

				// The first frame of its animation, or its sprite's image; none when it has no image.
				Ref<Texture2D> spriteTexture;
				uint32_t spriteIndex = 0;
				bool spriteFlipX = false;
				const bool hasImage = preview.Valid && preview.HasSprite &&
					ResolveSprite(preview.TextureHandle, preview.SpriteIndex, preview.FlipX, preview.AnimationsHandle, preview.StartClip,
						m_LoadsLeft, spriteTexture, spriteIndex, spriteFlipX);

				const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
				const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.88f);
				DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));

				const ImVec2 center = { (innerMin.x + innerMax.x) * 0.5f, (innerMin.y + innerMax.y) * 0.5f };
				const float extent = (innerMax.x - innerMin.x) * 0.36f; // half size of the biggest side

				if (preview.Valid == false)
				{
					// Couldn't read the file.
					drawList->AddLine({ center.x - extent, center.y - extent }, { center.x + extent, center.y + extent },
						ToColor(0.90f, 0.30f, 0.30f), 3.0f);
					drawList->AddLine({ center.x - extent, center.y + extent }, { center.x + extent, center.y - extent },
						ToColor(0.90f, 0.30f, 0.30f), 3.0f);
				}
				else if (hasImage)
				{
					DrawTileSprite(drawList, spriteTexture, spriteIndex, spriteFlipX,
						ImGui::ColorConvertFloat4ToU32({ preview.Color.r, preview.Color.g, preview.Color.b, preview.Color.a }),
						innerMin, innerMax);
				}
				else if (preview.HasSprite)
				{
					// No image (none set, or not loaded yet): a coloured quad with the sprite's aspect ratio and Z rotation.
					const float sx = std::max(std::abs(preview.Scale.x), 0.0001f);
					const float sy = std::max(std::abs(preview.Scale.y), 0.0001f);
					const float largest = std::max(sx, sy);
					const float halfW = extent * sx / largest;
					const float halfH = extent * sy / largest;

					const float c = std::cos(preview.RotationZ);
					const float s = std::sin(preview.RotationZ);
					auto corner = [&](float x, float y) -> ImVec2 { return { center.x + x * c - y * s, center.y + x * s + y * c }; };

					const ImVec2 p0 = corner(-halfW, -halfH);
					const ImVec2 p1 = corner(halfW, -halfH);
					const ImVec2 p2 = corner(halfW, halfH);
					const ImVec2 p3 = corner(-halfW, halfH);

					drawList->AddQuadFilled(p0, p1, p2, p3,
						ImGui::ColorConvertFloat4ToU32({ preview.Color.r, preview.Color.g, preview.Color.b, preview.Color.a }));
					drawList->AddQuad(p0, p1, p2, p3, ToColor(0.0f, 0.0f, 0.0f, 0.45f), 1.0f);
				}
				else
				{
					// Entity without a sprite: show its origin.
					const ImU32 gizmo = ToColor(0.85f, 0.85f, 0.90f);
					drawList->AddCircle(center, extent * 0.55f, gizmo, 24, 1.5f);
					drawList->AddLine({ center.x, center.y }, { center.x + extent * 0.8f, center.y }, ToColor(0.85f, 0.25f, 0.25f), 2.0f);
					drawList->AddLine({ center.x, center.y }, { center.x, center.y - extent * 0.8f }, ToColor(0.30f, 0.80f, 0.30f), 2.0f);
				}

				// Blue strip = prefab.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.26f, 0.59f, 0.98f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Texture:
			{
				const Thumbnail& thumbnail = ThumbnailManager::Get(item.Handle);
				if (thumbnail.Texture != nullptr)
				{
					const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
					const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.90f);
					DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));
					DrawTileSprite(drawList, thumbnail.Texture, 0, false, ToColor(1.0f, 1.0f, 1.0f), innerMin, innerMax);

					// Sprite sheets get a strip, like scenes and prefabs.
					if (thumbnail.IsSheet)
					{
						drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ImGui::GetColorU32(ImGuiCol_CheckMark),
							rounding, ImDrawFlags_RoundCornersBottom);
					}
				}
				else
				{
					DrawPageIcon(drawList, min, size);
				}
				break;
			}
			case AssetItemType::Animation:
			{
				Ref<Texture2D> frameTexture;
				uint32_t frameSprite = 0;
				bool frameFlipX = false;
				const Ref<SpriteAnimationSet> set = FindAsset<SpriteAnimationSet>(item.Handle, m_LoadsLeft);
				if (set != nullptr && GetStartFrame(*set, std::string(), frameTexture, frameSprite, frameFlipX))
				{
					const ImVec2 innerMin = TilePoint(min, size, 0.10f, 0.10f);
					const ImVec2 innerMax = TilePoint(min, size, 0.90f, 0.88f);
					DrawCheckerboard(drawList, innerMin, innerMax, std::max(4.0f, size / 11.0f));
					DrawTileSprite(drawList, frameTexture, frameSprite, frameFlipX, ToColor(1.0f, 1.0f, 1.0f), innerMin, innerMax);
				}
				else
				{
					DrawPageIcon(drawList, min, size);
				}

				// Purple strip = animation set.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.62f, 0.40f, 0.90f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Material:
			{
				// A round swatch: its albedo image (or white) times its tint.
				const Ref<MaterialAsset> material = FindAsset<MaterialAsset>(item.Handle, m_LoadsLeft);
				const ImVec2 swatchMin = TilePoint(min, size, 0.18f, 0.14f);
				const ImVec2 swatchMax = TilePoint(min, size, 0.82f, 0.78f);
				const float radius = (swatchMax.x - swatchMin.x) * 0.5f;
				const ImVec2 center = { swatchMin.x + radius, swatchMin.y + radius };
				if (material != nullptr)
				{
					const MaterialData& data = material->GetData();
					const ImU32 tint = ImGui::ColorConvertFloat4ToU32({ data.Tint.r, data.Tint.g, data.Tint.b, 1.0f });
					const Ref<Texture2D> albedo = (uint64_t)data.Albedo != 0 ? ThumbnailManager::Get(data.Albedo).Texture : nullptr;
					if (albedo != nullptr)
					{
						drawList->AddImageRounded((ImTextureID)Renderer2D::GetImGuiTexture(albedo), swatchMin, swatchMax, { 0.0f, 0.0f },
							{ 1.0f, 1.0f }, tint, radius);
					}
					else
					{
						drawList->AddCircleFilled(center, radius, tint, 32);
					}
					// A highlight, so it reads as a ball.
					drawList->AddCircleFilled({ center.x - radius * 0.35f, center.y - radius * 0.35f }, radius * 0.18f,
						ToColor(1.0f, 1.0f, 1.0f, 0.35f), 16);
				}
				else
				{
					drawList->AddCircleFilled(center, radius, ToColor(0.45f, 0.45f, 0.48f), 32);
				}
				drawList->AddCircle(center, radius, ToColor(0.0f, 0.0f, 0.0f, 0.5f), 32, 1.5f);

				// Orange strip = material.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.95f, 0.55f, 0.20f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::RenderProfile:
			{
				// A little landscape: the sky ambient over the ground ambient, a fog band at the horizon. The colours are
				// linear and dim, so they are brightened to read on the tile.
				const Ref<RenderProfile> profile = FindAsset<RenderProfile>(item.Handle, m_LoadsLeft);
				const ImVec2 viewMin = TilePoint(min, size, 0.14f, 0.14f);
				const ImVec2 viewMax = TilePoint(min, size, 0.86f, 0.80f);
				const float horizon = viewMin.y + (viewMax.y - viewMin.y) * 0.55f;
				auto shown = [](const glm::vec3& linear)
				{
					const glm::vec3 c = glm::clamp(glm::sqrt(glm::max(linear, glm::vec3(0.0f))), glm::vec3(0.0f), glm::vec3(1.0f));
					return ImGui::ColorConvertFloat4ToU32({ c.r, c.g, c.b, 1.0f });
				};
				if (profile != nullptr && profile->GetData().Environment.Override)
				{
					const SceneEnvironment& environment = profile->GetData().Environment;
					drawList->AddRectFilled(viewMin, { viewMax.x, horizon }, shown(environment.SkyAmbient));
					drawList->AddRectFilled({ viewMin.x, horizon }, viewMax, shown(environment.GroundAmbient));
					if (environment.Fog)
					{
						const float band = (viewMax.y - viewMin.y) * 0.12f;
						drawList->AddRectFilled({ viewMin.x, horizon - band }, { viewMax.x, horizon + band },
							(shown(environment.FogColor) & 0x00FFFFFF) | 0xB0000000);
					}
				}
				else
				{
					drawList->AddRectFilled(viewMin, viewMax, ToColor(0.45f, 0.45f, 0.48f));
				}
				drawList->AddRect(viewMin, viewMax, ToColor(0.0f, 0.0f, 0.0f, 0.5f), 0.0f, 0, 1.5f);

				// Teal strip = render profile.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.20f, 0.75f, 0.70f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Model:
			{
				// A cube seen from above the front-right: three shaded faces.
				const ImVec2 top[4] = { TilePoint(min, size, 0.50f, 0.18f), TilePoint(min, size, 0.80f, 0.32f),
					TilePoint(min, size, 0.50f, 0.46f), TilePoint(min, size, 0.20f, 0.32f) };
				const ImVec2 left[4] = { TilePoint(min, size, 0.20f, 0.32f), TilePoint(min, size, 0.50f, 0.46f),
					TilePoint(min, size, 0.50f, 0.82f), TilePoint(min, size, 0.20f, 0.68f) };
				const ImVec2 right[4] = { TilePoint(min, size, 0.50f, 0.46f), TilePoint(min, size, 0.80f, 0.32f),
					TilePoint(min, size, 0.80f, 0.68f), TilePoint(min, size, 0.50f, 0.82f) };
				drawList->AddConvexPolyFilled(top, 4, ToColor(0.78f, 0.80f, 0.86f));
				drawList->AddConvexPolyFilled(left, 4, ToColor(0.52f, 0.55f, 0.62f));
				drawList->AddConvexPolyFilled(right, 4, ToColor(0.38f, 0.41f, 0.48f));

				// Teal strip = model.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.20f, 0.70f, 0.70f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Font:
			{
				// A page with "Aa" on it.
				DrawPageIcon(drawList, min, size);
				const char* sample = "Aa";
				const ImVec2 textSize = ImGui::CalcTextSize(sample);
				const ImVec2 centre = TilePoint(min, size, 0.5f, 0.5f);
				drawList->AddText({ centre.x - textSize.x * 0.5f, centre.y - textSize.y * 0.5f }, ToColor(0.10f, 0.10f, 0.12f), sample);

				// Gold strip = font.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.90f, 0.75f, 0.30f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Terrain:
			{
				// Two grass shelves with a stone face between them and a dirt path across the lower one.
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.22f), TilePoint(min, size, 0.84f, 0.44f),
					ToColor(0.42f, 0.62f, 0.26f), 2.0f);
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.44f), TilePoint(min, size, 0.84f, 0.54f),
					ToColor(0.40f, 0.40f, 0.44f));
				drawList->AddRectFilled(TilePoint(min, size, 0.16f, 0.54f), TilePoint(min, size, 0.84f, 0.80f),
					ToColor(0.36f, 0.56f, 0.22f), 2.0f);
				drawList->AddRectFilled(TilePoint(min, size, 0.44f, 0.54f), TilePoint(min, size, 0.58f, 0.80f),
					ToColor(0.62f, 0.44f, 0.28f));
				// Green strip = terrain.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.45f, 0.70f, 0.30f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::Definition:
			{
				// A page: one record.
				DrawPageIcon(drawList, min, size);
				// Amber strip = definition.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.95f, 0.60f, 0.20f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
			case AssetItemType::DataTable:
			{
				// A grid: rows of records.
				const ImU32 cell = ToColor(0.62f, 0.64f, 0.68f);
				for (int row = 0; row < 4; row++)
				{
					const float top = 0.22f + 0.15f * row;
					drawList->AddRectFilled(TilePoint(min, size, 0.20f, top), TilePoint(min, size, 0.38f, top + 0.11f), cell, 1.0f);
					drawList->AddRectFilled(TilePoint(min, size, 0.42f, top), TilePoint(min, size, 0.80f, top + 0.11f), cell, 1.0f);
				}
				// Rust strip = data table.
				drawList->AddRectFilled(TilePoint(min, size, 0.0f, 0.94f), tileMax, ToColor(0.80f, 0.40f, 0.25f), rounding,
					ImDrawFlags_RoundCornersBottom);
				break;
			}
		}
	}

	void AssetPreview::DrawTooltip(const AssetItem& item)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(item.DisplayName.c_str());

		switch (item.Type)
		{
			case AssetItemType::Prefab:
			{
				const PrefabPreview& preview = GetPrefabPreview(item.Path);
				if (preview.Valid == false)
				{
					ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read prefab file");
					break;
				}

				ImGui::TextDisabled("Prefab");
				ImGui::Separator();
				ImGui::BulletText("Tag");
				if (preview.HasTransform)
				{
					ImGui::BulletText("Transform  (scale %.1f x %.1f)", preview.Scale.x, preview.Scale.y);
				}
				if (preview.HasSprite)
				{
					ImGui::BulletText("Sprite Renderer");
					ImGui::SameLine();
					ImGui::ColorButton("##color", { preview.Color.r, preview.Color.g, preview.Color.b, preview.Color.a },
						ImGuiColorEditFlags_NoTooltip, { 12.0f, 12.0f });
				}
				ImGui::Spacing();
				ImGui::TextDisabled("Double-click or drag into the scene");
				break;
			}
			case AssetItemType::Scene:
				ImGui::TextDisabled("Scene - double-click or drag to the viewport to open");
				break;
			case AssetItemType::Folder:
				ImGui::TextDisabled("Folder");
				break;
			case AssetItemType::Texture:
				ImGui::TextDisabled(ThumbnailManager::Get(item.Handle).IsSheet ? "Image - sprite sheet" : "Image");
				break;
			case AssetItemType::Animation:
				ImGui::TextDisabled("Sprite animation");
				break;
			case AssetItemType::Material:
				ImGui::TextDisabled("Material (Standard) - drag onto a mesh's material Asset field");
				break;
			case AssetItemType::RenderProfile:
				ImGui::TextDisabled("Render profile - a shared look; drag onto Render Settings > Look (scene)");
				break;
			case AssetItemType::Model:
				ImGui::TextDisabled("Model (glTF) - drag into the viewport to place it");
				break;
			case AssetItemType::Font:
				ImGui::TextDisabled("Font (BMFont) - pick it in a UI Text's Font field");
				break;
			case AssetItemType::Terrain:
				ImGui::TextDisabled("Terrain - pick it in a Terrain component's Terrain field");
				break;
			case AssetItemType::Definition:
			case AssetItemType::DataTable:
			{
				// Loads the file (once; the manager caches it) to say what it holds and whether it is valid.
				const DataTypeInfo* type = Data::GetDataType(item.Handle);
				const bool isTable = item.Type == AssetItemType::DataTable;
				if (type == nullptr)
				{
					ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "%s - can't be read (see the console)",
						isTable ? "Data table" : "Definition");
					break;
				}
				ImGui::TextDisabled("%s of %s - pick it in a script's %s field", isTable ? "Data table" : "Definition", type->Name.c_str(),
					isTable ? "Asset(DataTable, ...) or DataRow(...)" : "Asset(Definition, ...)");
				const DataReport report = Data::Validate(item.Handle);
				if (report.IsEmpty() == false)
				{
					ImGui::TextColored(report.HasErrors() ? ImVec4(0.9f, 0.4f, 0.4f, 1.0f) : ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "%s",
						report.GetSummary().c_str());
					size_t shown = 0;
					for (const DataMessage& message : report.GetMessages())
					{
						if (shown++ == 6)
						{
							ImGui::TextDisabled("...");
							break;
						}
						ImGui::BulletText("%s", message.ToString().c_str());
					}
				}
				break;
			}
		}

		if (item.Type != AssetItemType::Folder)
		{
			ImGui::TextDisabled("Asset handle %llu", (unsigned long long)(uint64_t)item.Handle);
		}

		ImGui::EndTooltip();
	}

	const AssetPreview::PrefabPreview& AssetPreview::GetPrefabPreview(const std::filesystem::path& path)
	{
		std::error_code ec;
		const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, ec);

		const std::string key = path.u8string();
		auto it = m_PrefabPreviews.find(key);
		if (it != m_PrefabPreviews.end() && it->second.WriteTime == writeTime)
		{
			return it->second;
		}

		// Reload with the same loader as instantiating, so previews always match what you'd get.
		PrefabPreview preview;
		preview.WriteTime = writeTime;

		Scene previewScene("PrefabPreview");
		Entity entity = PrefabSerializer::Load(previewScene, path);
		if (entity)
		{
			preview.Valid = true;
			preview.Name = entity.GetName();

			if (entity.HasComponent<TransformComponent>())
			{
				const TransformComponent& transform = entity.GetComponent<TransformComponent>();
				preview.HasTransform = true;
				preview.Scale = transform.Scale;
				preview.RotationZ = transform.Rotation.z;
			}

			if (entity.HasComponent<SpriteRendererComponent>())
			{
				const SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();
				preview.HasSprite = true;
				preview.Color = sprite.Color;
				preview.TextureHandle = sprite.Texture;
				preview.SpriteIndex = sprite.SpriteIndex;
				preview.FlipX = sprite.FlipX;
			}

			if (entity.HasComponent<SpriteAnimatorComponent>())
			{
				const SpriteAnimatorComponent& animator = entity.GetComponent<SpriteAnimatorComponent>();
				preview.AnimationsHandle = animator.Animations;
				preview.StartClip = animator.StartClip;
			}
		}

		return m_PrefabPreviews[key] = preview;
	}
}
