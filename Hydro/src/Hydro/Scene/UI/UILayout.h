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
		glm::vec2 Offset{ 0.0f, 0.0f };  // view pixels from the view's bottom-left corner to the canvas's (negative: cropped)
	};

	// One piece of a nine-slice image: a part of the source texture stretched over a part of the element's rect.
	struct UINineSliceCell
	{
		glm::vec2 DestinationMin{ 0.0f, 0.0f };   // canvas pixels, Y up
		glm::vec2 DestinationMax{ 0.0f, 0.0f };
		// Texture pixels, Y down (row 0 is the image's top row), as a sprite's region. A mirrored cell (FlipX / FlipY)
		// has its Min and Max swapped on that axis.
		glm::vec2 SourceMin{ 0.0f, 0.0f };
		glm::vec2 SourceMax{ 0.0f, 0.0f };
	};

	// One child of a layout group: its size in (canvas pixels, as its own rect resolves), its place out.
	struct UILayoutItem
	{
		glm::vec2 Size{ 0.0f, 0.0f };
		glm::vec2 Min{ 0.0f, 0.0f };
		glm::vec2 Max{ 0.0f, 0.0f };
	};

	// The maths of the runtime UI's layout (UI1): pure functions over plain values, used by Scene::ResolveUILayout
	// (the layout pass) and SceneRenderer (drawing). Nothing here touches a scene or the renderer.
	class UILayout
	{
	public:
		// The canvas in a view of viewSize pixels (docs/SceneAndECS.md#runtime-ui):
		//  - IntegerScale: the largest whole scale at which ReferenceResolution fits, never below 1; the canvas takes
		//    the whole view at that scale (so it's wider or taller than the reference, never smaller), and in a view
		//    smaller than the reference it stays at the reference size, centred and cropped.
		//  - ScaleWithScreen: the fractional scale at which the reference fits; one axis matches it, the other is wider.
		//  - ConstantPixelSize: scale 1, the canvas is the view.
		// The canvas is centred in the view (whole view pixels).
		static UICanvasMetrics GetCanvasMetrics(const UICanvasComponent& canvas, const glm::vec2& viewSize);

		// The element's rect in its parent's (parentMin..parentMax, canvas pixels), as Unity's RectTransform: the anchors
		// give a span in the parent, SizeDelta grows it, the pivot sits AnchoredPosition away from the anchors' point.
		// Snapped to whole canvas pixels, so pixel art never lands between pixels; never inside out (Max >= Min).
		static void ResolveRect(const UIRectComponent& rect, const glm::vec2& parentMin, const glm::vec2& parentMax, glm::vec2& outMin, glm::vec2& outMax);

		// The cells that draw a nine-slice image over destinationMin..Max (canvas pixels, Y up) from the source region
		// sourcePosition / sourceSize (texture pixels, Y down: a sprite's region). borders = left, bottom, right, top
		// in texture pixels, drawn 1:1. Borders larger than the source are cut to fit it; when the destination is
		// smaller than two opposite borders together, both shrink in proportion and the middle has no size, so the
		// corners never overlap or turn inside out. Cells without area are left out. Writes up to 9 cells, returns how many.
		static uint32_t BuildNineSlice(const glm::vec2& destinationMin, const glm::vec2& destinationMax,
			const glm::vec2& sourcePosition, const glm::vec2& sourceSize, const glm::vec4& borders, bool flipX, bool flipY,
			UINineSliceCell (&cells)[9]);

		// A layout group's children placed in order inside groupMin..groupMax less its padding (UILayoutGroupComponent):
		// stacked along the group's direction with its spacing, the stack placed by ChildAlignment, each child as long as
		// its Size along it and, across it, stretched to the content or its own size placed by ChildAlignment. With
		// FitContent the group's own rect first takes the stack's length along its direction, its pivot (0..1 of the rect)
		// staying where it was. Whole canvas pixels.
		static void ArrangeGroup(const UILayoutGroupComponent& group, const glm::vec2& pivot, glm::vec2& groupMin, glm::vec2& groupMax, std::vector<UILayoutItem>& items);

		// True when the image is drawn as nine slices (any border above 0).
		static bool IsNineSliced(const UIImageComponent& image)
		{
			return image.NineSlice.x > 0.0f || image.NineSlice.y > 0.0f || image.NineSlice.z > 0.0f || image.NineSlice.w > 0.0f;
		}
	};
}
