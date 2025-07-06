
#pragma once
#include "Hydro.h"

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
