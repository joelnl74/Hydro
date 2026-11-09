#include "hypch.h"

#include "SceneUIRenderer.h"
#include "Renderer2D.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Font.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UIInteraction.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Hydro
{
	// A view-pixel rect (min..max) as the transform of Renderer2D's unit quad.
	static glm::mat4 GetPixelQuadTransform(const glm::vec2& min, const glm::vec2& max)
	{
		const glm::vec2 centre = (min + max) * 0.5f;
		return glm::translate(glm::mat4(1.0f), glm::vec3(centre, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(max - min, 1.0f));
	}

	// A canvas-pixel point in view pixels, on a whole pixel.
	static glm::vec2 CanvasToViewPixels(const UICanvasMetrics& metrics, const glm::vec2& point)
	{
		return glm::round(metrics.Offset + point * metrics.Scale);
	}

	// A view-pixel quad cut to clipMin..clipMax (a scroll view's rect), its texture rect cut with it.
	// The rect's Min is the quad's left and top edge (texture rows run down, view pixels up).
	static void DrawClippedQuad(const glm::vec2& viewMin, const glm::vec2& viewMax, const Ref<Texture2D>& texture, const TextureRect& rect,
		const glm::vec4& color, const glm::vec2& clipMin, const glm::vec2& clipMax)
	{
		const glm::vec2 min = glm::max(viewMin, clipMin);
		const glm::vec2 max = glm::min(viewMax, clipMax);
		if (max.x <= min.x || max.y <= min.y)
		{
			return;
		}
		if (min == viewMin && max == viewMax)
		{
			Renderer2D::DrawQuad(GetPixelQuadTransform(viewMin, viewMax), texture, rect, color);
			return;
		}
		const glm::vec2 size = viewMax - viewMin;
		const float left = (min.x - viewMin.x) / size.x;
		const float right = (max.x - viewMin.x) / size.x;
		const float top = (viewMax.y - max.y) / size.y;
		const float bottom = (viewMax.y - min.y) / size.y;
		TextureRect cut;
		cut.Min = { glm::mix(rect.Min.x, rect.Max.x, left), glm::mix(rect.Min.y, rect.Max.y, top) };
		cut.Max = { glm::mix(rect.Min.x, rect.Max.x, right), glm::mix(rect.Min.y, rect.Max.y, bottom) };
		Renderer2D::DrawQuad(GetPixelQuadTransform(min, max), texture, cut, color);
	}

	// A one-pixel outline just inside a view-pixel rect, as four thin quads (lines would go through the scene camera).
	static void DrawPixelOutline(const glm::vec2& min, const glm::vec2& max, const glm::vec4& color)
	{
		if (max.x - min.x < 1.0f || max.y - min.y < 1.0f)
		{
			return;
		}
		Renderer2D::DrawQuad(GetPixelQuadTransform({ min.x, min.y }, { max.x, min.y + 1.0f }), color);
		Renderer2D::DrawQuad(GetPixelQuadTransform({ min.x, max.y - 1.0f }, { max.x, max.y }), color);
		Renderer2D::DrawQuad(GetPixelQuadTransform({ min.x, min.y + 1.0f }, { min.x + 1.0f, max.y - 1.0f }), color);
		Renderer2D::DrawQuad(GetPixelQuadTransform({ max.x - 1.0f, min.y + 1.0f }, { max.x, max.y - 1.0f }), color);
	}

	void SceneUIRenderer::Submit(Scene& scene)
	{
		auto canvases = scene.m_Registry.view<UICanvasComponent>();
		if (canvases.empty())
		{
			return;
		}

		// The visible top-level canvases, in SortOrder and then hierarchy order. A canvas inside another isn't drawn
		// (Scene::ResolveUILayout warns about it).
		m_Canvases.clear();
		const std::vector<entt::entity>& order = scene.GetEntityOrder();
		for (uint32_t i = 0; i < (uint32_t)order.size(); i++)
		{
			const entt::entity handle = order[i];
			if (canvases.contains(handle) == false || canvases.get<UICanvasComponent>(handle).Visible == false)
			{
				continue;
			}
			bool nested = false;
			for (Entity parent = scene.GetParent(Entity{ handle, &scene }); (bool)parent; parent = scene.GetParent(parent))
			{
				if (parent.HasComponent<UICanvasComponent>())
				{
					nested = true;
					break;
				}
			}
			if (nested == false)
			{
				m_Canvases.push_back({ canvases.get<UICanvasComponent>(handle).SortOrder, i, handle });
			}
		}
		if (m_Canvases.empty())
		{
			return;
		}
		std::sort(m_Canvases.begin(), m_Canvases.end(), [](const CanvasItem& a, const CanvasItem& b)
		{
			return a.SortOrder != b.SortOrder ? a.SortOrder < b.SortOrder : a.Index < b.Index;
		});

		// Screen space in view pixels, Y up, origin at the bottom-left: the same viewport the layout was resolved for.
		const glm::vec2 viewSize{ (float)scene.GetViewportWidth(), (float)scene.GetViewportHeight() };
		const UIInteraction* ui = scene.GetUIInteraction();
		Renderer2D::BeginScreenOverlay(Camera::Orthographic(0.0f, viewSize.x, 0.0f, viewSize.y, -1.0f, 1.0f));
		const glm::vec4 canvasOutline{ 1.0f, 0.85f, 0.2f, 1.0f };
		const glm::vec4 rectOutline{ 0.3f, 0.85f, 1.0f, 0.9f };
		for (const CanvasItem& item : m_Canvases)
		{
			const UICanvasComponent& canvas = canvases.get<UICanvasComponent>(item.Entity);
			// Where the layout placed it; a World canvas off screen (or without a camera) isn't placed.
			UICanvasMetrics metrics;
			if (scene.GetUICanvasMetrics(Entity{ item.Entity, &scene }.GetUUID(), metrics) == false)
			{
				continue;
			}

			scene.GetSubtree(Entity{ item.Entity, &scene }, m_Subtree);
			m_Buttons.clear();
			uint32_t skipBelow = UINT32_MAX; // a focus-visibility element that is hidden: skip its subtree
			for (Entity element : m_Subtree)
			{
				const uint32_t depth = scene.GetDepth(element);
				if (depth > skipBelow)
				{
					continue;
				}
				skipBelow = UINT32_MAX;

				// The tint and state of the button this element belongs to (the stack holds the buttons above it).
				while (m_Buttons.empty() == false && m_Buttons.back().Depth >= depth)
				{
					m_Buttons.pop_back();
				}
				ButtonFrame frame = m_Buttons.empty() ? ButtonFrame{} : m_Buttons.back();
				if (element.HasComponent<UIButtonComponent>())
				{
					const UIButtonComponent& button = element.GetComponent<UIButtonComponent>();
					UIInteractionState state = button.Interactable ? UIInteractionState::Normal : UIInteractionState::Disabled;
					if (ui != nullptr)
					{
						state = ui->GetState(scene, element);
					}
					switch (state)
					{
					case UIInteractionState::Focused: frame.Tint *= button.FocusedTint; break;
					case UIInteractionState::Pressed: frame.Tint *= button.PressedTint; break;
					case UIInteractionState::Disabled: frame.Tint *= button.DisabledTint; break;
					default: frame.Tint *= button.NormalTint; break;
					}
					frame.Depth = depth;
					frame.Focused = state == UIInteractionState::Focused || state == UIInteractionState::Pressed;
					m_Buttons.push_back(frame);
				}
				// Shown only while its button is (or isn't) focused: a cursor, a highlight.
				if (element.HasComponent<UIFocusVisibilityComponent>())
				{
					const bool wantFocused = element.GetComponent<UIFocusVisibilityComponent>().Show == UIFocusVisibility::Focused;
					if (frame.Focused != wantFocused)
					{
						skipBelow = depth;
						continue;
					}
				}

				if (element.HasComponent<UIResolvedRectComponent>() == false)
				{
					continue;
				}
				const UIResolvedRectComponent& rect = element.GetComponent<UIResolvedRectComponent>();
				if (rect.Visible == false)
				{
					continue;
				}
				// What the scroll views above it show, in view pixels.
				const glm::vec2 clipMin = rect.ClipMin.x < -1.0e8f ? glm::vec2(-1.0e9f) : CanvasToViewPixels(metrics, rect.ClipMin);
				const glm::vec2 clipMax = rect.ClipMax.x > 1.0e8f ? glm::vec2(1.0e9f) : CanvasToViewPixels(metrics, rect.ClipMax);
				if (element.HasComponent<UIImageComponent>())
				{
					SubmitImage(element.GetComponent<UIImageComponent>(), rect.Min, rect.Max, metrics, frame.Tint, clipMin, clipMax);
				}
				if (element.HasComponent<UITextComponent>())
				{
					SubmitText(element.GetComponent<UITextComponent>(), rect.Min, rect.Max, metrics, frame.Tint, clipMin, clipMax);
				}
			}

			if (canvas.DrawDebug)
			{
				for (Entity element : m_Subtree)
				{
					if (element.HasComponent<UIResolvedRectComponent>())
					{
						const UIResolvedRectComponent& rect = element.GetComponent<UIResolvedRectComponent>();
						if (rect.Visible)
						{
							DrawPixelOutline(CanvasToViewPixels(metrics, rect.Min), CanvasToViewPixels(metrics, rect.Max), rectOutline);
						}
					}
				}
				DrawPixelOutline(CanvasToViewPixels(metrics, glm::vec2(0.0f)), CanvasToViewPixels(metrics, metrics.Size), canvasOutline);
			}
		}
		Renderer2D::EndScreenOverlay();
	}

	void SceneUIRenderer::SubmitImage(const UIImageComponent& image, const glm::vec2& min, const glm::vec2& max,
		const UICanvasMetrics& metrics, const glm::vec4& tint,
		const glm::vec2& clipMin, const glm::vec2& clipMax)
	{
		// Never loads: whoever assigned the handle loaded it. No texture (or one not loaded) is a flat colour.
		const Ref<Texture2D> texture = AssetManager::GetLoadedAsset<Texture2D>(image.Texture);
		if (texture == nullptr || UILayout::IsNineSliced(image) == false)
		{
			TextureRect rect;
			if (texture != nullptr)
			{
				if (texture->GetSpriteMode() == SpriteMode::Multiple)
				{
					rect = texture->GetSpriteUVRect(image.SpriteIndex);
				}
				if (image.FlipX)
				{
					std::swap(rect.Min.x, rect.Max.x);
				}
				if (image.FlipY)
				{
					std::swap(rect.Min.y, rect.Max.y);
				}
			}
			const glm::vec2 viewMin = CanvasToViewPixels(metrics, min);
			const glm::vec2 viewMax = CanvasToViewPixels(metrics, max);
			if (viewMax.x > viewMin.x && viewMax.y > viewMin.y)
			{
				DrawClippedQuad(viewMin, viewMax, texture, rect, image.Color * tint, clipMin, clipMax);
			}
			return;
		}

		// Nine slices of the sprite's region (the whole image for a single texture).
		glm::vec2 sourcePosition{ 0.0f };
		glm::vec2 sourceSize{ (float)texture->GetWidth(), (float)texture->GetHeight() };
		if (texture->GetSpriteMode() == SpriteMode::Multiple)
		{
			const SpriteRegion& region = texture->GetSprite(image.SpriteIndex);
			sourcePosition = glm::vec2(region.Position);
			sourceSize = glm::vec2(region.Size);
		}

		UINineSliceCell cells[9];
		const uint32_t count = UILayout::BuildNineSlice(min, max, sourcePosition, sourceSize, image.NineSlice, image.FlipX,
			image.FlipY, cells);
		const glm::vec2 imageSize = glm::max(glm::vec2((float)texture->GetWidth(), (float)texture->GetHeight()), glm::vec2(1.0f));
		// Inset a thousandth of a texel so nearest sampling at a cell's edge never reads the next cell.
		const glm::vec2 inset = glm::vec2(0.001f) / imageSize;
		for (uint32_t i = 0; i < count; i++)
		{
			const UINineSliceCell& cell = cells[i];
			const glm::vec2 viewMin = CanvasToViewPixels(metrics, cell.DestinationMin);
			const glm::vec2 viewMax = CanvasToViewPixels(metrics, cell.DestinationMax);
			if (viewMax.x <= viewMin.x || viewMax.y <= viewMin.y)
			{
				continue;
			}

			TextureRect rect;
			rect.Min = cell.SourceMin / imageSize;
			rect.Max = cell.SourceMax / imageSize;
			const glm::vec2 direction = glm::sign(rect.Max - rect.Min); // -1 on a mirrored axis
			rect.Min += inset * direction;
			rect.Max -= inset * direction;
			DrawClippedQuad(viewMin, viewMax, texture, rect, image.Color * tint, clipMin, clipMax);
		}
	}

	void SceneUIRenderer::SubmitText(const UITextComponent& text, const glm::vec2& min, const glm::vec2& max,
		const UICanvasMetrics& metrics, const glm::vec4& tint,
		const glm::vec2& clipMin, const glm::vec2& clipMax)
	{
		// Never loads, like images: whoever assigned the handle loaded the font.
		const Ref<Font> font = AssetManager::GetLoadedAsset<Font>(text.Font);
		if (font == nullptr || text.Text.empty() || text.VisibleCharacters == 0)
		{
			return;
		}

		TextLayoutProperties settings;
		settings.Scale = text.Scale;
		settings.BoxWidth = max.x - min.x;
		settings.Wrap = text.Wrap;
		settings.AlignH = text.AlignH;
		settings.LineSpacing = text.LineSpacing;
		TextLayout::Layout(text.Text, font->GetData(), settings, m_TextLayout, font.get());

		// The text's top edge in canvas pixels (Y up), from its vertical alignment in the rect; whole pixels.
		float top = max.y;
		if (text.AlignV == TextAlignV::Middle)
		{
			top = max.y - std::floor((max.y - min.y - m_TextLayout.Size.y) * 0.5f);
		}
		else if (text.AlignV == TextAlignV::Bottom)
		{
			top = min.y + m_TextLayout.Size.y;
		}

		// The shadow first (when it has any alpha), then the glyphs over it.
		const bool shadow = text.ShadowColor.a > 0.0f;
		for (int pass = shadow ? 0 : 1; pass < 2; pass++)
		{
			const glm::vec2 offset = pass == 0 ? text.ShadowOffset : glm::vec2(0.0f);
			const glm::vec4 color = pass == 0 ? glm::vec4(glm::vec3(text.ShadowColor), text.ShadowColor.a * text.Color.a) * glm::vec4(1.0f, 1.0f, 1.0f, tint.a)
				: text.Color * tint;
			for (const TextGlyphQuad& glyph : m_TextLayout.Glyphs)
			{
				// The typewriter: later characters aren't drawn, but everything stays where the full text puts it.
				if (text.VisibleCharacters >= 0 && glyph.CharacterIndex >= (uint32_t)text.VisibleCharacters)
				{
					continue;
				}
				const Ref<Texture2D>& page = font->GetPage(glyph.Page);
				if (page == nullptr)
				{
					continue;
				}

				// Layout Y runs down from the text's top; the canvas's runs up.
				const glm::vec2 viewMin = CanvasToViewPixels(metrics, glm::vec2(min.x + glyph.Min.x, top - glyph.Max.y) + offset);
				const glm::vec2 viewMax = CanvasToViewPixels(metrics, glm::vec2(min.x + glyph.Max.x, top - glyph.Min.y) + offset);
				if (viewMax.x <= viewMin.x || viewMax.y <= viewMin.y)
				{
					continue;
				}
				const glm::vec2 pageSize = glm::max(glm::vec2((float)page->GetWidth(), (float)page->GetHeight()), glm::vec2(1.0f));
				TextureRect rect;
				rect.Min = glyph.SourceMin / pageSize;
				rect.Max = glyph.SourceMax / pageSize;
				DrawClippedQuad(viewMin, viewMax, page, rect, color, clipMin, clipMax);
			}
		}
	}
}
