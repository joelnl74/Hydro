
#pragma once
#include "Hydro/Scene/Scene.h"
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
	};
}
