#pragma once
#include "Hydro/Core/UUID.h"
#include "Components/Components.h"
#include "UI/UILayout.h"

#include <entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Hydro
{
	class Scene;
	class SceneGraph;
	class SceneTransforms;

	// A scene's runtime UI layout: every UIResolvedRectComponent in canvas pixels, parents first, one pass over the list.
	// Scene owns one and forwards ResolveUILayout and GetUICanvasMetrics here (docs/SceneAndECS.md#runtime-ui).
	class SceneUILayout
	{
	public:
		// Connects the signals that give every UIRectComponent its UIResolvedRectComponent, so a pass only writes values.
		SceneUILayout(Scene& scene, SceneGraph& graph, SceneTransforms& transforms);

		void Resolve();
		// Where the last pass put a top-level canvas in the view. False for one it didn't place (hidden, nested, a World
		// canvas off screen or without a camera).
		bool GetCanvasMetrics(UUID canvas, UICanvasMetrics& metrics) const;

	private:
		// Places the visible UI children of layout group entry `index` and, with FitContent, resizes the group (min, max).
		void ArrangeChildren(size_t index, const UIRectComponent& rect, const UILayoutGroupComponent& group, glm::vec2& min,
			glm::vec2& max);
		// Moves, clips and sizes the scrollbar of scroll view entry `index`.
		void ScrollView(size_t index);
		// False when canvas entry `index` isn't shown (a World canvas behind the camera).
		bool PlaceCanvas(size_t index, const UICanvasComponent& canvas, const glm::vec2& viewSize, UICanvasMetrics& metrics);

	private:
		// The rect a list entry's UI children resolve against.
		struct Entry
		{
			glm::vec2 Min{ 0.0f, 0.0f };
			glm::vec2 Max{ 0.0f, 0.0f };
			bool Visible = false;
			bool InCanvas = false; // below a canvas (even an ignored, nested one)
			UUID Canvas = 0;       // the canvas its UI children are drawn on (0 = none)
		};
		// The rect a layout group placed an entry at.
		struct DrivenRect
		{
			glm::vec2 Min{ 0.0f, 0.0f };
			glm::vec2 Max{ 0.0f, 0.0f };
			bool Driven = false;
		};

		Scene& m_Scene;
		SceneGraph& m_Graph;
		SceneTransforms& m_Transforms;
		// Kept between passes so a pass allocates nothing once warm.
		std::vector<Entry> m_Entries;
		std::vector<DrivenRect> m_Driven;
		std::vector<size_t> m_ChildIndices;
		std::vector<UILayoutItem> m_Items;
		std::vector<size_t> m_ItemIndices;
		// Placed canvases by UUID, and the primary camera's world-to-view-pixels matrix for World canvases (once per pass).
		std::unordered_map<uint64_t, UICanvasMetrics> m_CanvasMetrics;
		glm::mat4 m_WorldToView{ 1.0f };
		int m_WorldToViewState = 0; // 0 not worked out this pass, 1 ready, -1 no camera
		// Already warned about, so the log isn't flooded.
		std::unordered_set<entt::entity> m_NestedCanvasWarned;
	};
}
