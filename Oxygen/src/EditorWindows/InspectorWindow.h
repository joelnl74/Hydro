#pragma once
#include "Hydro/Scene/Entity.h"

namespace Hydro
{
	// Shows and edits the components of the entity selected in the hierarchy.
	// Editor UI only: it reads and writes component data, it holds no behaviour of its own.
	class InspectorWindow
	{
	public:
		InspectorWindow() = default;
		~InspectorWindow() = default;

		void OnImguiRender(Entity entity);

	private:
		void DrawEntityHeader(Entity entity);
		void DrawComponents(Entity entity);
		void DrawAddComponentButton(Entity entity);
		void DrawPrefabButton(Entity entity);
	};
}
