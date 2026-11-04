#pragma once

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/UI/UILayout.h"
#include "Hydro/Renderer/TextLayout.h"

#include <entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class Scene;

	// The runtime UI for SceneRenderer (#164): every visible canvas in SortOrder, in screen space after the tonemap,
	// in whole view pixels. Runtime only: a screen-space canvas has no place in the Editor view.
	class SceneUIRenderer
	{
	public:
		void Submit(Scene& scene);

	private:
		// One image over a resolved rect (canvas pixels), whole or nine-sliced, cut to clipMin..clipMax (view pixels).
		static void SubmitImage(const UIImageComponent& image, const glm::vec2& min, const glm::vec2& max, const UICanvasMetrics& metrics,
			const glm::vec4& tint, const glm::vec2& clipMin, const glm::vec2& clipMax);
		// One text laid out in its rect (wrapped, vertically aligned, up to VisibleCharacters). Nothing without a font.
		void SubmitText(const UITextComponent& text, const glm::vec2& min, const glm::vec2& max, const UICanvasMetrics& metrics,
			const glm::vec4& tint, const glm::vec2& clipMin, const glm::vec2& clipMax);

		// This frame's canvases (SortOrder, first place in the hierarchy, entity), reused.
		struct CanvasItem
		{
			int32_t SortOrder = 0;
			uint32_t Index = 0;
			entt::entity Entity = entt::null;
		};
		// The buttons above the element being drawn, by depth (a stack): their tint (multiplied down) and whether the
		// nearest is focused (UIFocusVisibilityComponent).
		struct ButtonFrame
		{
			uint32_t Depth = 0;
			glm::vec4 Tint{ 1.0f };
			bool Focused = false;
		};

		std::vector<CanvasItem> m_Canvases;
		std::vector<Entity> m_Subtree;    // the canvas being drawn and its descendants, reused
		std::vector<ButtonFrame> m_Buttons; // reused
		TextLayoutResult m_TextLayout;    // SubmitText's, reused
	};
}
