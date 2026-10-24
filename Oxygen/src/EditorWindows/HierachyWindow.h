
#pragma once
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"

#include <functional>
#include <string>

namespace Hydro
{
	class HierachyWindow
	{
	public:
		HierachyWindow();
		~HierachyWindow();
		void OnImguiAttach();
		void OnImguiRender(Ref<Scene> scene);

		Entity GetSelectedEntity() const { return m_SelectedEntity; }
		void ClearSelection() { m_SelectedEntity = {}; }
		void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

		// Called with the entity of a row that was double-clicked (it is selected too). The editor uses
		// it to move the scene camera to the entity.
		std::function<void(Entity)> OnEntityFocus;

	private:
		void SaveAsPrefab(Entity entity);
		// True when the entity's name contains the search text (case-insensitive); an empty search matches all.
		bool MatchesSearch(const std::string& name) const;

	private:
		Entity m_SelectedEntity;
		char m_SearchBuffer[128] = {};
	};
}
