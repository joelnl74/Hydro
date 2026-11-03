#pragma once

#include "Hydro/Scene/Entity.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Hydro
{
	class Scene;

	// Lines and text a script draws in the Editor view without pressing Play (ScriptableEntity::OnDrawGizmos), in
	// world space over the scene. Collected each frame by CollectScriptGizmos; Oxygen draws them.
	class Gizmos
	{
	public:
		struct LineItem
		{
			glm::vec3 Start{ 0.0f };
			glm::vec3 End{ 0.0f };
			glm::vec4 Color{ 1.0f };
			float Thickness = 2.0f; // pixels
		};

		struct LabelItem
		{
			glm::vec3 Position{ 0.0f };
			std::string Text;
			glm::vec4 Color{ 1.0f };
		};

		// The colours gizmos use by convention: dim when their entity isn't selected, bright when it is.
		static constexpr glm::vec4 Normal{ 0.45f, 0.85f, 1.0f, 0.55f };
		static constexpr glm::vec4 Highlight{ 1.0f, 0.8f, 0.25f, 1.0f };

		void Line(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color = Normal, float thickness = 2.0f);
		// A line with a head at `end`, the head flat on the ground (XY).
		void Arrow(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color = Normal, float thickness = 2.0f);
		// A circle flat on the ground (XY) around `centre`.
		void WireCircle(const glm::vec3& centre, float radius, const glm::vec4& color = Normal, int segments = 32);
		// A small cross on the ground at `position`, `size` metres across.
		void Cross(const glm::vec3& position, float size = 0.3f, const glm::vec4& color = Normal);
		// Text centred on `position` (projected onto the Editor view), with a dark outline.
		void Label(const glm::vec3& position, const std::string& text, const glm::vec4& color = glm::vec4(1.0f));

		// The script's own entity is the selected one.
		bool IsSelected() const { return m_Selected; }
		// That entity is the selected one (a route's waypoint, say).
		bool IsSelected(const Entity& entity) const { return (bool)m_SelectedEntity && entity == m_SelectedEntity; }

		const std::vector<LineItem>& GetLines() const { return m_Lines; }
		const std::vector<LabelItem>& GetLabels() const { return m_Labels; }
		bool IsEmpty() const { return m_Lines.empty() && m_Labels.empty(); }
		void Clear();

		// CollectScriptGizmos: whose gizmos come next, and what the editor has selected.
		void Begin(const Entity& owner, const Entity& selected);

	private:
		std::vector<LineItem> m_Lines;
		std::vector<LabelItem> m_Labels;
		Entity m_SelectedEntity;
		bool m_Selected = false;
	};

	// Every script in `scene` whose class overrides OnDrawGizmos draws into `gizmos`. While editing each is made for
	// the call with its saved properties (no OnCreate) and destroyed after; while playing, the running instance draws.
	void CollectScriptGizmos(Scene& scene, const Entity& selected, Gizmos& gizmos);
}
