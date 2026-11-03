#pragma once
#include "Hydro/Scene/Components/Components.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// How one canvas sits in a view: its size in canvas pixels, its scale and where its bottom-left corner lands.
	struct UICanvasMetrics
	{
		float Scale = 1.0f;              // view pixels per canvas pixel
		glm::vec2 Size{ 0.0f, 0.0f };    // the canvas in canvas pixels: the rect its direct children resolve against
		glm::vec2 Offset{ 0.0f, 0.0f };  // view pixels from the view's bottom-left to the canvas's (negative: cropped)
	};

	// One piece of a nine-slice image: a part of the source texture stretched over a part of the element's rect.
	struct UINineSliceCell
	{
		glm::vec2 DestinationMin{ 0.0f, 0.0f };   // canvas pixels, Y up
		glm::vec2 DestinationMax{ 0.0f, 0.0f };
		// Texture pixels, Y down (row 0 is the top), as a sprite's region. A mirrored cell has Min and Max swapped.
		glm::vec2 SourceMin{ 0.0f, 0.0f };
		glm::vec2 SourceMax{ 0.0f, 0.0f };
	};

	// One child of a layout group: its size in (canvas pixels), its place out.
	struct UILayoutItem
	{
		glm::vec2 Size{ 0.0f, 0.0f };
		glm::vec2 Min{ 0.0f, 0.0f };
		glm::vec2 Max{ 0.0f, 0.0f };
	};

	// The runtime UI's layout maths: pure functions over plain values, used by Scene::ResolveUILayout and SceneRenderer.
	class UILayout
	{
	public:
		// The canvas in a view of viewSize pixels, centred, for each UIScaleMode (docs/SceneAndECS.md#runtime-ui).
		static UICanvasMetrics GetCanvasMetrics(const UICanvasComponent& canvas, const glm::vec2& viewSize);

		// The element's rect in its parent's, as Unity's RectTransform, snapped to whole canvas pixels so pixel art never
		// lands between pixels; never inside out.
		static void ResolveRect(const UIRectComponent& rect, const glm::vec2& parentMin, const glm::vec2& parentMax, glm::vec2& outMin,
			glm::vec2& outMax);

		// The cells of a nine-slice image over the destination (canvas pixels, Y up) from a sprite region (texture pixels,
		// Y down). Borders (l, b, r, t) draw 1:1 and shrink to fit, so corners never overlap. Returns the cell count (<= 9).
		static uint32_t BuildNineSlice(const glm::vec2& destinationMin, const glm::vec2& destinationMax,
			const glm::vec2& sourcePosition, const glm::vec2& sourceSize, const glm::vec4& borders, bool flipX, bool flipY,
			UINineSliceCell (&cells)[9]);

		// Places a layout group's children inside its rect less padding, along its direction, in whole canvas pixels.
		// With FitContent the group's rect first takes the stack's length, its pivot staying put.
		static void ArrangeGroup(const UILayoutGroupComponent& group, const glm::vec2& pivot, glm::vec2& groupMin, glm::vec2& groupMax,
			std::vector<UILayoutItem>& items);

		// True when the image is drawn as nine slices (any border above 0).
		static bool IsNineSliced(const UIImageComponent& image)
		{
			return image.NineSlice.x > 0.0f || image.NineSlice.y > 0.0f || image.NineSlice.z > 0.0f || image.NineSlice.w > 0.0f;
		}
	};
}
