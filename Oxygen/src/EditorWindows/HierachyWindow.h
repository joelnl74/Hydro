
#pragma once
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"

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

	private:
		void SaveAsPrefab(Entity entity);

	private:
		Entity m_SelectedEntity;
	};
}
